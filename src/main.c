#include <stdio.h>
#include <stdlib.h>

#include "../include/graph.h"

#ifdef MILESTONE4
#include "../include/gui_m4.h"
#include "../include/travelers.h"
#else
#include "../include/gui.h"
#endif

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

#ifdef MILESTONE4
    /* re-open file to read the traveler block */
    FILE* f = fopen(argv[1], "r");
    if (!f) { perror("fopen"); freeGraph(&graph); return 1; }

    /* skip: N M, all edges, single query line */
    int tmp;
    fscanf(f, "%d %d", &tmp, &tmp);
    for (int i = 0; i < graph.numEdges; i++)
        fscanf(f, "%d %d %d", &tmp, &tmp, &tmp);
    fscanf(f, "%d %d", &tmp, &tmp);

    Traveler travelers[MAX_TRAVELERS];
    int numTravelers = readTravelers(f, travelers);
    fclose(f);

    if (numTravelers <= 0) {
        printf("Error: no travelers found in file.\n");
        freeGraph(&graph); return 1;
    }

    /* parent computes all Dijkstra paths before forking */
    if (!computeTravelerPaths(&graph, travelers, numTravelers)) {
        printf("Error: failed to compute traveler paths.\n");
        freeGraph(&graph); return 1;
    }

    /* fork one child per traveler */
    createTravelers(travelers, numTravelers);

    printf("\nLaunching GUI...\n");
    draw_gui(&graph, travelers, numTravelers);

    /* animation done — signal and reap all children */
    killTravelers(travelers, numTravelers);
    waitForTravelers(travelers, numTravelers);

    for (int i = 0; i < numTravelers; i++)
        free(travelers[i].path);

#else
    printf("\nLaunching GUI...\n");
    draw_gui(&graph);
#endif

    freeGraph(&graph);
    return 0;
}
