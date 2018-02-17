CC = gcc
CFLAGS = -Wall -g
OBJS = main.o log.o memory.o sig.o

httpd: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<
