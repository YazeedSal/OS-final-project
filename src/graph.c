#include <stdio.h>
#include <stdlib.h>

#include "../include/graph.h"
#include "../include/skip_comments.h"

static void initGraph(Graph* graph)
{
    graph->numNodes    = 0;
    graph->numEdges    = 0;
    graph->source      = -1;
    graph->destination = -1;
    graph->adjLists    = NULL;
    graph->edges       = NULL;
    graph->numTravelers = 0;
}

static int addEdge(Graph* graph, int source, int destination, int weight)
{
    AdjNode* newNode = malloc(sizeof(AdjNode));
    if (!newNode) { printf("Error: memory allocation failed.\n"); return 0; }

    newNode->destination = destination;
    newNode->weight      = weight;
    newNode->next        = NULL;

    if (graph->adjLists[source] == NULL) {
        graph->adjLists[source] = newNode;
    } else {
        AdjNode* cur = graph->adjLists[source];
        while (cur->next) cur = cur->next;
        cur->next = newNode;
    }
    return 1;
}

int readGraphFromFile(const char* filename, Graph* graph)
{
    FILE* file = fopen(filename, "r");
    if (!file) { printf("Error: could not open file '%s'.\n", filename); return 0; }

    initGraph(graph);

    skip_comments(file);
    if (fscanf(file, "%d %d", &graph->numNodes, &graph->numEdges) != 2) {
        printf("Error: invalid input. Expected number of nodes and edges.\n");
        fclose(file); return 0;
    }

    if (graph->numNodes <= 0 || graph->numEdges < 0) {
        printf("Error: invalid number of nodes or edges.\n");
        fclose(file); return 0;
    }

    graph->adjLists = malloc(graph->numNodes * sizeof(AdjNode*));
    if (!graph->adjLists) {
        printf("Error: memory allocation failed.\n");
        fclose(file); return 0;
    }
    for (int i = 0; i < graph->numNodes; i++) graph->adjLists[i] = NULL;

    graph->edges = malloc(graph->numEdges * sizeof(Edge));
    if (!graph->edges && graph->numEdges > 0) {
        printf("Error: memory allocation failed.\n");
        fclose(file); freeGraph(graph); return 0;
    }

    for (int i = 0; i < graph->numEdges; i++) {
        int src, dst, w;
        skip_comments(file);
        if (fscanf(file, "%d %d %d", &src, &dst, &w) != 3) {
            printf("Error: invalid edge input.\n");
            fclose(file); freeGraph(graph); return 0;
        }
        if (src < 0 || src >= graph->numNodes ||
            dst < 0 || dst >= graph->numNodes) {
            printf("Error: edge contains an invalid node number.\n");
            fclose(file); freeGraph(graph); return 0;
        }
        if (w < 0) {
            printf("Error: negative weights are not allowed.\n");
            fclose(file); freeGraph(graph); return 0;
        }
        if (!addEdge(graph, src, dst, w)) {
            fclose(file); freeGraph(graph); return 0;
        }
        graph->edges[i].source      = src;
        graph->edges[i].destination = dst;
        graph->edges[i].weight      = w;
    }

    /*
     * Try to read an optional single-query line (milestones 1-3).
     * We peek at the next token — if it looks like two ints that are
     * both in range [0, numNodes) we treat it as a query line.
     * But we ONLY do this if the file still has the old format, i.e.
     * the next non-comment token is followed by exactly one more int
     * before a comment or EOF.
     *
     * Safe approach: read two ints speculatively then check if what
     * follows is a comment or EOF (traveler count would be a single int).
     */
    {
        long pos = ftell(file);
        skip_comments(file);
        int s = -1, d = -1;
        int n = fscanf(file, "%d %d", &s, &d);

        if (n == 2 &&
            s >= 0 && s < graph->numNodes &&
            d >= 0 && d < graph->numNodes) {
            /* peek: next non-whitespace should be '#', EOF, or a single int
               (traveler count). If it's another int pair it's the query. */
            long after = ftell(file);
            skip_comments(file);
            int peek;
            int np = fscanf(file, "%d", &peek);
            fseek(file, after, SEEK_SET);   /* put peek back */

            /* if peek succeeded and the value <= numNodes it could be
               a traveler count — in that case what we read was NOT a query */
            if (np == 1 && peek > 0 && peek <= graph->numNodes) {
                /* looks like traveler count — s,d were travelers, rewind */
                fseek(file, pos, SEEK_SET);
            } else {
                /* genuine query line */
                graph->source      = s;
                graph->destination = d;
            }
        } else {
            /* nothing useful — rewind */
            fseek(file, pos, SEEK_SET);
        }
    }

    fclose(file);
    return 1;
}

void printGraph(const Graph* graph)
{
    int i;
    printf("Number of nodes: %d\n", graph->numNodes);
    printf("Number of edges: %d\n", graph->numEdges);
    printf("\nEdges:\n");
    for (i = 0; i < graph->numEdges; i++)
        printf("%d -> %d (%d)\n",
               graph->edges[i].source,
               graph->edges[i].destination,
               graph->edges[i].weight);
    if (graph->source >= 0)
        printf("\nSource node: %d\nDestination node: %d\n",
               graph->source, graph->destination);
}

void freeGraph(Graph* graph)
{
    if (graph->adjLists) {
        for (int i = 0; i < graph->numNodes; i++) {
            AdjNode* cur = graph->adjLists[i];
            while (cur) {
                AdjNode* next = cur->next;
                free(cur);
                cur = next;
            }
        }
        free(graph->adjLists);
    }
    if (graph->edges) free(graph->edges);
    initGraph(graph);
}
