#cmake_minimum_required(VERSION 4.1)
#project(OS_final_project C)
#
#set(CMAKE_C_STANDARD 11)
#
#include_directories(include)
#
#add_executable(OS_final_project
#        src/main.c
#        src/graph.c
#        src/dijkstra.c
#        tests/input1.txt
#        src/gui.c
#        src/gui.c
#)

cmake_minimum_required(VERSION 3.10)
project(OS_final_project C)

set(CMAKE_C_STANDARD 11)

include_directories(include)
include_directories(/usr/local/include)

add_executable(OS_final_project
        src/main.c
        src/graph.c
        src/dijkstra.c
        src/gui.c
)

target_link_libraries(OS_final_project
        /usr/local/lib/libraylib.a
        m
        pthread
        dl
        GL
        X11
)