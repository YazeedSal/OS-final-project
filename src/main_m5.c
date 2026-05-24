#include <stdio.h>
#include "../include/ipc.h"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    run_m5(argv[1]);
    return 0;
}
