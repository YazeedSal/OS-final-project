#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "../include/dijkstra.h"

#define MAX_NODES 15


//Finding minimum distance node
static int findMinDistance(int dist[], int visited[], int n) {
    int min = INT_MAX;
    int minIndex = -1;

    for (int i = 0; i < n; i++) {
        if (!visited[i] && dist[i] < min) {
            min = dist[i];
            minIndex = i;
        }
    }

    return minIndex;
}

//Path printing
static void printPath(int parent[], int v) {
    if (parent[v] == -1) {
        printf("%d", v);
        return;
    }

    printPath(parent, parent[v]);
    printf(" -> %d", v);
}



void dijkstra(const Graph* graph) {
    int n = graph->numNodes;
    int start = graph->source;
    int end = graph->destination;

    int* dist = malloc(n * sizeof(int));
    int* visited = malloc(n * sizeof(int));
    int* parent = malloc(n * sizeof(int));

    if (dist == NULL || visited == NULL || parent == NULL) {
        printf("Error: memory allocation failed.\n");
        free(dist);
        free(visited);
        free(parent);
        return;
    }

    for (int i = 0; i < n; i++) {
        dist[i] = INT_MAX;
        visited[i] = 0;
        parent[i] = -1;
    }

    dist[start] = 0;

    for (int count = 0; count < n; count++) {
        int u = findMinDistance(dist, visited, n);

        if (u == -1) {
            break;
        }

        visited[u] = 1;

        AdjNode* current = graph->adjLists[u];

        while (current != NULL) {
            int v = current->destination;
            int weight = current->weight;

            if (!visited[v] && dist[u] != INT_MAX &&
                dist[u] + weight < dist[v]) {

                dist[v] = dist[u] + weight;
                parent[v] = u;
                }

            current = current->next;
        }
    }

    if (dist[end] == INT_MAX) {
        printf("No path found\n");
    } else {
        printPath(parent, end);
        printf("\n%d\n", dist[end]);
    }

    free(dist);
    free(visited);
    free(parent);
}

/* ─── GUI variant: same algorithm, returns path array ──────────────────── */
/*
 * Fills outPath[] with [source, n1, n2, ..., destination].
 * Sets *outLen to the number of nodes in the path (0 if unreachable).
 * Returns total cost, or -1 if unreachable.
 */
int dijkstra_path(const Graph* graph, int* outPath, int* outLen)
{
    int n     = graph->numNodes;
    int start = graph->source;
    int end   = graph->destination;

    int* dist    = malloc(n * sizeof(int));
    int* visited = malloc(n * sizeof(int));
    int* parent  = malloc(n * sizeof(int));

    *outLen = 0;

    if (!dist || !visited || !parent) {
        free(dist); free(visited); free(parent);
        return -1;
    }

    for (int i = 0; i < n; i++) {
        dist[i] = INT_MAX; visited[i] = 0; parent[i] = -1;
    }
    dist[start] = 0;

    for (int count = 0; count < n; count++) {
        int u = findMinDistance(dist, visited, n);
        if (u == -1) break;
        visited[u] = 1;

        AdjNode* cur = graph->adjLists[u];
        while (cur) {
            int v = cur->destination, w = cur->weight;
            if (!visited[v] && dist[u] != INT_MAX &&
                dist[u] + w < dist[v]) {
                dist[v]   = dist[u] + w;
                parent[v] = u;
                }
            cur = cur->next;
        }
    }

    if (dist[end] == INT_MAX) {
        free(dist); free(visited); free(parent);
        return -1;
    }

    /* reconstruct path backwards via parent[], then reverse into outPath */
    int tmp[MAX_NODES];
    int len = 0;
    for (int v = end; v != -1; v = parent[v])
        tmp[len++] = v;

    for (int i = 0; i < len; i++)
        outPath[i] = tmp[len - 1 - i];

    *outLen = len;
    int cost = dist[end];

    free(dist); free(visited); free(parent);
    return cost;
}
