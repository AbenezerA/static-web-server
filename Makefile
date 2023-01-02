CC = gcc
CFLAGS = -g -Wall -Wpedantic -std=c17
 
LDFLAGS =
LDLIBS = 

http-server: http-server.o

http-server.o: http-server.c

.PHONY: clean
clean:
	rm -f http-server a.out *.o

.PHONY: all
all: clean http-server
