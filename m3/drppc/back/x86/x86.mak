#
# Local makefile for the Intel x86 backend (included by makefile.mak
# in the root directory.)
#

AS = nasmw

ASFLAGS =	-f coff

ASSEMBLE = $(AS) $< $(ASFLAGS) -o $@

# Backend specific flags
HERE = back/x86
CFLAGS += -DTARGET_CPU_HEADER="\"$(HERE)/x86.h\""
CFLAGS += -DNUM_TARGET_NATIVE_REGS="6"

# Additional object files
OBJ += $(OBJ_DIR)/x86.o
OBJ += $(OBJ_DIR)/emit.o
OBJ += $(OBJ_DIR)/native.o
OBJ += $(OBJ_DIR)/disasmx86.o
OBJ += $(OBJ_DIR)/insnsd.o
OBJ += $(OBJ_DIR)/sync.o

# Additional rules
$(OBJ_DIR)/%.o:		$(HERE)/%.c $(HERE)/%.h $(HERE)/emit.h
	$(COMPILE)
$(OBJ_DIR)/disasmx86.o:	$(HERE)/disasm.c
	$(COMPILE)
$(OBJ_DIR)/%.o:		$(HERE)/%.c
	$(COMPILE)
$(OBJ_DIR)/native.o:	$(HERE)/native.asm
	$(ASSEMBLE)
