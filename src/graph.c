#include <stdio.h>
#include <stdlib.h>

#include "../include/graph.h"

/* Put the graph in a safe empty state. */
static void initGraph(Graph* graph)
{
    graph->numNodes = 0;
    graph->numEdges = 0;
    graph->source = -1;
    graph->destination = -1;
    graph->adjLists = NULL;
    graph->edges = NULL;
}

/* Add one directed edge to the adjacency list.
   This keeps edges in the same order they appear for each source node. */
static int addEdge(Graph* graph, int source, int destination, int weight)
{
    AdjNode* newNode;
    AdjNode* current;

    newNode = (AdjNode*)malloc(sizeof(AdjNode));
    if (newNode == NULL) {
        printf("Error: memory allocation failed.\n");
        return 0;
    }

    newNode->destination = destination;
    newNode->weight = weight;
    newNode->next = NULL;

    if (graph->adjLists[source] == NULL) {
        graph->adjLists[source] = newNode;
    } else {
        current = graph->adjLists[source];

        while (current->next != NULL) {
            current = current->next;
        }

        current->next = newNode;
    }

    return 1;
}

/* Read the graph from a text file.
   Returns 1 if everything was read successfully, or 0 if there was an error. */
int readGraphFromFile(const char* filename, Graph* graph)
{
    FILE* file;
    int i;
    int source;
    int destination;
    int weight;

    initGraph(graph);

    file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error: could not open file '%s'.\n", filename);
        return 0;
    }

    if (fscanf(file, "%d %d", &graph->numNodes, &graph->numEdges) != 2) {
        printf("Error: invalid input. Expected number of nodes and edges.\n");
        fclose(file);
        return 0;
    }

    if (graph->numNodes <= 0 || graph->numEdges < 0) {
        printf("Error: invalid number of nodes or edges.\n");
        fclose(file);
        return 0;
    }

    graph->adjLists = (AdjNode**)malloc(graph->numNodes * sizeof(AdjNode*));
    if (graph->adjLists == NULL) {
        printf("Error: memory allocation failed.\n");
        fclose(file);
        return 0;
    }

    for (i = 0; i < graph->numNodes; i++) {
        graph->adjLists[i] = NULL;
    }

    graph->edges = (Edge*)malloc(graph->numEdges * sizeof(Edge));
    if (graph->edges == NULL && graph->numEdges > 0) {
        printf("Error: memory allocation failed.\n");
        fclose(file);
        freeGraph(graph);
        return 0;
    }

    for (i = 0; i < graph->numEdges; i++) {
        if (fscanf(file, "%d %d %d", &source, &destination, &weight) != 3) {
            printf("Error: invalid edge input.\n");
            fclose(file);
            freeGraph(graph);
            return 0;
        }

        if (source < 0 || source >= graph->numNodes ||
            destination < 0 || destination >= graph->numNodes) {
            printf("Error: edge contains an invalid node number.\n");
            fclose(file);
            freeGraph(graph);
            return 0;
        }

        if (weight < 0) {
            printf("Error: negative weights are not allowed.\n");
            fclose(file);
            freeGraph(graph);
            return 0;
        }

        if (!addEdge(graph, source, destination, weight)) {
            fclose(file);
            freeGraph(graph);
            return 0;
        }

        graph->edges[i].source = source;
        graph->edges[i].destination = destination;
        graph->edges[i].weight = weight;
    }

    if (fscanf(file, "%d %d", &graph->source, &graph->destination) != 2) {
        printf("Error: invalid shortest path query input.\n");
        fclose(file);
        freeGraph(graph);
        return 0;
    }

    if (graph->source < 0 || graph->source >= graph->numNodes ||
        graph->destination < 0 || graph->destination >= graph->numNodes) {
        printf("Error: source or destination node is invalid.\n");
        fclose(file);
        freeGraph(graph);
        return 0;
    }

    fclose(file);
    return 1;
}

/* Print the graph data that was read from the file. */
void printGraph(const Graph* graph)
{
    int i;

    printf("Number of nodes: %d\n", graph->numNodes);
    printf("Number of edges: %d\n", graph->numEdges);
    printf("\nEdges:\n");

    for (i = 0; i < graph->numEdges; i++) {
        printf("%d -> %d (%d)\n",
               graph->edges[i].source,
               graph->edges[i].destination,
               graph->edges[i].weight);
    }

    printf("\nSource node: %d\n", graph->source);
    printf("Destination node: %d\n", graph->destination);
}

/* Free all memory used by the graph. */
void freeGraph(Graph* graph)
{
    int i;
    AdjNode* current;
    AdjNode* next;

    if (graph->adjLists != NULL) {
        for (i = 0; i < graph->numNodes; i++) {
            current = graph->adjLists[i];

            while (current != NULL) {
                next = current->next;
                free(current);
                current = next;
            }
        }

        free(graph->adjLists);
    }

    if (graph->edges != NULL) {
        free(graph->edges);
    }

    initGraph(graph);
}
