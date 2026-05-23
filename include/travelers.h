#ifndef OS_FINAL_PROJECT_TRAVELERS_HRAVELERS_H
#define OS_FINAL_PROJECT_TRAVELERS_H

#include <sys/types.h>
#include <stdio.h>
typedef struct Traveler {

    int source;
    int destination;

    int *path;
    int pathLength;

    int currentIndex;

    pid_t pid;

    // Color color;
    //needs "Include raylib.h"
    int colorIndex;

} Traveler;

int readTravelers(FILE *file, Traveler travelers[]);

void createTravelers(Traveler travelers[], int count);

void killTravelers(Traveler travelers[], int count);

void waitForTravelers(Traveler travelers[], int count);

#endif