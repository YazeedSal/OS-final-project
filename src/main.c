#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "../include/graph.h"
#include "../include/skip_comments.h"

#ifdef MILESTONE4
#include "../include/gui_m4.h"
#include "../include/travelers.h"
#endif

#ifdef MILESTONE5
#include "../include/gui_m4.h"
#include "../include/travelers.h"
#include "../include/ipc.h"
#endif

/* helper: reopen file and skip past the graph edges so readTravelers()
   can be called on the remaining content.                              */
static void skip_to_travelers(FILE* f, const Graph* g)
{
    int tmp;
    skip_comments(f);
    fscanf(f, "%d %d", &tmp, &tmp);       /* N M                */
    for (int i = 0; i < g->numEdges; i++) {
        skip_comments(f);
        fscanf(f, "%d %d %d", &tmp, &tmp, &tmp);  /* edges      */
    }
    /* leave file pointer here — readTravelers handles comments itself */
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        printf("Usage: %s <graph_file>\n", argv[0]);
        return 1;
    }

    Graph graph;
    if (!readGraphFromFile(argv[1], &graph)) return 1;

    printf("Graph loaded successfully.\n");
    printGraph(&graph);

/* ══════════════ MILESTONE 5 ══════════════ */
#ifdef MILESTONE5

    FILE* f = fopen(argv[1], "r");
    if (!f) { perror("fopen"); freeGraph(&graph); return 1; }
    skip_to_travelers(f, &graph);

    Traveler travelers[MAX_TRAVELERS];
    int numTravelers = readTravelers(f, travelers);
    fclose(f);

    if (numTravelers <= 0) {
        fprintf(stderr, "Error: no travelers found.\n");
        freeGraph(&graph); return 1;
    }

    int pipes[MAX_TRAVELERS][2];
    int read_fds[MAX_TRAVELERS];
    int sources[MAX_TRAVELERS], dests[MAX_TRAVELERS];
    pid_t pids[MAX_TRAVELERS];

    for (int i = 0; i < numTravelers; i++) {
        sources[i] = travelers[i].source;
        dests[i]   = travelers[i].destination;
        if (pipe(pipes[i]) == -1) { perror("pipe"); freeGraph(&graph); return 1; }
    }

    for (int i = 0; i < numTravelers; i++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); freeGraph(&graph); return 1; }

        if (pid == 0) {
            for (int j = 0; j < numTravelers; j++) {
                close(pipes[j][0]);
                if (j != i) close(pipes[j][1]);
            }
            child_travel(sources[i], dests[i], pipes[i][1], &graph);
            /* child_travel always exits */
        }

        pids[i]     = pid;
        read_fds[i] = pipes[i][0];
        close(pipes[i][1]);
    }

    printf("\nLaunching GUI...\n");
    draw_gui_m5(&graph, read_fds, sources, dests, numTravelers);

    for (int i = 0; i < numTravelers; i++)
        waitpid(pids[i], NULL, 0);

/* ══════════════ MILESTONE 4 ══════════════ */
#elif defined(MILESTONE4)

    FILE* f = fopen(argv[1], "r");
    if (!f) { perror("fopen"); freeGraph(&graph); return 1; }
    skip_to_travelers(f, &graph);

    Traveler travelers[MAX_TRAVELERS];
    int numTravelers = readTravelers(f, travelers);
    fclose(f);

    if (numTravelers <= 0) {
        fprintf(stderr, "Error: no travelers found.\n");
        freeGraph(&graph); return 1;
    }

    if (!computeTravelerPaths(&graph, travelers, numTravelers)) {
        fprintf(stderr, "Error: failed to compute paths.\n");
        freeGraph(&graph); return 1;
    }

    createTravelers(travelers, numTravelers);

    printf("\nLaunching GUI...\n");
    draw_gui_m4(&graph, travelers, numTravelers);

    killTravelers(travelers, numTravelers);
    waitForTravelers(travelers, numTravelers);

    for (int i = 0; i < numTravelers; i++)
        free(travelers[i].path);

/* ══════════════ MILESTONES 2 & 3 ══════════════ */
#else
    printf("\nLaunching GUI...\n");
    /* gui_m23.c exposes draw_gui() for milestones 2 and 3 */
    extern void draw_gui(const Graph*);
    draw_gui(&graph);
#endif

    freeGraph(&graph);
    return 0;
}
