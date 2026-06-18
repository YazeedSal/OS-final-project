/*
 * scheduler.c — Milestone 7: FCFS scheduling (the core of the mission).
 *
 * Two parts live here:
 *   1. The FCFS policy itself — one waiting queue per node. Only one traveler
 *      may be inside a node at a time. FCFS = join the queue at the back,
 *      get let in from the front.
 *   2. The simulation that uses it — the parent forks one child per traveler,
 *      every child asks before entering a node and BLOCKS until the parent
 *      wakes it, and the parent grants access in FCFS order.
 *
 * The parent records a timeline of events (wait / enter / leave / finish) so
 * the GUI in gui_m4.c can replay the run afterwards.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>

#include "../include/scheduler.h"
#include "../include/dijkstra.h"

#define HOP_DELAY_US 300000   /* 300 ms per weight unit (matches the GUI) */
#define MAX_EVENTS   (MAX_TRAVELERS * (MAX_NODES * 3 + 2))

/* ═══════════════════════════════════════════════════════════════════
   PART 1 — the FCFS queues
   ═══════════════════════════════════════════════════════════════════ */
static int occupant[MAX_NODES];                 /* traveler inside, or -1    */
static int queue[MAX_NODES][MAX_TRAVELERS];     /* FIFO of waiting travelers */
static int qlen[MAX_NODES];                     /* how many are waiting      */

static void sched_init(int num_nodes)
{
    for (int i = 0; i < num_nodes; i++) {
        occupant[i] = -1;
        qlen[i]     = 0;
    }
}

/* First-Come: a newly-waiting traveler joins the BACK of the line. */
static void sched_enqueue(int node, int traveler)
{
    if (qlen[node] < MAX_TRAVELERS)
        queue[node][qlen[node]++] = traveler;
}

/* First-Served: if the node is free, admit whoever is at the FRONT. */
static int sched_try_admit(int node)
{
    if (occupant[node] != -1) return -1;   /* node is busy   */
    if (qlen[node] == 0)      return -1;   /* nobody waiting */

    int t = queue[node][0];                /* head of the line      */
    for (int i = 1; i < qlen[node]; i++)   /* shift everyone up one */
        queue[node][i - 1] = queue[node][i];
    qlen[node]--;

    occupant[node] = t;
    return t;
}

static void sched_release(int node)
{
    occupant[node] = -1;
}

/* ═══════════════════════════════════════════════════════════════════
   PART 2 — small helpers
   ═══════════════════════════════════════════════════════════════════ */

/* weight of edge u -> v, or 1 if there is no such edge */
static int edge_w(const Graph* g, int u, int v)
{
    for (AdjNode* c = g->adjLists[u]; c; c = c->next)
        if (c->destination == v) return c->weight;
    return 1;
}

/* seconds elapsed since 'start' */
static float secs_since(struct timespec start)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (float)(now.tv_sec - start.tv_sec)
         + (float)(now.tv_nsec - start.tv_nsec) / 1e9f;
}

/* ═══════════════════════════════════════════════════════════════════
   PART 3 — the child process: walk my path, ask before every node
   ═══════════════════════════════════════════════════════════════════ */
static void child_run(int idx, int src, int dst, Graph* g,
                      int req_wfd, int grant_rfd)
{
    g->source = src;
    g->destination = dst;

    int path[MAX_NODES], len = 0;
    dijkstra_path(g, path, &len);

    pid_t me = getpid();

    for (int i = 0; i < len; i++) {
        int node = path[i];
        int next = (i + 1 < len) ? path[i + 1] : -1;

        /* ask to enter this node, then block until the parent wakes me */
        ReqMsg req = { idx, me, REQ_ENTER, node, next };
        write(req_wfd, &req, sizeof(req));

        int grant;
        if (read(grant_rfd, &grant, sizeof(grant)) <= 0) break;

        sleep(1);                  /* critical section: 1 full second inside */

        ReqMsg leave = { idx, me, REQ_LEAVE, node, next };
        write(req_wfd, &leave, sizeof(leave));

        if (next != -1)            /* travel along the edge to the next node */
            usleep((unsigned)(edge_w(g, node, next) * HOP_DELAY_US));
    }

    ReqMsg fin = { idx, me, REQ_FINISH, -1, -1 };
    write(req_wfd, &fin, sizeof(fin));

    close(req_wfd);
    close(grant_rfd);
    _exit(0);   /* _exit (not exit) so we don't flush the parent's stdio buffer */
}

/* admit the head of a node's queue (if any) and wake that child */
static void admit_and_wake(int node, int grant_wfd[], int next_of[],
                           SimEvent* ev, int* n, struct timespec start)
{
    int t = sched_try_admit(node);
    if (t == -1) return;

    printf("[FCFS] T%d entered node %d\n", t, node);
    ev[(*n)++] = (SimEvent){ secs_since(start), t, EV_ENTER, node, next_of[t] };

    int go = 1;
    write(grant_wfd[t], &go, sizeof(go));   /* wake the waiting child */
}

/* ═══════════════════════════════════════════════════════════════════
   PART 4 — the parent: run the FCFS scheduler, return the timeline
   ═══════════════════════════════════════════════════════════════════ */
SimEvent* run_scheduled_sim(Graph* g, Traveler* tr, int K, int* outN)
{
    int   req_pipe[2];
    int   grant[MAX_TRAVELERS][2];
    int   grant_wfd[MAX_TRAVELERS];
    pid_t pid[MAX_TRAVELERS];

    if (pipe(req_pipe) == -1) { perror("pipe"); exit(1); }
    for (int i = 0; i < K; i++)
        if (pipe(grant[i]) == -1) { perror("pipe"); exit(1); }

    fflush(stdout);   /* empty the buffer so children don't inherit & re-print it */

    for (int i = 0; i < K; i++) {
        pid[i] = fork();
        if (pid[i] < 0) { perror("fork"); exit(1); }

        if (pid[i] == 0) {
            /* child keeps only: req write end + its own grant read end */
            close(req_pipe[0]);
            for (int j = 0; j < K; j++) {
                close(grant[j][1]);
                if (j != i) close(grant[j][0]);
            }
            child_run(i, tr[i].source, tr[i].destination, g,
                      req_pipe[1], grant[i][0]);
        }
    }

    /* parent keeps: req read end + every grant write end */
    close(req_pipe[1]);
    for (int i = 0; i < K; i++) {
        close(grant[i][0]);
        grant_wfd[i] = grant[i][1];
    }

    SimEvent* ev = malloc(sizeof(SimEvent) * MAX_EVENTS);
    int n = 0;

    int next_of[MAX_TRAVELERS];           /* 'next' from each child's ENTER */
    for (int i = 0; i < K; i++) next_of[i] = -1;

    sched_init(g->numNodes);

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int    active = K;
    ReqMsg msg;
    while (active > 0 &&
           read(req_pipe[0], &msg, sizeof(msg)) == (ssize_t)sizeof(msg)) {

        float t = secs_since(start);

        if (msg.type == REQ_ENTER) {
            next_of[msg.traveler] = msg.next;
            printf("[FCFS] T%d (pid=%d) waiting for node %d\n",
                   msg.traveler, (int)msg.pid, msg.node);
            ev[n++] = (SimEvent){ t, msg.traveler, EV_WAIT, msg.node, msg.next };

            sched_enqueue(msg.node, msg.traveler);
            admit_and_wake(msg.node, grant_wfd, next_of, ev, &n, start);
        }
        else if (msg.type == REQ_LEAVE) {
            printf("[FCFS] T%d left node %d\n", msg.traveler, msg.node);
            ev[n++] = (SimEvent){ t, msg.traveler, EV_LEAVE, msg.node, msg.next };

            sched_release(msg.node);
            admit_and_wake(msg.node, grant_wfd, next_of, ev, &n, start);
        }
        else { /* REQ_FINISH */
            printf("[FCFS] T%d finished\n", msg.traveler);
            ev[n++] = (SimEvent){ t, msg.traveler, EV_FINISH, -1, -1 };
            active--;
        }

        if (n > MAX_EVENTS - 4) break;    /* safety guard */
    }

    close(req_pipe[0]);
    for (int i = 0; i < K; i++) {
        close(grant_wfd[i]);
        waitpid(pid[i], NULL, 0);
    }

    *outN = n;
    return ev;
}
