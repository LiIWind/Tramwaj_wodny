CC = gcc
CFLAGS = -Wall

all: main kapitan dyspozytor pasazer

main: main.c common.h
	$(CC) $(CFLAGS) -o main main.c

kapitan: kapitan.c common.h
	$(CC) $(CFLAGS) -o kapitan kapitan.c

dyspozytor: dyspozytor.c common.h
	$(CC) $(CFLAGS) -o dyspozytor dyspozytor.c

pasazer: pasazer.c common.h
	$(CC) $(CFLAGS) -o pasazer pasazer.c

clean:
	rm -f main kapitan dyspozytor pasazer *.o
