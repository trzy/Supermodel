# Default makefile for drppc.
# It requires gcc 3.2 and nasm 0.98.

CC = gcc
LD = gcc
RM = del

# Select the source CPU
# SOURCE_CPU = powerpc
# Default source CPU
ifeq ($(SOURCE_CPU),)
SOURCE_CPU = powerpc
endif

# Select the target CPU
# TARGET_CPU = x86
# Default target CPU
ifeq ($(TARGET_CPU),)
TARGET_CPU = x86
endif

# Executable
EXE = test.exe

# Compiler flags
CFLAGS =    -I. -std=gnu99 -Wall -pedantic -Wno-unused -DDRPPC_DEBUG -DDRPPC_PROFILE -O2 # -O0 -g -pg

# Linker flags
LFLAGS =    -lz -Wl,-M >test.map

# Compile syntax
COMPILE = @echo Compiling $<; $(CC) $< $(CFLAGS) -c -o $@

OBJ_DIR = obj

# Objects
OBJ =	obj/test.o \
	obj/cfgparse.o \
	obj/file.o \
	obj/unzip.o \
	obj/toplevel.o \
	obj/mmap.o \
	obj/mempool.o \
	obj/bblookup.o \
	obj/ir.o \
	obj/irdasm.o \
	obj/ccache.o

# Include the source and target CPUs makefiles
include front/$(SOURCE_CPU)/$(SOURCE_CPU).mak
include back/$(TARGET_CPU)/$(TARGET_CPU).mak

# Main rules
all:	$(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(EXE) $(LFLAGS)

clean:
	$(RM) obj/*
	$(RM) $(EXE)

# Object rules

obj/%.o:	%.c %.h
	$(COMPILE)

obj/%.o:	%.c
	$(COMPILE)

obj/%.o:	tester/%.c tester/%.h
	$(COMPILE)

obj/%.o:	tester/%.c
	$(COMPILE)

obj/%.o:	front/%.c front/%.h
	$(COMPILE)

obj/%.o:	front/%.c
	$(COMPILE)

obj/%.o:	middle/%.c middle/%.h
	$(COMPILE)

obj/%.o:	middle/%.c
	$(COMPILE)

obj/%.o:	back/%.c back/%.h
	$(COMPILE)

obj/%.o:	back/%.c
	$(COMPILE)
