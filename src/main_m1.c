#include <stdio.h>
#include "../include/graph.h"
#include "../include/dijkstra.h"

int main(int argc, char* argv[]) {
    if (argc < 2) { printf("Usage: ./dijkstra <file>\n"); return 1; }
    Graph g;
    if (!readGraphFromFile(argv[1], &g)) return 1;
    dijkstra(&g);
    freeGraph(&g);
    return 0;
}
