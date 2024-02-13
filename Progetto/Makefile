# Progetto: ESCAPE ROOM
# Autore: Francesco Panattoni
# Matricola: 604230

# Makefile

CC = gcc
CFLAGS = -Wall

all: server client

server: server.c
	$(CC) $(CFLAGS) $< -o $@

client: client.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f server client