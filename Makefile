CC      = gcc
CFLAGS  = -Wall -Wextra -g
LIBS    = -lraylib -lm -lX11

SRC     = src
INC     = include

# --- Milestone 1: dijkstra CLI (no GUI) ---
M1_SRCS = $(SRC)/main_m1.c $(SRC)/graph.c $(SRC)/dijkstra.c
M1_OUT  = dijkstra

# --- Milestone 2: sim - static graph visualiser (no animation/dijkstra) ---
M2_SRCS = $(SRC)/main.c $(SRC)/graph.c $(SRC)/gui.c $(SRC)/dijkstra.c
M2_OUT  = sim

# --- Milestone 3: sim - visualiser + Dijkstra animation ---
M3_SRCS = $(SRC)/main.c $(SRC)/graph.c $(SRC)/dijkstra.c $(SRC)/gui.c
M3_OUT  = sim

# --- Milestone 4: multi-process travelers with GUI ---
M4_SRCS = $(SRC)/main.c $(SRC)/graph.c $(SRC)/dijkstra.c $(SRC)/gui.c $(SRC)/travelers.c
M4_OUT  = sim

# --- Milestone 5: IPC - children compute own paths and report via pipes ---
M5_SRCS = $(SRC)/main_m5.c $(SRC)/ipc.c $(SRC)/graph.c $(SRC)/dijkstra.c
M5_OUT  = sim

.PHONY: milestone1 milestone2 milestone3 milestone4 milestone5 clean

milestone1:
	$(CC) $(CFLAGS) $(M1_SRCS) -I$(INC) -o $(M1_OUT) -lm
	@echo "Built milestone 1 -> ./dijkstra"

milestone2:
	$(CC) $(CFLAGS) $(M2_SRCS) -I$(INC) -o $(M2_OUT) $(LIBS)
	@echo "Built milestone 2 -> ./sim"

milestone3:
	$(CC) $(CFLAGS) -DMILESTONE3 $(M3_SRCS) -I$(INC) -o $(M3_OUT) $(LIBS)
	@echo "Built milestone 3 -> ./sim"

milestone4:
	$(CC) $(CFLAGS) -DMILESTONE4 $(M4_SRCS) -I$(INC) -o $(M4_OUT) $(LIBS)
	@echo "Built milestone 4 -> ./sim"

milestone5:
	$(CC) $(CFLAGS) $(M5_SRCS) -I$(INC) -o $(M5_OUT) -lm
	@echo "Built milestone 5 -> ./sim"

clean:
	rm -f $(M1_OUT) $(M2_OUT) dijkstra
	@echo "Cleaned build artifacts"
