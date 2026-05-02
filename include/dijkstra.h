#ifndef DIJKSTRA_H
#define DIJKSTRA_H

#include "graph.h"

void dijkstra(const Graph* graph);
int dijkstra_path(const Graph* graph, int* outPath, int* outLen);

#endif
