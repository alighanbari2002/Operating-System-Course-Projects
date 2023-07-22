CC=gcc
RM=rm -rf

all: client server

server: server.c
	$(CC) server.c -o s.out

client: client.c
	$(CC) client.c -o c.out

clean:
	$(RM) *.out