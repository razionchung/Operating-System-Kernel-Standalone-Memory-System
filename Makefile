######################################################################
#
#                       Author: Hannah Pan
#                       Date:   01/31/2021
#
# The autograder will run the following command to build the program:
#
#       make -B
#
######################################################################

# name of the program to build
#
PROG=./bin/pennos
PROG2=./bin/pennfat
PROMPT='"pennos> "'

# Remove -DNDEBUG during development if assert(3) is used
#
override CPPFLAGS += -DNDEBUG -DPROMPT=$(PROMPT)

CC = clang

# Replace -O1 with -g for a debug version during development
#
CFLAGS = -Wall -Werror -g

SRCS = $(wildcard *.c) $(wildcard ./src/Kernel/*.c) ./src/Standalone_PennFat/descriptors.c ./src/Standalone_PennFat/pennfat.c ./src/Standalone_PennFat/directory.c
OBJS = $(SRCS:.c=.o)

SRCS2 = $(wildcard ./src/Standalone_PennFat/*.c)
OBJS2 = $(SRCS2:.c=.o)

# HEADERS = $(wildcard *.h)

# SRCS_2 = $(wildcard ./Standalone_PennFat/descriptors.c)
# OBJS_2 = $(SRCS_2:.c=.o)
# all: $(PROG)
# $(PROG) : $(OBJS)
# 	$(CC) -o $@ $^

# .PHONY : clean

# clean :
# 	$(RM) $(OBJS) $(PROG)
.PHONY : clean

all: $(PROG2) $(PROG)
$(PROG2) : $(OBJS2)
	$(CC) -o $@ $^
$(PROG) : $(OBJS) $(HEADERS)
	$(CC) -o $@ $(OBJS) ./src/Kernel/parser.o

# %.o: %.c $(HEADERS)
# 	$(CC) $(CPPFLAGS) $(CFLAGS) -c $<
	

clean :
	$(RM) $(OBJS) $(PROG)
