#ifndef GRAPH_H
#define GRAPH_H

#define MAX_NODES     15
#define MAX_TRAVELERS 15

/* One node in an adjacency list. */
typedef struct AdjNode {
    int destination;
    int weight;
    struct AdjNode* next;
} AdjNode;

/* A simple edge record. */
typedef struct Edge {
    int source;
    int destination;
    int weight;
} Edge;

/* The whole graph. */
typedef struct Graph {
    int       numNodes;
    int       numEdges;
    int       source;        /* single-traveler query (milestones 1-3) */
    int       destination;
    AdjNode** adjLists;
    Edge*     edges;

    /* milestone 4+: filled by main.c after computeTravelerPaths() */
    int       numTravelers;
} Graph;

int  readGraphFromFile(const char* filename, Graph* graph);
void printGraph(const Graph* graph);
void freeGraph(Graph* graph);

#endif
