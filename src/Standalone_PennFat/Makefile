# CC=gcc
# CFLAGS=-Iinclude

# pennfat: src/pennfat_main.c src/pennfat.c src/commands.c
#     $(CC) $(CFLAGS) -o pennfat src/main.c src/pennfat.c src/commands.c

# Compiler
CC = clang

# Compiler flags
CFLAGS = -Iinclude -Wall -g

# Name of the executable
TARGET = ../../bin/pennfat

SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
HEADERS = $(wildcard *.h)

# Default target
all: $(TARGET)

# Rule to create executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Rule to create object files
%.o: %.c
	$(CC) $(CFLAGS) -c $<

# Clean target
clean:
	rm -f $(TARGET) $(OBJS)

# Phony targets
.PHONY: all clean