#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <sys/types.h>
#include "graph.h"        /* Graph, MAX_NODES, MAX_TRAVELERS */
#include "travelers.h"    /* Traveler                        */

/*
 * Milestone 7 — FCFS and SJF scheduling of node access.
 *
 * When several travelers want to enter the same node, the parent decides
 * the order.
 *
 *   FCFS (First Come, First Served):
 *     - a traveler that starts waiting joins the BACK of the node's queue
 *     - when the node becomes free, the traveler at the FRONT is let in
 *
 *   SJF (Shortest Job First):
 *     - a traveler that starts waiting joins the queue
 *     - when the node becomes free, the traveler with the shortest total
 *       path cost (computed by Dijkstra before the simulation) is let in
 *
 * The only difference is which element sched_try_admit() picks from the
 * queue — nothing else moves.
 */

typedef enum { SCHED_FCFS, SCHED_SJF, SCHED_LPID } SchedAlgo;

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
 * Run the scheduled simulation with the chosen algorithm:
 *   - forks one child process per traveler (each computes its own path),
 *   - schedules node access using FCFS or SJF,
 *   - prints a log to the terminal,
 *   - returns a malloc'd array of recorded events (caller frees it) and
 *     writes the number of events into *outN.
 */
SimEvent* run_scheduled_sim(Graph* g, Traveler* travelers, int K, int* outN,
                            SchedAlgo algo);

#endif
