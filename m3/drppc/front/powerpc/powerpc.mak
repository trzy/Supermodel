# Local makefile for the IBM/Motorola PowerPC frontend
# included by the main makefile

# Additional flags
SRC_HERE = front/powerpc
CFLAGS += -DSOURCE_CPU_HEADER="\"$(SRC_HERE)/powerpc.h\""

# Additional object files
OBJ += $(OBJ_DIR)/powerpc.o $(OBJ_DIR)/ppcdasm.o $(OBJ_DIR)/4xx.o $(OBJ_DIR)/6xx.o
OBJ += $(OBJ_DIR)/i_alu.o $(OBJ_DIR)/i_branch.o $(OBJ_DIR)/i_fp.o $(OBJ_DIR)/i_ldst.o $(OBJ_DIR)/i_ps.o $(OBJ_DIR)/i_special.o
OBJ += $(OBJ_DIR)/d_alu.o $(OBJ_DIR)/d_branch.o $(OBJ_DIR)/d_fp.o $(OBJ_DIR)/d_ldst.o $(OBJ_DIR)/d_ps.o $(OBJ_DIR)/d_special.o

# Main Source Files
$(OBJ_DIR)/powerpc.o:	$(SRC_HERE)/powerpc.c $(SRC_HERE)/powerpc.h $(SRC_HERE)/idesc.h $(SRC_HERE)/internal.h
	$(COMPILE)
$(OBJ_DIR)/4xx.o:	$(SRC_HERE)/4xx.c $(SRC_HERE)/powerpc.h $(SRC_HERE)/internal.h
	$(COMPILE)
$(OBJ_DIR)/6xx.o:	$(SRC_HERE)/6xx.c $(SRC_HERE)/powerpc.h $(SRC_HERE)/internal.h
	$(COMPILE)

# Interpreter
$(OBJ_DIR)/%.o:	$(SRC_HERE)/interp/%.c $(SRC_HERE)/powerpc.h $(SRC_HERE)/internal.h
	$(COMPILE)

# Decoder
$(OBJ_DIR)/%.o:	$(SRC_HERE)/decode/%.c $(SRC_HERE)/powerpc.h $(SRC_HERE)/internal.h
	$(COMPILE)

# Disassembler
$(OBJ_DIR)/ppcdasm.o:   $(SRC_HERE)/ppcdasm.c $(SRC_HERE)/ppcdasm.h
	$(COMPILE)
