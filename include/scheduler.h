#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <sys/types.h>
#include "graph.h"        /* Graph, MAX_NODES, MAX_TRAVELERS */
#include "travelers.h"    /* Traveler                        */

/*
 * Milestone 7 — FCFS scheduling of node access.
 *
 * When several travelers want to enter the same node, the parent decides
 * the order. The whole policy is "First Come, First Served":
 *   - a traveler that starts waiting joins the BACK of the node's queue
 *   - when the node becomes free, the traveler at the FRONT is let in
 *
 * To add a second algorithm later (e.g. SJF) you only change which element
 * sched_try_admit() picks from the queue — nothing else moves.
 *
 * This file is the heart of the milestone: scheduler.c holds both the FCFS
 * queue and the whole process/IPC simulation. main_m7.c only feeds it a
 * graph + travelers and shows the result.
 */

/* ───── message a child sends to the parent through the request pipe ───── */
enum { REQ_ENTER, REQ_LEAVE, REQ_FINISH };

typedef struct {
    int   traveler;   /* traveler index 0..K-1                            */
    pid_t pid;        /* child PID (used only for the log)                */
    int   type;       /* REQ_ENTER / REQ_LEAVE / REQ_FINISH              */
    int   node;       /* node this request is about                       */
    int   next;       /* next node on the child's path (-1 = none)        */
} ReqMsg;

/* ───── event the parent records so the GUI can replay the run later ───── */
typedef enum { EV_WAIT, EV_ENTER, EV_LEAVE, EV_FINISH } EventType;

typedef struct {
    float     t;         /* seconds since the simulation started          */
    int       traveler;  /* traveler index 0..K-1                         */
    EventType type;
    int       node;      /* node concerned (-1 for EV_FINISH)             */
    int       next;      /* next node (-1 if none)                        */
} SimEvent;

/*
 * Run the whole FCFS-scheduled simulation:
 *   - forks one child process per traveler (each computes its own path),
 *   - schedules node access First-Come-First-Served,
 *   - prints an FCFS log to the terminal,
 *   - returns a malloc'd array of recorded events (caller frees it) and
 *     writes the number of events into *outN.
 */
SimEvent* run_scheduled_sim(Graph* g, Traveler* travelers, int K, int* outN);

#endif
