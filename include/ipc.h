#ifndef IPC_H
#define IPC_H

#include <sys/types.h>

/*
 * Message a child sends to the parent through its pipe.
 * One message is sent per node visited during the trip.
 */
typedef struct {
    pid_t pid;
    int   current_node;
    int   next_node;  /* -1 when the traveler has reached the destination */
    int   finished;   /* 1 = traveler is done, 0 = position update        */
} TravelMessage;

/* Run the full milestone 5 simulation (read file, fork, IPC, log). */
void run_m5(const char *filename);

#endif
