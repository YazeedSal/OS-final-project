cmake_minimum_required(VERSION 4.1)
project(OS_final_project C)

set(CMAKE_C_STANDARD 11)

include_directories(include)

add_executable(OS_final_project
        src/main.c
        src/graph.c
        src/dijkstra.c
        input.txt
)