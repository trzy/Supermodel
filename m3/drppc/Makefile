#
# Default makefile for drppc.
#
# It requires GCC 3.2 and NASM 0.98.
#

CC = gcc
LD = gcc
AS = nasm

################################################################################

ifeq ($(SOURCE_CPU),)
SOURCE_CPU = powerpc
endif

ifeq ($(TARGET_CPU),)
TARGET_CPU = x86
endif

################################################################################

EXE = drppc.exe

CFLAGS = -DDRPPC_DEBUG -DDRPPC_PROFILE -Wall -Wno-unused -pedantic \
         -I./ -I./middle -I./front/$(SOURCE_CPU) -I./back/$(TARGET_CPU) -std=gnu99

AFLAGS = -f coff -b

COMPILE = $(CC) $< $(CFLAGS) -c -o $@
ASSEMBLE = $(AS) $< $(AFLAGS) -o $@

ifeq ($(SOURCE_CPU),powerpc)
SOURCE_OBJ = obj/source.o obj/4xx.o obj/6xx.o obj/decode.o obj/i_arith.o \
			 obj/i_branch.o obj/i_fp.o obj/i_ldst.o obj/i_logic.o obj/i_ps.o \
			 obj/i_special.o obj/disasm.o
else
SOURCE_OBJ =
endif

ifeq ($(TARGET_CPU),x86)
TARGET_OBJ = obj/target.o
else
TARGET_OBJ = 
endif

################################################################################

OBJ =	obj/test.o obj/toplevel.o obj/mmap.o obj/bb_lookup.o obj/ir.o \
		$(SOURCE_OBJ) $(TARGET_OBJ)

################################################################################

all:	$(OBJ)
		$(CC) $(CFLAGS) $(OBJ) -o $(EXE)

clean:
		del obj\*.o

obj/%.o:	%.c %.h
	$(COMPILE)

obj/%.o:	%.c
	$(COMPILE)

obj/%.o:	middle/%.c middle/%.h
	$(COMPILE)

obj/%.o:	middle/%.c
	$(COMPILE)

obj/%.o:    front/$(SOURCE_CPU)/%.c front/$(SOURCE_CPU)/%.h
	$(COMPILE)

obj/%.o:	front/$(SOURCE_CPU)/%.c
	$(COMPILE)

obj/%.o:    back/$(TARGET_CPU)/%.c front/$(TARGET_CPU)/%.h
	$(COMPILE)

obj/%.o:	back/$(TARGET_CPU)/%.c
	$(COMPILE)

