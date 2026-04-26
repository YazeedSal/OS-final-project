#ifndef GUI_H
#define GUI_H

#include "graph.h"

/*
 * Opens a raylib window and renders the graph as a PCB-style diagram.
 * CPU chips = vertices, PCB traces with arrowheads = directed edges.
 * The source and destination nodes from the Dijkstra query are
 * highlighted with a blue and red ring respectively.
 *
 * Blocks until the user closes the window (ESC or the X button).
 */
void draw_gui(const Graph* g);

#endif
