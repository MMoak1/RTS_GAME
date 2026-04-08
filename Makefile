CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
LDFLAGS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

SRC = main.c game.c
OBJ = $(SRC:.c=.o)
EXEC = rts_game

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(EXEC)

.PHONY: all clean
