/*
 * ipc.c — Milestone 5: child-side IPC
 *
 * IPC mechanism: unnamed pipes (one per child traveler).
 * Reason: simple, no extra setup, perfect one-producer/one-consumer fit —
 * each child writes position updates, the parent reads and logs them.
 *
 * Responsibilities:
 *   - child_travel(): compute own Dijkstra path, send one TravelMessage
 *                     per node visited, then exit.
 *
 * Everything else (forking, pipe creation, reading, logging, GUI) lives
 * in main.c and gui_m5.c so there is no duplication with graph.c /
 * travelers.c / dijkstra.c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../include/ipc.h"
#include "../include/graph.h"
#include "../include/dijkstra.h"

#define HOP_DELAY_US  300000   /* 300 ms per hop — weight W = W×300ms total */

void child_travel(int src, int dst, int write_fd, void* graph_ptr)
{
    Graph* graph = (Graph*)graph_ptr;

    graph->source      = src;
    graph->destination = dst;

    int path[MAX_NODES];
    int path_len = 0;
    int cost     = dijkstra_path(graph, path, &path_len);

    if (cost == -1 || path_len == 0) {
        TravelMessage msg = { getpid(), -1, -1, 1 };
        write(write_fd, &msg, sizeof(msg));
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

        /* sleep W×300ms so heavier edges take proportionally longer.
           For the last node (destination) there is no next edge, skip sleep. */
        if (msg.next_node != -1) {
            /* look up the weight of the edge current -> next */
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
    write(write_fd, &msg, sizeof(msg));

    close(write_fd);
    exit(0);
}
