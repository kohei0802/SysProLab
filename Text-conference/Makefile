CC = gcc
# CFLAGS = -Wall -Wextra -I./src
CFLAGS = -Wall -I./src

all: server client

server: src/server.c src/server.h
	$(CC) $(CFLAGS) -o server src/server.c

client: src/client.c src/client.h
	$(CC) $(CFLAGS) -o client src/client.c

clean:
	rm -f server client

.PHONY: all clean