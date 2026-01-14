CC = gcc
CFLAGS = -Wall
COMMON_OBJS = logger.o
DEPS = common.h logger.h

all: main kapitan dyspozytor pasazer

logger.o: logger.c $(DEPS)
	$(CC) $(CFLAGS) -c logger.c

main: main.c $(DEPS) $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o main main.c $(COMMON_OBJS)

kapitan: kapitan.c $(DEPS) $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o kapitan kapitan.c $(COMMON_OBJS)

dyspozytor: dyspozytor.c $(DEPS) $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o dyspozytor dyspozytor.c $(COMMON_OBJS)

pasazer: pasazer.c $(DEPS) $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o pasazer pasazer.c $(COMMON_OBJS)

clean:
	rm -f main kapitan dyspozytor pasazer *.o
	ipcs -m | grep `whoami` | awk '{print $$2}' | xargs -n1 ipcrm -m 2>/dev/null || true
	ipcs -s | grep `whoami` | awk '{print $$2}' | xargs -n1 ipcrm -s 2>/dev/null || true

