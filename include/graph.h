#ifndef GRAPH_H
#define GRAPH_H

/* One node in an adjacency list.
   Each node represents one directed edge: source -> destination. */
typedef struct AdjNode {
    int destination;
    int weight;
    struct AdjNode* next;
} AdjNode;

/* A simple edge record used only for clear printing. */
typedef struct Edge {
    int source;
    int destination;
    int weight;
} Edge;

/* The whole graph stored as an array of adjacency lists. */
typedef struct Graph {
    int numNodes;
    int numEdges;
    int source;
    int destination;
    AdjNode** adjLists;
    Edge* edges;
} Graph;

int readGraphFromFile(const char* filename, Graph* graph);
void printGraph(const Graph* graph);
void freeGraph(Graph* graph);

#endif
