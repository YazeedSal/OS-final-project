/*
 * ipc.c - Milestone 5: Inter-Process Communication
 *
 * IPC mechanism chosen: unnamed pipes (one pipe per child traveler).
 * Reason: simple, no extra setup, and fits the one-producer/one-consumer
 * pattern perfectly — each child writes, the parent reads.
 *
 * Design:
 *   - Parent forks one child per traveler.
 *   - Each child computes its own Dijkstra path and sends a TravelMessage
 *     to the parent for every node it visits.
 *   - Parent uses select() to read from all pipes at the same time and
 *     prints the log. All printing is done by the parent only.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>

#include "../include/ipc.h"
#include "../include/graph.h"
#include "../include/dijkstra.h"

#define MAX_TRAVELERS  32
#define MAX_PATH       64
#define STEP_DELAY_US  400000   /* 0.4 s between hops so output interleaves */

/* ── Skip '#' comment lines and leading whitespace before the next token ── */
static void skip_comments(FILE *f)
{
    int c;
    while (1) {
        while ((c = fgetc(f)) != EOF &&
               (c == ' ' || c == '\t' || c == '\n' || c == '\r'))
            ;
        if (c == EOF) return;
        if (c == '#') {
            while ((c = fgetc(f)) != EOF && c != '\n')
                ;
        } else {
            ungetc(c, f);
            return;
        }
    }
}

/*
 * Read the graph (nodes + edges) and the traveler list from a file.
 *
 * Expected format (lines starting with '#' are ignored):
 *   N M
 *   src dst weight    (M times)
 *   K
 *   src dst           (K times)
 */
static int read_input(const char *filename, Graph *graph,
                      int sources[], int dests[], int *num_travelers)
{
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("fopen");
        return 0;
    }

    /* graph header */
    skip_comments(f);
    if (fscanf(f, "%d %d", &graph->numNodes, &graph->numEdges) != 2) {
        fprintf(stderr, "Error: expected numNodes numEdges\n");
        fclose(f);
        return 0;
    }

    graph->adjLists   = calloc(graph->numNodes, sizeof(AdjNode *));
    graph->edges      = malloc(graph->numEdges * sizeof(Edge));
    graph->source     = 0;
    graph->destination = 0;

    if (!graph->adjLists || !graph->edges) {
        fprintf(stderr, "Error: memory allocation failed\n");
        fclose(f);
        return 0;
    }

    /* edges */
    for (int i = 0; i < graph->numEdges; i++) {
        int src, dst, w;
        skip_comments(f);
        if (fscanf(f, "%d %d %d", &src, &dst, &w) != 3) {
            fprintf(stderr, "Error: failed to read edge %d\n", i);
            fclose(f);
            return 0;
        }

        AdjNode *node   = malloc(sizeof(AdjNode));
        node->destination = dst;
        node->weight      = w;
        node->next        = NULL;

        if (graph->adjLists[src] == NULL) {
            graph->adjLists[src] = node;
        } else {
            AdjNode *cur = graph->adjLists[src];
            while (cur->next) cur = cur->next;
            cur->next = node;
        }

        graph->edges[i].source      = src;
        graph->edges[i].destination = dst;
        graph->edges[i].weight      = w;
    }

    /* traveler list */
    skip_comments(f);
    if (fscanf(f, "%d", num_travelers) != 1) {
        fprintf(stderr, "Error: expected number of travelers\n");
        fclose(f);
        return 0;
    }

    for (int i = 0; i < *num_travelers; i++) {
        skip_comments(f);
        if (fscanf(f, "%d %d", &sources[i], &dests[i]) != 2) {
            fprintf(stderr, "Error: failed to read traveler %d\n", i);
            fclose(f);
            return 0;
        }
    }

    fclose(f);
    return 1;
}

/*
 * Child process: compute the shortest path independently, then "travel"
 * through each node — sending one TravelMessage per hop into write_fd.
 */
static void child_travel(Graph *graph, int src, int dst, int write_fd)
{
    graph->source      = src;
    graph->destination = dst;

    int path[MAX_PATH];
    int path_len = 0;

    int cost = dijkstra_path(graph, path, &path_len);
    if (cost == -1 || path_len == 0) {
        close(write_fd);
        exit(0);
    }

    pid_t my_pid = getpid();
    TravelMessage msg;

    for (int i = 0; i < path_len; i++) {
        msg.pid          = my_pid;
        msg.current_node = path[i];
        msg.next_node    = (i + 1 < path_len) ? path[i + 1] : -1;
        msg.finished     = 0;

        if (write(write_fd, &msg, sizeof(msg)) == -1) {
            perror("write");
            break;
        }

        usleep(STEP_DELAY_US);
    }

    /* signal the parent that this traveler is finished */
    msg.pid          = my_pid;
    msg.current_node = -1;
    msg.next_node    = -1;
    msg.finished     = 1;
    write(write_fd, &msg, sizeof(msg));

    close(write_fd);
    exit(0);
}

/*
 * Parent process: read TravelMessages from all child pipes simultaneously
 * using select() and print a log line for every event.
 */
static void parent_log_loop(int read_fds[], int num_travelers)
{
    int active = num_travelers;
    int closed[MAX_TRAVELERS];
    memset(closed, 0, sizeof(closed));

    while (active > 0) {
        fd_set readfds;
        FD_ZERO(&readfds);
        int maxfd = 0;

        for (int i = 0; i < num_travelers; i++) {
            if (!closed[i]) {
                FD_SET(read_fds[i], &readfds);
                if (read_fds[i] > maxfd) maxfd = read_fds[i];
            }
        }

        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        for (int i = 0; i < num_travelers; i++) {
            if (closed[i] || !FD_ISSET(read_fds[i], &readfds)) continue;

            TravelMessage msg;
            ssize_t n = read(read_fds[i], &msg, sizeof(msg));

            if (n <= 0) {
                close(read_fds[i]);
                closed[i] = 1;
                active--;
                continue;
            }

            if (msg.finished) {
                printf("[PID=%d] finished\n", (int)msg.pid);
                fflush(stdout);
                close(read_fds[i]);
                closed[i] = 1;
                active--;
            } else if (msg.next_node == -1) {
                printf("[PID=%d] arrived at node %d | DESTINATION\n",
                       (int)msg.pid, msg.current_node);
                fflush(stdout);
            } else {
                printf("[PID=%d] arrived at node %d | next node: %d\n",
                       (int)msg.pid, msg.current_node, msg.next_node);
                fflush(stdout);
            }
        }
    }
}

/* ── Public entry point called from main ── */
void run_m5(const char *filename)
{
    Graph graph;
    graph.adjLists = NULL;
    graph.edges    = NULL;

    int sources[MAX_TRAVELERS], dests[MAX_TRAVELERS];
    int num_travelers = 0;

    if (!read_input(filename, &graph, sources, dests, &num_travelers)) {
        return;
    }

    if (num_travelers <= 0) {
        fprintf(stderr, "No travelers found in input file.\n");
        return;
    }

    /* create one pipe per traveler */
    int pipes[MAX_TRAVELERS][2];
    pid_t pids[MAX_TRAVELERS];
    int read_fds[MAX_TRAVELERS];

    for (int i = 0; i < num_travelers; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            return;
        }
    }

    /* fork a child for each traveler */
    for (int i = 0; i < num_travelers; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return;
        }

        if (pid == 0) {
            /* child: close all read ends and every sibling's write end */
            for (int j = 0; j < num_travelers; j++) {
                close(pipes[j][0]);
                if (j != i) close(pipes[j][1]);
            }
            child_travel(&graph, sources[i], dests[i], pipes[i][1]);
            /* child_travel always calls exit() */
        }

        pids[i]    = pid;
        read_fds[i] = pipes[i][0];
        close(pipes[i][1]);   /* parent does not write */
    }

    /* parent reads from all pipes and prints the log */
    parent_log_loop(read_fds, num_travelers);

    /* wait for all children to exit */
    for (int i = 0; i < num_travelers; i++) {
        waitpid(pids[i], NULL, 0);
    }

    freeGraph(&graph);
}
