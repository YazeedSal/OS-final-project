CC      = gcc
CFLAGS  = -Wall -Wextra -g
LIBS    = -lraylib -lm

SRC     = src
INC     = include

# ─── Milestone 1: dijkstra CLI (no GUI) ─────────────────────────────────────
M1_SRCS = $(SRC)/main_m1.c $(SRC)/graph.c $(SRC)/dijkstra.c
M1_OUT  = dijkstra

# ─── Milestone 2: sim — static graph visualiser (no animation/dijkstra) ─────
M2_SRCS = $(SRC)/main.c $(SRC)/graph.c $(SRC)/gui.c $(SRC)/dijkstra.c
M2_OUT  = sim

# ─── Milestone 3: sim — visualiser + Dijkstra animation ─────────────────────
M3_SRCS = $(SRC)/main.c $(SRC)/graph.c $(SRC)/dijkstra.c $(SRC)/gui.c
M3_OUT  = sim

# ────────────────────────────────────────────────────────────────────────────

.PHONY: milestone1 milestone2 milestone3 clean

milestone1:
	$(CC) $(CFLAGS) $(M1_SRCS) -I$(INC) -o $(M1_OUT) -lm
	@echo "Built milestone 1 -> ./dijkstra"

milestone2:
	$(CC) $(CFLAGS) $(M2_SRCS) -I$(INC) -o $(M2_OUT) $(LIBS)
	@echo "Built milestone 2 -> ./sim"

milestone3:
	$(CC) $(CFLAGS) -DMILESTONE3 $(M3_SRCS) -I$(INC) -o $(M3_OUT) $(LIBS)
	@echo "Built milestone 3 -> ./sim"

clean:
	rm -f $(M1_OUT) $(M2_OUT)
	@echo "Cleaned build artifacts"
