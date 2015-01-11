# created by alch for MOXA search utility

PROJECT_NAME=moxalist

CC=gcc
CFLAGS=-c -Wall

all: main

main: main.o
	${CC} main.o -o ${PROJECT_NAME}

main.o: main.c
	${CC} ${CFLAGS} main.c

clean:
	rm -f *.o ${PROJECT_NAME}
