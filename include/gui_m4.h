/*
 * gui_m4.h — declarations for draw_gui_m4() and draw_gui_m5()
 */

#ifndef GUI_M4_H
#define GUI_M4_H

#include "graph.h"
#include "travelers.h"

/* Milestone 4: path-driven animation */
void draw_gui_m4(const Graph* g, Traveler* travelers, int numTravelers);

/*
 * Milestone 5 / 6: live pipe-driven animation.
 *
 * gate_write_fds: array of write-ends of parent→child gate pipes.
 *                 Pass NULL for Milestone 5 (no gate needed).
 *                 For Milestone 6, the GUI writes 1 byte per traveler
 *                 when it receives a WAITING message, unblocking the
 *                 child so it can attempt sem_wait().
 */
void draw_gui_m5(const Graph* g,
                 int read_fds[],
                 int sources[], int dests[],
                 int numTravelers);

#endif /* GUI_M4_H */
