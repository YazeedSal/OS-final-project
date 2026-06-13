#ifndef OS_FINAL_PROJECT_SYNC_H
#define OS_FINAL_PROJECT_SYNC_H

/* Initialise one named semaphore per node — called by parent before fork. */
void sync_init(int num_nodes);

/* Destroy and unlink all semaphores — called by parent after all children exit. */
void sync_destroy(int num_nodes);

/*
 * Re-open all semaphores by name — called by each CHILD after fork,
 * before any sync_enter_node / sync_leave_node call.
 * Children cannot use the parent's sem_t* pointers directly.
 */
void sync_reopen(int num_nodes);

/* Acquire the semaphore for a node (blocks if occupied). */
void sync_enter_node(int node);

/* Release the semaphore for a node. */
void sync_leave_node(int node);

#endif
