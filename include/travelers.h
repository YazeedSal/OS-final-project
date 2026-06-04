#ifndef OS_FINAL_PROJECT_TRAVELERS_H
#define OS_FINAL_PROJECT_TRAVELERS_H

#include <sys/types.h>
#include <stdio.h>

/* forward declaration — avoids circular include with graph.h */
struct Graph;

#define MAX_TRAVELERS 15

typedef struct Traveler {
    int    source;
    int    destination;
    int*   path;
    int    pathLength;
    int    currentIndex;
    pid_t  pid;
    int    colorIndex;
} Traveler;

int  readTravelers(FILE* file, Traveler travelers[]);
void createTravelers(Traveler travelers[], int count);
void killTravelers(Traveler travelers[], int count);
void waitForTravelers(Traveler travelers[], int count);
int  computeTravelerPaths(struct Graph* graph, Traveler travelers[], int count);

#endif
