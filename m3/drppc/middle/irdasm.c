/*
 * middle/irdasm.c
 *
 * IR Instruction disassembler.
 */

#include "ir.h"
#include "irdasm.h"

/*******************************************************************************
 Local Routines
*******************************************************************************/

static void PrintReg(UINT reg)
{
	if (IS_NATIVE_REG(reg))
		printf("$%u", reg - DFLOW_NATIVE_BASE);
	else if (IS_TEMP_REG(reg))
		printf("T%u", reg - DFLOW_TEMP_BASE);
	else if (IS_F_REG(reg))
		printf("F%u", reg - DFLOW_FP_BASE);
	else if (IS_R_REG(reg))
		printf("R%u", reg - DFLOW_INTEGER_BASE);
	else if (IS_FLAG_REG(reg))
		printf("Flag%u", reg - DFLOW_FLAG_BASE);
	else
		printf("???[%u]", reg);
}

static void TwoOp (IR * ir)
{
	PrintReg(IR_OPERAND_DEST(ir));
	printf("=");
	if (IR_CONSTANT(ir))
		printf("0x%08X", IR_OPERAND_CONST(ir));
	else
		PrintReg(IR_OPERAND_SRC1(ir));
}

static void ThreeOp (IR * ir)
{
	PrintReg(IR_OPERAND_DEST(ir));
	printf("=");
	PrintReg(IR_OPERAND_SRC1(ir));
	printf(",");
	if (IR_CONSTANT(ir))
		printf("0x%08X", IR_OPERAND_CONST(ir));
	else
		PrintReg(IR_OPERAND_SRC2(ir));
}

static void ShiftRotOp (IR * ir)
{
	PrintReg(IR_OPERAND_DEST(ir));
	printf("=");
	PrintReg(IR_OPERAND_SRC1(ir));
	printf(" by ");
	if (IR_CONSTANT(ir))
		printf("%d", IR_OPERAND_CONST(ir));
	else
		PrintReg(IR_OPERAND_SRC2(ir));
}

static void LoadOp (IR * ir)
{
	PrintReg(IR_OPERAND_DEST(ir));
	printf("=");
	if (IR_ADDR_CONST(ir))
		printf("[0x%X]", IR_OPERAND_SRC1(ir) + IR_OPERAND_CONST(ir));
	else
	{
		printf("[");
		PrintReg(IR_OPERAND_SRC1(ir));
		printf(",0x%X]", IR_OPERAND_CONST(ir));
	}
}

static void StoreOp (IR * ir)
{
	if (IR_ADDR_CONST(ir))
		printf("[0x%X]=", IR_OPERAND_SRC1(ir) + IR_OPERAND_CONST(ir));
	else
	{
		printf("[");
		PrintReg(IR_OPERAND_SRC1(ir));
		printf(",0x%X]=", IR_OPERAND_CONST(ir));
	}
	if (IR_DATA_CONST(ir))
		printf("0x%X", IR_OPERAND_SRC2(ir));
	else
		PrintReg(IR_OPERAND_SRC2(ir));
}

static void CallReadOp (IR * ir)
{
	PrintReg(IR_OPERAND_DEST(ir));
	printf("=[0x%08X]()", IR_OPERAND_CONST(ir));
}

static void BranchOp (IR * ir)
{
	if (IR_OPERAND_SRC1(ir) == -1)
	{
		if (IR_TARGET_CONST(ir))
			printf("@0x%08X", IR_OPERAND_SRC2(ir));
		else
		{
			printf("@"); PrintReg(IR_OPERAND_SRC2(ir));
		}
	}
	else
	{
		printf("if ("); PrintReg(IR_OPERAND_SRC1(ir)); printf(") ");
		if (IR_TARGET_CONST(ir))
			printf("@0x%08X", IR_OPERAND_SRC2(ir));
		else
		{
			printf("@"); PrintReg(IR_OPERAND_SRC2(ir));
		}
		printf(" else ");
		if (IR_NEXT_CONST(ir))
			printf("@0x%08X", IR_OPERAND_SRC3(ir));
		else
		{
			printf("@"); PrintReg(IR_OPERAND_SRC3(ir));
		}
	}
}

static void PrintDeps (IR * ir)
{
	UINT i;

	printf("\t\t<");
	for (i = 0; i < NUM_DFLOW_BITS; i++)
		if (GetDFlowInBit(ir, i) != 0)
		{
			PrintReg(i); printf(" "); 
		}
	printf(";");
	for (i = 0; i < NUM_DFLOW_BITS; i++)
		if (GetDFlowOutBit(ir, i) != 0)
		{
			printf(" "); PrintReg(i);
		}
	printf(">");
	if (IR_MUST_EMIT(ir) == TRUE)
		printf(" * ");
}

/*******************************************************************************
 Interface
*******************************************************************************/

void IR_PrintOp (IR * ir)
{
	switch (IR_OPERATION(ir))
	{
	case IR_NOP:	printf("nop     "); break;
	case IR_LOADI:	printf("loadi   "); TwoOp(ir); break;
	case IR_MOVE:	printf("move    "); TwoOp(ir); break;
	case IR_ADD:	printf("add     "); ThreeOp(ir); break;
	case IR_SUB:	printf("sub     "); ThreeOp(ir); break;
	case IR_NEG:	printf("neg     "); TwoOp(ir); break;
	case IR_MULU:	printf("mulu    "); ThreeOp(ir); break;
	case IR_AND:	printf("and     "); ThreeOp(ir); break;
	case IR_OR:		printf("or      "); ThreeOp(ir); break;
	case IR_XOR:	printf("xor     "); ThreeOp(ir); break;
	case IR_SHL:	printf("shl     "); ShiftRotOp(ir); break;
	case IR_SHR:	printf("shr     "); ShiftRotOp(ir); break;
	case IR_ROL:	printf("rol     "); ShiftRotOp(ir); break;
	case IR_ROR:	printf("ror     "); ShiftRotOp(ir); break;
	case IR_BREV:	printf("brev    "); TwoOp(ir); break;
	case IR_CMP:	printf("cmp");
					switch (IR_CONDITION(ir))
					{
					case IR_CONDITION_EQ:	printf("eq   "); break;
					case IR_CONDITION_NE:	printf("ne   "); break;
					case IR_CONDITION_LTU:	printf("ltu  "); break;
					case IR_CONDITION_GTU:	printf("gtu  "); break;
					case IR_CONDITION_LTEU:	printf("lteu "); break;
					case IR_CONDITION_GTEU:	printf("gteu "); break;
					case IR_CONDITION_LTS:	printf("lts  "); break;
					case IR_CONDITION_GTS:	printf("gts  "); break;
					case IR_CONDITION_LTES:	printf("ltes "); break;
					case IR_CONDITION_GTES:	printf("gtes "); break;
					default:				printf("%u   ", IR_CONDITION(ir)); break;
					}
					ThreeOp(ir);
					break;
	case IR_CONVERT:printf("cvt.");
					switch (IR_SIZE(ir))
					{
					case IR_S_TO_D:	printf("s.d "); break;
					case IR_D_TO_S: printf("d.s "); break;
					default: printf("%u   ", IR_SIZE(ir)); break;
					}
					TwoOp(ir);
					break;
	case IR_LOAD:	printf("load");
					switch (IR_SIZE(ir))
					{
					case IR_INT8:	printf("8   "); break;
					case IR_INT16:	printf("16  "); break;
					case IR_INT32:	printf("32  "); break;
					case IR_INT64:	printf("64  "); break;
					}
					LoadOp(ir);
					break;
	case IR_STORE:	printf("store");
					switch (IR_SIZE(ir))
					{
					case IR_INT8:	printf("8  "); break;
					case IR_INT16:	printf("16 "); break;
					case IR_INT32:	printf("32 "); break;
					case IR_INT64:	printf("64  "); break;
					}
					StoreOp(ir);
					break;
	case IR_CALLREAD:
					printf("cread   "); CallReadOp(ir);
					break;
	case IR_LOADPTR:
					printf("load");
					switch (IR_SIZE(ir))
					{
					case IR_INT8:	printf("8   "); break;
					case IR_INT16:	printf("16  "); break;
					case IR_INT32:	printf("32  "); break;
					}
					PrintReg(IR_OPERAND_DEST(ir));
					printf("=[@0x%08X]", IR_OPERAND_CONST(ir));
					break;
	case IR_STOREPTR:
					printf("store");
					switch (IR_SIZE(ir))
					{
					case IR_INT8:	printf("8  "); break;
					case IR_INT16:	printf("16 "); break;
					case IR_INT32:	printf("32 "); break;
					}
					printf("[@0x%08X]=", IR_OPERAND_CONST(ir));
					if (IR_DATA_CONST(ir))
						printf("0x%08X", IR_OPERAND_SRC1(ir));
					else
						PrintReg(IR_OPERAND_SRC1(ir));
					break;
	case IR_BRANCH:	printf("branch  ");	BranchOp(ir); break;
	case IR_BCOND:	printf("branch  ");	BranchOp(ir); break;
	case IR_SYNC:	printf("sync    "); break;
	default:		printf("[%.2u]    ", IR_OPERATION(ir)); break;
	}

	PrintDeps(ir);
	printf("\n");
}
