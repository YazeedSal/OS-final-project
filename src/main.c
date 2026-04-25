#include <stdio.h>
#include "graph.h"
#include "dijkstra.h"

int main() {
    Graph graph;

    if (readGraphFromFile("input.txt", &graph) == 0) {
        printf("Error: could not read graph from file.\n");
        return 1;
    }

    dijkstra(&graph);

    freeGraph(&graph);

    return 0;
}