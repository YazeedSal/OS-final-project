#include <stdio.h>
#include <stdlib.h>

#include "../include/graph.h"
#include "../include/gui.h"
#include "../include/dijkstra.h"
int main(int argc, char* argv[]) {
    Graph graph;
    const char* filename;

   
    if (argc < 2) {
        printf("Usage: %s <graph_file>\n", argv[0]);
        printf("Example: %s tests/input1.txt\n", argv[0]);
        return 1;
    }

    filename = argv[1];

    if (!readGraphFromFile(filename, &graph)) {
        /* readGraphFromFile already printed a descriptive error */
        return 1;
    }

    printf("Graph loaded successfully.\n");
    printGraph(&graph);
    printf("\nLaunching GUI...\n");
    
    dijkstra(&graph);

    draw_gui(&graph);

  }
