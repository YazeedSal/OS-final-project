/*
 * ipc.h — TravelMessage struct and child_travel declaration.
 *
 * M6: child_travel now also receives a gate_fd (read end of a
 *     parent→child pipe). The child blocks on gate_fd after sending
 *     a WAITING message and only proceeds once the parent writes a
 *     byte, confirming it is safe to enter the node visually.
 */

#ifndef IPC_H
#define IPC_H

#include <sys/types.h>

typedef struct {
    pid_t pid;
    int   current_node;
    int   next_node;
    int   finished;   /* 1 = traveler has reached destination */
    int   waiting;    /* 1 = queued outside node (M6 only)    */
} TravelMessage;

/*
 * gate_fd : read-end of a parent→child "gate" pipe (M6).
 *           Pass -1 for M5 (gate not used).
 */
void child_travel(int src, int dst, int write_fd, void* graph_ptr);

#endif /* IPC_H */
