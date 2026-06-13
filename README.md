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
| 6 | Synchronization | ✅ Done |
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

### Milestone 6 — Synchronization
Adds a critical section constraint: **at most one traveler can be inside a node at any time**. When a traveler arrives at an occupied node it waits outside until the current occupant finishes its 1-second stay and leaves.

**Synchronization mechanism: POSIX named semaphores** (`sem_open`) — one semaphore per node, initialised to 1 (binary semaphore). `sync_enter_node()` calls `sem_wait()` which blocks if the node is occupied. `sync_leave_node()` calls `sem_post()` to release it. Semaphores are named using the parent PID to avoid collisions between concurrent runs.

Each child now sends two pipe messages per node: a **WAITING** message before calling `sem_wait()` (so the GUI can show the traveler queued outside), and an **ENTERED** message after acquiring the semaphore (so the GUI shows the traveler inside). The GUI renders queued travelers as dim pulsing rings offset around the node, distinct from the solid moving packets.

---


## File Structure
```
os-simulation-project/
├── src/                  # All .c source files
│   ├── main.c            # Entry point (handles all milestones)
│   ├── graph.c           # Graph data structures and file reading
│   ├── dijkstra.c        # Dijkstra's shortest path implementation
│   ├── gui.c             # GUI for milestones 4 & 5 (draw_gui_m4, draw_gui_m5)
│   ├── gui_m23.c         # GUI for milestones 2 & 3
│   ├── travelers.c       # Traveler struct, fork, kill, wait, path computation
│   ├── ipc.c             # Child travel logic with pipe-based IPC (milestone 5)
│   └── sync.c            # POSIX named semaphore per node (milestone 6)
├── include/              # All .h header files
│   ├── graph.h           # Graph struct and function declarations
│   ├── dijkstra.h        # Dijkstra function declarations
│   ├── gui.h             # GUI function declarations (M4 & M5)
│   ├── travelers.h       # Traveler struct and function declarations
│   ├── ipc.h             # TravelMessage struct and child_travel declaration
│   ├── sync.h            # sync_init/destroy/enter/leave declarations (milestone 6)
│   └── skip_comments.h   # Shared inline helper for skipping # comment lines
├── tests/                # Input/output test cases
│   ├── input1.txt        # Sample input for milestones 1–3
│   └── input_m5.txt      # Sample input for milestones 4–5
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

### Milestone 6
```bash
make milestone6
./sim tests/input_m6.txt
```

### Clean
```bash
make clean
```
 
---
