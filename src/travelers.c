#include "travelers.h"
#include "dijkstra.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

int readTravelers(FILE *file, Traveler travelers[]) {
    int count;

    if (fscanf(file, "%d", &count) != 1) {
        return -1;
    }

    for (int i = 0; i < count; i++) {
        if (fscanf(file,"%d %d",
            &travelers[i].source,
            &travelers[i].destination) != 2) {
            return -1;
        }

    travelers[i].path =NULL;
        travelers[i].pathLength = 0;
        travelers[i].currentIndex = 0;
        travelers[i].pid = -1;
        travelers[i].colorIndex = i;
    }
    return count;
}

void createTravelers(Traveler travelers[], int count) {
    for (int i = 0; i < count; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            exit(1);
        }
        if (pid == 0) {
            printf("[%d] started\n", getpid());

            while (1) {
                pause();
            }
        }

    travelers[i].pid = pid;
    }
}

void killTravelers(Traveler traveler[], int count) {
    for (int i = 0; i < count; i++) {
        if (traveler[i].pid > 0) {
            kill(traveler[i].pid, SIGTERM);
        }
    }
}

void waitForTravelers(Traveler travelers[], int count) {
    for (int i = 0; i < count; i++) {
        if (travelers[i].pid > 0) {
            waitpid(travelers[i].pid, NULL, 0);
        }
    }
}


int computeTravelerPaths(Graph *graph, Traveler travelers[], int count) {
    for (int i = 0; i < count; i++) {
        graph->source = travelers[i].source;
        graph->destination = travelers[i].destination;

        travelers[i].path = malloc(graph->numNodes * sizeof(int));

        if (travelers[i].path == NULL) {
            return 0;
        }

        if (!dijkstra_path(graph,
                           travelers[i].path,
                           &travelers[i].pathLength)) {
            travelers[i].pathLength = 0;
                           }

        travelers[i].currentIndex = 0;
    }

    return 1;
}