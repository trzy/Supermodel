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

EXE = test.exe # Now it conforms tester/test.c

LFLAGS = -lz

CFLAGS = -DDRPPC_DEBUG -DDRPPC_PROFILE -Wall -Wno-unused -pedantic -std=gnu99 \
         -I./ -I./tester -I./middle -I./front -I./front/$(SOURCE_CPU) -I./front/$(SOURCE_CPU)/interp -I./back -I./back/$(TARGET_CPU)

AFLAGS = -f coff

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
TARGET_OBJ = obj/target.o obj/memory.o
else
TARGET_OBJ = 
endif

################################################################################

OBJ =	obj/test.o obj/cfgparse.o obj/file.o obj/unzip.o obj/toplevel.o obj/mmap.o obj/bb_lookup.o obj/ir.o \
		$(SOURCE_OBJ) $(TARGET_OBJ)

################################################################################

all:	$(OBJ)
		$(CC) $(CFLAGS) $(OBJ) -o $(EXE) $(LFLAGS)

clean:
		del obj\*.o
		del *.exe

obj/%.o:	%.c %.h
	$(COMPILE)

obj/%.o:	%.c
	$(COMPILE)

obj/%.o:	tester/%.c tester/%.h
	$(COMPILE)

obj/%.o:	tester/%.c
	$(COMPILE)

obj/%.o:	middle/%.c middle/%.h
	$(COMPILE)

obj/%.o:	middle/%.c
	$(COMPILE)

obj/%.o:    front/$(SOURCE_CPU)/%.c front/$(SOURCE_CPU)/%.h
	$(COMPILE)

obj/%.o:	front/$(SOURCE_CPU)/%.c
	$(COMPILE)

obj/%.o:    front/$(SOURCE_CPU)/interp/%.c front/$(SOURCE_CPU)/interp/%.h
	$(COMPILE)

obj/%.o:	front/$(SOURCE_CPU)/interp/%.c
	$(COMPILE)

obj/%.o:    back/$(TARGET_CPU)/%.c front/$(TARGET_CPU)/%.h
	$(COMPILE)

obj/%.o:	back/$(TARGET_CPU)/%.c
	$(COMPILE)

obj/%.o:	back/$(TARGET_CPU)/%.asm
	$(ASSEMBLE)
