#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../include/ipc.h"
#include "../include/graph.h"
#include "../include/dijkstra.h"

#ifdef MILESTONE6
#include "../include/sync.h"
#endif

#define HOP_DELAY_US 300000   /* 300 ms per hop — weight W = W×300ms total */

void child_travel(int src, int dst, int write_fd, void* graph_ptr)
{
    Graph* graph = (Graph*)graph_ptr;

#ifdef MILESTONE6
    /* re-open semaphores by name — the parent's sem_t* pointers are
       not valid in this child's address space after fork()           */
    sync_reopen(graph->numNodes);
#endif

    graph->source      = src;
    graph->destination = dst;

    int path[MAX_NODES];
    int path_len = 0;
    int cost     = dijkstra_path(graph, path, &path_len);

    if (cost == -1 || path_len == 0) {
        TravelMessage msg = { getpid(), -1, -1, 1, 0 };
        write(write_fd, &msg, sizeof(msg));
        close(write_fd);
        exit(0);
    }

    pid_t my_pid = getpid();
    TravelMessage msg;

    for (int i = 0; i < path_len; i++) {
        int node = path[i];

#ifdef MILESTONE6
        /* ── send WAITING message before trying to enter ── */
        msg.pid          = my_pid;
        msg.current_node = node;
        msg.next_node    = (i + 1 < path_len) ? path[i + 1] : -1;
        msg.finished     = 0;
        msg.waiting      = 1;   /* queued outside */
        write(write_fd, &msg, sizeof(msg));

        /* block here until the node is free */
        sync_enter_node(node);
#endif

        /* ── send ENTERED message (inside the node now) ── */
        msg.pid          = my_pid;
        msg.current_node = node;
        msg.next_node    = (i + 1 < path_len) ? path[i + 1] : -1;
        msg.finished     = 0;
        msg.waiting      = 0;   /* inside node */

        if (write(write_fd, &msg, sizeof(msg)) == -1) {
            perror("write");
#ifdef MILESTONE6
            sync_leave_node(node);
#endif
            break;
        }

#ifdef MILESTONE6
        /* stay inside the node for 1 second (critical section) */
        sleep(1);
        sync_leave_node(node);
#endif

        /* ── travel to next node: sleep W × 300ms ── */
        if (msg.next_node != -1) {
            int w = 1;
            AdjNode* cur = graph->adjLists[path[i]];
            while (cur) {
                if (cur->destination == path[i + 1]) { w = cur->weight; break; }
                cur = cur->next;
            }
            usleep((unsigned int)(w * HOP_DELAY_US));
        }
    }

    /* finished message */
    msg.pid          = my_pid;
    msg.current_node = -1;
    msg.next_node    = -1;
    msg.finished     = 1;
    msg.waiting      = 0;
    write(write_fd, &msg, sizeof(msg));

    close(write_fd);
    exit(0);
}
