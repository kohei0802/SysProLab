CC = gcc
CFLAGS = -Wall -I./src

OBJS = src/common.o

all: server client

server: src/server.o $(OBJS)
	$(CC) $(CFLAGS) -o server src/server.o $(OBJS)

client: src/client.o $(OBJS)
	$(CC) $(CFLAGS) -o client src/client.o $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f server client src/*.o

.PHONY: all clean