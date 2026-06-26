/*
 * main_m7.c — Milestone 7 runner.
 *
 * Run:  ./sim -schd fcfs <file>
 *
 * This file only does the plumbing: parse the command line, read the graph
 * and travelers, hand them to run_scheduled_sim() (the real work lives in
 * scheduler.c), then either replay the result in the GUI or print it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/graph.h"
#include "../include/travelers.h"
#include "../include/skip_comments.h"
#include "../include/scheduler.h"

#ifndef NO_GUI
#include "../include/gui_m4.h"
#endif

/* skip the graph header + edges so readTravelers() sees the traveler block */
static void skip_to_travelers(FILE* f, const Graph* g)
{
    int a, b, c;
    skip_comments(f);
    if (fscanf(f, "%d %d", &a, &b) != 2) return;
    for (int i = 0; i < g->numEdges; i++) {
        skip_comments(f);
        if (fscanf(f, "%d %d %d", &a, &b, &c) != 3) return;
    }
}

int main(int argc, char* argv[])
{
    /* accepted forms:  ./sim -schd fcfs <file>   (or)   ./sim <file> */
    const char* algo = NULL;
    const char* file = NULL;

    if (argc == 4 && strcmp(argv[1], "-schd") == 0) {
        algo = argv[2];
        file = argv[3];
    } else if (argc == 2) {
        algo = "fcfs";
        file = argv[1];
    } else {
        fprintf(stderr, "Usage: %s -schd fcfs <file>\n", argv[0]);
        return 1;
    }

    SchedAlgo sched;
    if (strcmp(algo, "fcfs") == 0)      sched = SCHED_FCFS;
    else if (strcmp(algo, "sjf") == 0)  sched = SCHED_SJF;
    else if (strcmp(algo, "priority") == 0) sched = SCHED_LPID;
    else {
        fprintf(stderr, "Error: unknown algorithm '%s' (use 'fcfs', 'sjf', or 'priority').\n", algo);
        return 1;
    }

    Graph g;
    if (!readGraphFromFile(file, &g)) return 1;

    FILE* f = fopen(file, "r");
    if (!f) { perror("fopen"); freeGraph(&g); return 1; }
    skip_to_travelers(f, &g);
    Traveler tr[MAX_TRAVELERS];
    int K = readTravelers(f, tr);
    fclose(f);

    if (K <= 0) {
        fprintf(stderr, "Error: no travelers found.\n");
        freeGraph(&g);
        return 1;
    }

    const char* algoName = (sched == SCHED_SJF)  ? "SJF"  :
                          (sched == SCHED_LPID) ? "LPID" : "FCFS";
    printf("Scheduler: %s  |  nodes: %d  edges: %d  travelers: %d\n",
           algoName, g.numNodes, g.numEdges, K);

    int numEvents = 0;
    SimEvent* events = run_scheduled_sim(&g, tr, K, &numEvents, sched);


#ifndef NO_GUI
    {
        int sources[MAX_TRAVELERS], dests[MAX_TRAVELERS];
        for (int i = 0; i < K; i++) {
            sources[i] = tr[i].source;
            dests[i]   = tr[i].destination;
        }
        draw_gui_m7(&g, events, numEvents, sources, dests, K, algoName);
    }
#endif

    free(events);
    freeGraph(&g);
    return 0;
}
