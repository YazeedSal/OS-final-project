# OS Simulation Project — Directed Graph Traffic Simulation

> **Course:** Operating Systems  
> **Theme:** TBD *(e.g. train network, city map, communication network)*  
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
| 3 | Movement animation on the graph | ⏳ Pending |
| 4 | Multiple processes & parent process | ⏳ Pending |
| 5 | Inter-process communication (IPC) | ⏳ Pending |
| 6 | Synchronization | ⏳ Pending |
| 7 | Scheduling algorithms | ⏳ Pending |

Each milestone is tagged in Git using the format `milestone1`, `milestone2`, etc.

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

> More source files will be added as milestones progress (e.g. `simulation.c`, `ipc.c`, `sync.c`, `scheduler.c`).

