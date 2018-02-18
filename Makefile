CC = gcc
CFLAGS = -Wall -g
OBJS = main.o log.o memory.o sig.o

SRCDIR = ./src
SRCS = $(wildcard $(SRCDIR)/*.c)

OBJDIR = ./obj
OBJS = $(addprefix $(OBJDIR)/, $(notdir $(SRCS:.c=.o)))

TARGET = ./bin/httpd

$(TARGET): $(OBJS)
		-mkdir -p ./bin
			$(CC) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c
		-mkdir -p $(OBJDIR)
			$(CC) $(CFLAGS) -o $@ -c $<

all: clean $(TARGET)

clean:
		-rm -f $(OBJS) $(TARGET)

