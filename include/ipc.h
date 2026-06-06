#ifndef IPC_H
#define IPC_H

#include <sys/types.h>

/*
 * Message a child sends to the parent through its pipe.
 * One message per node visited during the trip.
 *
 * IPC mechanism: unnamed pipes (one per child).
 * Reason: simple, no extra setup, perfect one-producer/one-consumer fit.
 */
typedef struct {
    pid_t pid;
    int   current_node;
    int   next_node;   /* -1 when the traveler has reached the destination */
    int   finished;    /* 1 = traveler is done, 0 = position update        */
} TravelMessage;

/*
 * Child entry point — computes its own Dijkstra path and sends one
 * TravelMessage per node into write_fd, then exits.
 */
void child_travel(int src, int dst, int write_fd, void* graph);

#endif
