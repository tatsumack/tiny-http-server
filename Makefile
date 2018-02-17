CC = gcc
CFLAGS = -Wall -g
OBJS = main.o log.o memory.o

httpd: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

main.o: main.c
	$(CC) -c $(CFLAGS) -o $@ $<

log.o: log.c
	$(CC) -c $(CFLAGS) -o $@ $<

memory.o: memory.c
	$(CC) -c $(CFLAGS) -o $@ $<

