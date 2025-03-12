CFLAGS = -lrt

all: deliver/deliver server/server

deliver/deliver: deliver/deliver.c
	gcc $(CFLAGS) -o deliver/deliver deliver/deliver.c

server/server: server/server.c
	gcc $(CFLAGS) -o server/server server/server.c

clean:
	rm -f deliver/deliver server/server