CC = gcc

.PHONY: all install server client clean

all: server client
	make server
	make client

install:
	sudo apt-get install libmpg123-dev libao-dev

server:
	$(CC) ServerUDP.c -o ServerUDP -lmpg123 -lao
serverRun:
	$(CC) ServerUDP.c -o ServerUDP -lmpg123 -lao && ./ServerUDP

client:
	$(CC) ClientUDP.c -o ClientUDP -lmpg123 -lao
clientRun:
	$(CC) ClientUDP.c -o ClientUDP -lmpg123 -lao && ./ClientUDP 1.mp3

clean:
	$(RM) ServerUDP ClientUDP
