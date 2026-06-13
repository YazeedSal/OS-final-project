#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>

#include "../include/graph.h"
#include "../include/sync.h"

static sem_t* node_sems[MAX_NODES];
static char   sem_names[MAX_NODES][64];
static int    sem_count = 0;

void sync_init(int num_nodes)
{
    sem_count = num_nodes;

    for (int i = 0; i < num_nodes; i++) {
        snprintf(sem_names[i], sizeof(sem_names[i]),
                 "/node_sem_%d_%d", (int)getpid(), i);

        /* clean up any leftover from a previous crashed run */
        sem_unlink(sem_names[i]);

        node_sems[i] = sem_open(sem_names[i], O_CREAT | O_EXCL, 0600, 1);
        if (node_sems[i] == SEM_FAILED) {
            perror("sem_open (init)");
            exit(1);
        }
    }
}

/*
 * Called by each child process after fork().
 * The parent's sem_t* pointers are not valid in the child's address space,
 * so the child must re-open every semaphore by name.
 * The names were set by the parent in sync_init() and are shared via
 * the inherited sem_names[][] array (plain char data, safe across fork).
 */
void sync_reopen(int num_nodes)
{
    for (int i = 0; i < num_nodes; i++) {
        node_sems[i] = sem_open(sem_names[i], 0);   /* open existing */
        if (node_sems[i] == SEM_FAILED) {
            perror("sem_open (reopen)");
            exit(1);
        }
    }
}

void sync_destroy(int num_nodes)
{
    for (int i = 0; i < num_nodes; i++) {
        if (node_sems[i] && node_sems[i] != SEM_FAILED) {
            sem_close(node_sems[i]);
            sem_unlink(sem_names[i]);
        }
    }
}

void sync_enter_node(int node)
{
    if (node < 0 || node >= MAX_NODES) return;
    if (sem_wait(node_sems[node]) == -1) {
        perror("sem_wait");
        exit(1);
    }
}

void sync_leave_node(int node)
{
    if (node < 0 || node >= MAX_NODES) return;
    if (sem_post(node_sems[node]) == -1) {
        perror("sem_post");
        exit(1);
    }
}
