GCC=gcc
CFLAGS=-g -Wall -std=c99

all:	main.c
	${GCC} ${CFLAGS} main.c -o smallsh

clean:
	rm -f smallsh *~
