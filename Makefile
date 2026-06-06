CC     = gcc
CFLAGS = -Wall -Wextra -g
LIBS   = -lraylib -lm
SRC    = src
INC    = include

.PHONY: milestone1 milestone2 milestone3 milestone4 milestone5 clean

milestone1:
	$(CC) $(CFLAGS) $(SRC)/main_m1.c $(SRC)/graph.c $(SRC)/dijkstra.c \
	    -I$(INC) -o dijkstra -lm
	@echo "Built milestone 1 -> ./dijkstra"

milestone2:
	$(CC) $(CFLAGS) $(SRC)/main.c $(SRC)/graph.c $(SRC)/gui.c \
	    -I$(INC) -o sim $(LIBS)
	@echo "Built milestone 2 -> ./sim"

milestone3:
	$(CC) $(CFLAGS) -DMILESTONE3 $(SRC)/main.c $(SRC)/graph.c \
	    $(SRC)/dijkstra.c $(SRC)/gui.c \
	    -I$(INC) -o sim $(LIBS)
	@echo "Built milestone 3 -> ./sim"

milestone4:
	$(CC) $(CFLAGS) -DMILESTONE4 $(SRC)/main.c $(SRC)/graph.c \
	    $(SRC)/dijkstra.c $(SRC)/travelers.c $(SRC)/gui_m4.c \
	    -I$(INC) -o sim $(LIBS)
	@echo "Built milestone 4 -> ./sim"

milestone5:
	$(CC) $(CFLAGS) -DMILESTONE5 $(SRC)/main.c $(SRC)/graph.c \
	    $(SRC)/dijkstra.c $(SRC)/travelers.c $(SRC)/ipc.c $(SRC)/gui_m4.c \
	    -I$(INC) -o sim $(LIBS)
	@echo "Built milestone 5 -> ./sim"

clean:
	rm -f dijkstra sim
	@echo "Cleaned build artifacts"
