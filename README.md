# OS Simulation Project — Directed Graph Traffic Simulation

> **Course:** Operating Systems  
> **Theme:** A bunch of modules with traces connecting them on a computer PCB 
> **Language:** C | **Platform:** Linux | **Graphics:** raylib  

---

## About the Project

This project simulates movement across a directed graph, where multiple "passengers" (processes) travel simultaneously through nodes and edges. The simulation is built incrementally across 7 milestones, each introducing core OS concepts covered in the course: processes, inter-process communication (IPC), synchronization, and scheduling.

---

## Contributors

|       Name     |              Responsibility               |
|----------------|-------------------------------------------|
| Yazeed Salameh | Graphic user interface                    |
| Ammar Ekermawi | Algorithm building                        |
| Sami Maitah    | File reading and graph building           |

---

## Milestones

| # | Topic | Status |
|---|-------|--------|
| 1 | Graph representation & algorithms |✅ Done|
| 2 | Graphical interface — rendering the graph |✅ Done |
| 3 | Movement animation on the graph |✅ Done |
| 4 | Multiple processes & parent process | ✅ Done |
| 5 | Inter-process communication (IPC) | ✅ Done |
| 6 | Synchronization | ⏳ Pending |
| 7 | Scheduling algorithms | ⏳ Pending |

Each milestone is tagged in Git using the format `milestone1`, `milestone2`, etc.

---
 
## Implementation Description
 
### Milestone 1 — Dijkstra Shortest Path (CLI)
Reads a directed weighted graph from a file into an adjacency list. Runs Dijkstra's algorithm from the source node and prints the shortest path and total cost to stdout.
 
### Milestone 2 — Graph Visualiser (raylib GUI)
Displays the graph in a raylib window styled as a PCB circuit board. Vertices are drawn as CPU chips, directed edges as PCB traces with arrowheads, and edge weights as yellow labels. Source and destination nodes are colour-highlighted.
 
### Milestone 3 — Animation (Dijkstra Path Traversal)
Extends milestone 2 with a moving packet that travels the shortest path. Each edge is crossed in W hops of 300ms (W = edge weight), with a 1-second pause at intermediate nodes. A Play/Stop button controls the animation and an arrival banner is shown at the destination.

### Milestone 4 — Multiple Processes & Parent Process
Multiple travelers move simultaneously. The parent process reads the file, computes all Dijkstra paths, then forks one child per traveler using `fork()`. Children only "live" (sleep) while their trip is shown in the GUI. The parent manages the raylib loop and terminates each child with a signal once its route is complete.

### Milestone 5 — Inter-Process Communication (IPC)
Each child now computes its **own** Dijkstra path autonomously — the parent no longer passes route data to the children. IPC is done via **unnamed pipes** (one pipe per child). As each child travels node by node it writes a small message struct to its pipe; the parent uses `select()` to read from all pipes concurrently and prints a log line for every hop:

```
[PID=X] arrived at node N | next node: M
[PID=X] arrived at node N | DESTINATION
[PID=X] finished
```

Pipes were chosen because they are simple, require no shared-memory setup, and map cleanly onto the one-producer/one-consumer relationship between each child and the parent.

---


## File Structure
```
os-simulation-project/
├── src/                  # All .c source files
│   ├── main.c            # Entry point
│   ├── graph.c           # Graph data structures and algorithms
│   ├── gui.c             # GUI's functions
│   └── dijkstra.c        # Dijkstra's shortest path implementation
├── include/              # All .h header files
│   ├── dijkstra.h        # Dijkstra function declarations
│   ├── graph.h           # Graph functions and structure declartions
│   └── gui.h             # GUI's functions declarations
├── tests/                # Input/output test cases
│   └──  input1.txt
├── CMakeLists.txt        # CMake build configuration
├── Makefile              # Build configuration
├── .gitignore
└── README.md
```
---

## Compilation & Run Commands
 
### Milestone 1
```bash
make milestone1
./dijkstra tests/<input_file>
```
 
### Milestone 2
```bash
make milestone2
./sim tests/<input_file>
```
 
### Milestone 3
```bash
make milestone3
./sim tests/<input_file>
```

### Milestone 4
```bash
make milestone4
./sim tests/<input_file>
```

### Milestone 5
```bash
make milestone5
./sim tests/input_m5.txt
```

Input file format for milestones 4–5:
```
N M          # N nodes, M edges
src dst w    # one edge per line
...
K            # number of travelers
src dst      # one traveler per line
...
```
Lines starting with `#` are ignored.

### Clean
```bash
make clean
```
 
---



