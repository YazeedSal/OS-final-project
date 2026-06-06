#ifndef GUI_H
#define GUI_H

#include "graph.h"
#include "travelers.h"

/* Milestone 4 — path-driven animation with Play/Stop button */
void draw_gui_m4(const Graph* g, Traveler* travelers, int numTravelers);

/* Milestone 5 — pipe-driven animation, Play/Stop pauses visuals only */
void draw_gui_m5(const Graph* g,
                 int read_fds[], int sources[], int dests[],
                 int numTravelers);

#endif
