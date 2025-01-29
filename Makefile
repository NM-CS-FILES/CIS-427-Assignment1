CC = clang

all: server client

server: src/server.c
	$(CC) src/server.c src/shared.c src/sqlite3.c -o out/server.o

client: src/client.c
	$(CC) src/client.c src/shared.c src/sqlite3.c -o out/client.o
