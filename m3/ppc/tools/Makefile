#
# Makefile for Basic Block Tools
#

CC = gcc
CFLAGS = -I. -I.. 

PROGS = bb_pprint

COMMON_HEADER = types.h ../analys.h bbtools.h
COMMON_OBJ = obj/bbtools.o

#
# all: All programs
#

all: $(PROGS)
	
bb_pprint:	bb_pprint.c $(COMMON_HEADER) $(COMMON_OBJ)
	$(CC) -o bb_pprint.exe bb_pprint.c $(COMMON_OBJ) $(CFLAGS)
	
obj/%.o:	%.c
	$(CC) -o $@ -c $< $(CFLAGS)