CC = gcc

.PHONY: all install server client clean

all: server client
	make server
	make client

install:
	sudo apt-get install libpthread-stubs0-dev libmpg123-dev libao-dev libcjson-dev

server:
	$(CC) server.c -o server -lpthread -lmpg123 -lao -lcjson
serverRun:
	$(CC) server.c -o server -lpthread -lmpg123 -lao -lcjson && ./server

client:
	$(CC) client.c -o client -lpthread
clientRun:
	$(CC) client.c -o client -lpthread && ./client

clean:
	$(RM) server client
