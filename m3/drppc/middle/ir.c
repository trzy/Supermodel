/*
 * middle/ir.c
 *
 * Intermediate Representation (IR) language generation and management.
 */

/*
 * Description:
 *
 * This module is the heart of code optimization. Once the front-end has built
 * the intermediate representation (IR) of the basic block (a doubly linked
 * list of IR instructions), the middle-end is free to do whatever manipulations
 * it wants on the list (trying to preserve semantics, of course.) 
 *
 * At the current stage, the middle-end performs constant folding and constant
 * propagation, dead code removal, register allocation, and another dead code
 * removal pass, in this order.
 *
 * Since the basic block is basically (!) a sequence of linear instructions, no
 * loop optimization or basic block inlining can be performed in the middle-end.
 */

#include <string.h>	// memset in IR_NewInst, memcpy in RemoveDeadCode

#include "drppc.h"
#include "toplevel.h"
#include "mempool.h"
#include "ir.h"
#include "irdasm.h"
#include SOURCE_CPU_HEADER
#include TARGET_CPU_HEADER

/*******************************************************************************
 Configuration

 Middle end configuration. Toggle those shiny little numbers and stare at the
 magic.
*******************************************************************************/

#define PRINT_IR_DISASM				1

#define PERFORM_CONSTANT_FOLDING	1
#define LOG_CONSTANT_FOLDING		0	/* Actually unused */

#define PERFORM_DEAD_CODE_REMOVAL	1
#define LOG_DEAD_CODE_REMOVAL		0

#define PERFORM_REG_ALLOCATION		1
#define LOG_REG_ALLOCATION			0	/* Actually unused */

/*******************************************************************************
 Local Variables
*******************************************************************************/

static MEMPOOL	* cache;			// The IR code cache
static IR		* sentinel;			// The IR BB sentinel

static struct
{
	BOOL is_const;
	UINT32 constant;

} reg_constant[NUM_DFLOW_BITS];

static UINT32	default_search[NUM_DFLOW_WORDS];
static UINT32	default_removable[NUM_DFLOW_WORDS];

/*******************************************************************************
 Constant Evaluation

 ...
*******************************************************************************/

static UINT32 DoAdd (UINT32 a, UINT32 b)
{
	return a + b;
}

static UINT32 DoSub (UINT32 a, UINT32 b)
{
	return a - b;
}

static UINT32 DoNeg (UINT32 a)
{
	return -a;
}

static UINT32 DoMulu (UINT32 a, UINT32 b)
{
	return a * b;
}

static UINT32 DoDivu (UINT32 a, UINT32 b)
{
	return a / b;
}

static UINT32 DoAnd (UINT32 a, UINT32 b)
{
	return a & b;
}

static UINT32 DoOr (UINT32 a, UINT32 b)
{
	return a | b;
}

static UINT32 DoXor (UINT32 a, UINT32 b)
{
	return a ^ b;
}

static UINT32 DoNot (UINT32 a)
{
	return ~a;
}

static UINT32 DoRol (UINT32 val, UINT sa)
{
	return ((val << sa) | (val >> (32 - sa)));
}

static UINT32 DoRor (UINT32 val, UINT sa)
{
	return ((val >> sa) | (val << (32 - sa)));
}

static UINT32 DoShl (UINT32 val, UINT sa)
{
	return val << sa;
}

static UINT32 DoShr (UINT32 val, UINT sa)
{
	return val >> sa;
}

static UINT32 DoReverse8In16 (UINT32 val)
{
	return	((val >> 8) & 0x00FF) |
			((val << 8) & 0xFF00);
}

static UINT32 DoReverse8In32 (UINT32 val)
{
	return	((val >> 24) & 0x000000FF) |
			((val >>  8) & 0x0000FF00) |
			((val <<  8) & 0x00FF0000) |
			((val << 24) & 0xFF000000);
}

static UINT32 DoCompare (IR_CONDITION cond, UINT32 op1, UINT32 op2)
{
	UINT32 flag;

	switch (cond)
	{
	case IR_CONDITION_EQ:	flag = op1 == op2; break;
	case IR_CONDITION_NE:	flag = op1 != op2; break;
	case IR_CONDITION_LTU:	flag = (UINT)op1 < (UINT)op2; break;
	case IR_CONDITION_GTU:	flag = (UINT)op1 > (UINT)op2; break;
	case IR_CONDITION_LTS:	flag = (INT)op1 < (INT)op2; break;
	case IR_CONDITION_GTS:	flag = (INT)op1 > (INT)op2; break;
	case IR_CONDITION_LTEU:	flag = (UINT)op1 <= (UINT)op2; break;
	case IR_CONDITION_GTEU:	flag = (UINT)op1 >= (UINT)op2; break;
	case IR_CONDITION_LTES:	flag = (INT)op1 <= (INT)op2; break;
	case IR_CONDITION_GTES:	flag = (INT)op1 >= (INT)op2; break;
	default:				ASSERT(0); break;
	}

	return flag;
}

/*******************************************************************************
 IR List

 A few basic functions to add nodes to and remove nodes from the IR list.
*******************************************************************************/

static IR * NewInst (void)
{
	/*
	 * NOTE: it assumes the sentinel to be ready for work.
	 */

	IR	* ir;

	ir = (IR *)MemPool_Grab(cache, sizeof(IR));

	// Reset all fields
	memset(ir, 0, sizeof(IR));

	IR_OPERAND_DEST(ir) = -1;
	IR_OPERAND_SRC1(ir) = -1;
	IR_OPERAND_SRC2(ir) = -1;
	IR_OPERAND_SRC3(ir) = -1;

	// Link the node
	sentinel->prev->next = ir;
	ir->prev = sentinel->prev;
	sentinel->prev = ir;
	ir->next = sentinel;

	return ir;
}

static void DeleteInst (IR * ir)
{
	ir->prev->next = ir->next;
	ir->next->prev = ir->prev;
}

static void PrintCurrentBB (void)
{
	IR		* ir = sentinel;
	UINT	addr = 0;

	do
	{
		IR_PrintOp(ir);
		ir = ir->next;
		addr ++;

	} while (ir != sentinel);
}

/*******************************************************************************
 Data flow vector management

 These routines must be used to access bits in the dflow_in[] and dflow_out[]
 vectors.
*******************************************************************************/

UINT GetDFlowInBit (IR * ir, UINT bit)
{
	ASSERT(ir != NULL);
	ASSERT(bit < NUM_DFLOW_BITS);

	return (ir->dflow_in[bit / 32] & (1 << (bit & 31))) ? 1 : 0;
}

UINT GetDFlowOutBit (IR * ir, UINT bit)
{
	ASSERT(ir != NULL);
	ASSERT(bit < NUM_DFLOW_BITS);

	return (ir->dflow_out[bit / 32] & (1 << (bit & 31))) ? 1 : 0;
}

void SetDFlowInBit (IR * ir, UINT bit)
{
	ASSERT(ir != NULL);
	ASSERT(bit < NUM_DFLOW_BITS);

	ir->dflow_in[bit / 32] |= (1 << (bit & 31));
}

void SetDFlowOutBit (IR * ir, UINT bit)
{
	ASSERT(ir != NULL);
	ASSERT(bit < NUM_DFLOW_BITS);

	ir->dflow_out[bit / 32] |= (1 << (bit & 31));
}

void ClearDFlowInBit (IR * ir, UINT bit)
{
	ASSERT(ir != NULL);
	ASSERT(bit < NUM_DFLOW_BITS);

	ir->dflow_in[bit / 32] &= ~(1 << (bit & 31));
}

void ClearDFlowOutBit (IR * ir, UINT bit)
{
	ASSERT(ir != NULL);
	ASSERT(bit < NUM_DFLOW_BITS);

	ir->dflow_out[bit / 32] &= ~(1 << (bit & 31));
}

/*******************************************************************************
 Dependency Tracking

 Here we keep track of instruction dependencies, and perform dead code removal.
*******************************************************************************/

static void BuildDefaultSearchMask (void)
{
	UINT i;

	memset(default_search, 0, sizeof(default_search));
	memset(default_removable, 0, sizeof(default_removable));

	for (i = DFLOW_FLAG_BASE; i <= DFLOW_FLAG_END; i ++)		// Commit all flags
	{
		default_search[i/32] |= (1 << (i & 31));
		default_removable[i/32] |= (1 << (i & 31));				// All flags are removable
	}
	for (i = DFLOW_INTEGER_BASE; i <= DFLOW_INTEGER_END; i ++)	// Commit all R's
		default_search[i/32] |= (1 << (i & 31));
	for (i = DFLOW_FP_BASE; i <= DFLOW_FP_END; i ++)			// Commit all F's
		default_search[i/32] |= (1 << (i & 31));
	for (i = DFLOW_NATIVE_BASE; i <= DFLOW_NATIVE_END; i ++)	// Commit all native regs
		default_search[i/32] |= (1 << (i & 31));
}

static void SetUpDFlowVectors (IR * ir)
{
	// Setup dest bit in dflow_out[]
	if (IR_OPERAND_DEST(ir) != -1)
		SetDFlowOutBit(ir, IR_OPERAND_DEST(ir));

	// Setup src* bits in dflow_in[]
	if (IR_OPERAND_SRC1(ir) != -1 && IR_CONSTANT(ir) != 1)
		SetDFlowInBit(ir, IR_OPERAND_SRC1(ir));
	if (IR_OPERAND_SRC2(ir) != -1 && IR_CONSTANT(ir) != 2)
		SetDFlowInBit(ir, IR_OPERAND_SRC2(ir));
	if (IR_OPERAND_SRC3(ir) != -1 && IR_CONSTANT(ir) != 3)
		SetDFlowInBit(ir, IR_OPERAND_SRC3(ir));
}

static void RemoveDeadCode (void)
{

#if PERFORM_DEAD_CODE_REMOVAL

	IR		* ir;
	UINT32	search[NUM_DFLOW_WORDS];
	BOOL	needed;
	UINT	i;

	/*
	 * Start with the default search mask.
	 */

	memcpy(search, default_search, sizeof(search));

	/*
	 * From the last instruction to the first, excluding the sentinel ...
	 */

	ir = sentinel->prev;
	while (ir != sentinel)
	{
		/*
		 * Check if the current instruction satisfies any of the dependencies
		 * required by the following instructions.
		 */

		needed = FALSE;
		for (i = 0; i < NUM_DFLOW_WORDS; i++)
		{
			/*
			 * Remove from the dep output vector those bits that aren't needed
			 * later and actually *can* be removed by default.
			 * See BuildDefaultSearchMask.
			 */

			ir->dflow_out[i] &= ~(default_removable[i] & ~search[i]);

			if (ir->dflow_out[i] & search[i])
				needed = TRUE;
		}

		if (!needed && !IR_MUST_EMIT(ir))
		{
			/*
			 * The instruction generates no deps that are needed later, and it
			 * doesn't have the must_emit flag set. Remove it.
			 */

			DeleteInst(ir);
			ir = ir->next;
		}
		else
		{
			/*
			 * The instruction outputs deps that are used later, so we cannot
			 * delete it. Additionally, we have to add its in deps to the
			 * search mask.
			 */

			for (i = 0; i < NUM_DFLOW_WORDS; i++)
			{
				search[i] &= ~ir->dflow_out[i];
				search[i] |= ir->dflow_in[i];
			}
		}

		ir = ir->prev;
	}

#endif // PERFORM_DEAD_CODE_REMOVAL

}

/*******************************************************************************
 Constant Folding

 Here are managed informations about register values.
*******************************************************************************/

static BOOL IsConstant (UINT reg)
{
#if PERFORM_CONSTANT_FOLDING
	ASSERT(reg < NUM_DFLOW_BITS);
	return reg_constant[reg].is_const;
#else
	return FALSE;
#endif
}

static UINT32 GetConstant (UINT reg)
{
#if PERFORM_CONSTANT_FOLDING
	ASSERT(reg < NUM_DFLOW_BITS);
	return reg_constant[reg].constant;
#else
	// Should never be called (since IsConstant() returns FALSE)
	ASSERT(0);
	return -1;
#endif
}

static void SetConstant (UINT reg, UINT32 constant)
{
#if PERFORM_CONSTANT_FOLDING
	ASSERT(reg < NUM_DFLOW_BITS);
	reg_constant[reg].is_const = TRUE;
	reg_constant[reg].constant = constant;
#endif
}

static void ResetConstant (UINT reg)
{
#if PERFORM_CONSTANT_FOLDING
	ASSERT(reg < NUM_DFLOW_BITS);
	reg_constant[reg].is_const = FALSE;
	reg_constant[reg].constant = 0xDEADBEEF;
#endif
}

/*******************************************************************************
 Register Allocator

 Here's the fancy register allocator.
*******************************************************************************/

static void DoRegAllocPass (void)
{
#if PERFORM_REG_ALLOCATION
#endif
}

/*******************************************************************************
 IR Instruction Encoder Templates

 Used by the IR instruction encoders.
*******************************************************************************/

/*
 * IR_EncodeALU_D_S:
 *
 * Encodes the specified ALU operation (format: Dest, Src).
 */

static void IR_EncodeALU_D_S
(
	IR_OP	op,
	UINT	dest,
	UINT	src,
	UINT32	(* Compute)(UINT32)
)
{
	if (IsConstant(src))
	{
		UINT32 k = GetConstant(src);

		IR_EncodeLoadi(dest, Compute(k));
	}
	else
	{
		IR * ir = NewInst();

		IR_OPERATION(ir)	= op;
		IR_OPERAND_DEST(ir)	= dest;
		IR_OPERAND_SRC1(ir)	= src;

		SetUpDFlowVectors(ir);

		ResetConstant(dest);
	}
}

/*
 * IR_EncodeALU_D_S_S:
 *
 * Encodes the specified ALU operation (format: Dest, Src1, Src2).
 */

static void IR_EncodeALU_D_S_S
(
	IR_OP	op,
	UINT	dest,
	UINT	src1,
	UINT	src2,
	UINT32	(* Compute)(UINT32, UINT32)
)
{
	if (IsConstant(src1) && IsConstant(src2))
	{
		UINT32	k1 = GetConstant(src1);
		UINT32	k2 = GetConstant(src2);

		IR_EncodeLoadi(dest, Compute(k1, k2));
	}
	else
	{
		IR	* ir = NewInst();

		IR_OPERATION(ir)	= op;
		IR_OPERAND_DEST(ir)	= dest;
		IR_OPERAND_SRC1(ir)	= src1;
		IR_OPERAND_SRC2(ir)	= src2;

		SetUpDFlowVectors(ir);

		ResetConstant(dest);
	}
}

/*
 * IR_EncodeALU_D_S_S:
 *
 * Encodes the specified ALU operation (format: Dest, Src, Imm).
 */

static void IR_EncodeALU_D_S_I
(
	IR_OP	op,
	UINT	dest,
	UINT	src,
	UINT32	imm,
	UINT32 (* Compute)(UINT32, UINT32)
)
{
	if (IsConstant(src))
	{
		UINT32	k = GetConstant(src);

		IR_EncodeLoadi(dest, Compute(k, imm));
	}
	else
	{
		IR	* ir = NewInst();

		IR_OPERATION(ir)	= op;
		IR_OPERAND_DEST(ir)	= dest;
		IR_OPERAND_SRC1(ir)	= src;
		IR_OPERAND_SRC3(ir)	= imm;
		IR_CONSTANT(ir)		= 3;

		SetUpDFlowVectors(ir);

		ResetConstant(dest);
	}
}

/*
 * IR_EncodeLoad
 *
 * Encodes a load operation.
 */

static void IR_EncodeLoad (IR_SIZE size, UINT dest, UINT base, UINT32 offs)
{
	IR * ir = NewInst();

	IR_OPERATION(ir)		= IR_LOAD;
	IR_SIZE(ir)				= size;
	IR_OPERAND_DEST(ir)		= dest;

	if (IsConstant(base))
	{
		IR_ADDR_CONST(ir)	= TRUE;
		IR_OPERAND_SRC1(ir)	= GetConstant(base);
	}
	else
	{
		IR_ADDR_CONST(ir)	= FALSE;
		IR_OPERAND_SRC1(ir)	= base;
	}

	IR_OPERAND_SRC3(ir)		= offs;
	IR_CONSTANT(ir)			= 3;

	/*
	 * NOTE:
	 *
	 * SetMustEmitFlag must be set to TRUE unless the target address is known
	 * _and_ doesn't belong to an I/O area. The point is that the original code
	 * may poll hardware ports and ignore the read value. The dead code removal
	 * pass would delete such instructions, breaking the hardware poll
	 * originally intended.
	 */

	IR_MUST_EMIT(ir) = TRUE;

	// TEMP
	{
		SetDFlowOutBit(ir, IR_OPERAND_DEST(ir));
		if (IR_ADDR_CONST(ir) == FALSE)
			SetDFlowInBit(ir, IR_OPERAND_SRC1(ir));
	}

	ResetConstant(dest);
}

/*
 * IR_EncodeStore
 *
 * Encodes a store operation.
 */

static void IR_EncodeStore (IR_SIZE size, UINT src, UINT base, UINT32 offs)
{
	IR * ir = NewInst();

	IR_OPERATION(ir)		= IR_STORE;
	IR_SIZE(ir)				= size;

	if (IsConstant(base))
	{
		IR_ADDR_CONST(ir)	= TRUE;
		IR_OPERAND_SRC1(ir)	= GetConstant(base);
	}
	else
	{
		IR_ADDR_CONST(ir)	= FALSE;
		IR_OPERAND_SRC1(ir)	= base;
	}

	// NOTE: not constant propagation support for 64-bit data.

	if (IsConstant(src) && size != IR_INT64)
	{
		IR_DATA_CONST(ir)	= TRUE;
		IR_OPERAND_SRC2(ir)	= GetConstant(src);
	}
	else
	{
		IR_DATA_CONST(ir)	= FALSE;
		IR_OPERAND_SRC2(ir)	= src;
	}

	IR_OPERAND_SRC3(ir)		= offs;
	IR_CONSTANT(ir)			= 3;

	IR_MUST_EMIT(ir) = TRUE;

	// TEMP
	{
		if (IR_ADDR_CONST(ir) == FALSE)
			SetDFlowInBit(ir, IR_OPERAND_SRC1(ir));
		if (IR_DATA_CONST(ir) == FALSE)
			SetDFlowInBit(ir, IR_OPERAND_SRC2(ir));
	}
}

/*******************************************************************************
 IR Instruction Encoders

 They do most of the work.
*******************************************************************************/

/*
 * void IR_EncodeLoadi (UINT dest, UINT32 imm);
 */

void IR_EncodeLoadi (UINT dest, UINT32 imm)
{
	IR * ir = NewInst();

	IR_OPERATION(ir)	= IR_LOADI;
	IR_OPERAND_DEST(ir)	= dest;
	IR_OPERAND_SRC3(ir)	= imm;
	IR_CONSTANT(ir)		= 3;

	SetUpDFlowVectors(ir);

	SetConstant(dest, imm);
}

/*
 * void IR_EncodeMove (UINT dest, UINT src);
 */

void IR_EncodeMove (UINT dest, UINT src)
{
	if (IsConstant(src))
	{
		IR_EncodeLoadi(dest, GetConstant(src));
	}
	else
	{
		IR * ir = NewInst();

		IR_OPERATION(ir)	= IR_MOVE;
		IR_OPERAND_DEST(ir)	= dest;
		IR_OPERAND_SRC1(ir)	= src;

		SetUpDFlowVectors(ir);

		ResetConstant(dest);
	}
}

/*
 * void IR_EncodeAdd (UINT dest, UINT src1, UINT src2);
 */

void IR_EncodeAdd (UINT dest, UINT src1, UINT src2)
{
	IR_EncodeALU_D_S_S(IR_ADD, dest, src1, src2, DoAdd);
}

/*
 * void IR_EncodeAddi (UINT dest, UINT src, UINT32 imm);
 */

void IR_EncodeAddi (UINT dest, UINT src, UINT32 imm)
{
	IR_EncodeALU_D_S_I(IR_ADD, dest, src, imm, DoAdd);
}

/*
 * void IR_EncodeSub (UINT dest, UINT src1, UINT src2);
 */

void IR_EncodeSub (UINT dest, UINT src1, UINT src2)
{
	IR_EncodeALU_D_S_S(IR_SUB, dest, src1, src2, DoSub);
}

/*
 * void IR_EncodeNeg (UINT dest, UINT src);
 */

void IR_EncodeNeg (UINT dest, UINT src)
{
	IR_EncodeALU_D_S(IR_NEG, dest, src, DoNeg);
}

/*
 * void IR_EncodeMulu (UINT dest, UINT src1, UINT src2);
 */

void IR_EncodeMulu (UINT dest, UINT src1, UINT src2)
{
	IR_EncodeALU_D_S_S(IR_MULU, dest, src1, src2, DoMulu);
}

/*
 * void IR_EncodeMului (UINT dest, UINT src, UINT32 imm);
 */

void IR_EncodeMului (UINT dest, UINT src, UINT32 imm)
{
	IR_EncodeALU_D_S_I(IR_MULU, dest, src, imm, DoMulu);
}

/*
 * void IR_EncodeAnd (UINT dest, UINT src1, UINT src2);
 */

void IR_EncodeAnd (UINT dest, UINT src1, UINT src2)
{
	IR_EncodeALU_D_S_S(IR_AND, dest, src1, src2, DoAnd);
}

/*
 * void IR_EncodeAndi (UINT dest, UINT src1, UINT32 imm);
 */

void IR_EncodeAndi (UINT dest, UINT src, UINT32 imm)
{
	IR_EncodeALU_D_S_I(IR_AND, dest, src, imm, DoAnd);
}

/*
 * void IR_EncodeOr (UINT dest, UINT src1, UINT src2);
 */

void IR_EncodeOr (UINT dest, UINT src1, UINT src2)
{
	IR_EncodeALU_D_S_S(IR_OR, dest, src1, src2, DoOr);
}

/*
 * void IR_EncodeOri (UINT dest, UINT src1, UINT32 imm);
 */

void IR_EncodeOri (UINT dest, UINT src, UINT32 imm)
{
	IR_EncodeALU_D_S_I(IR_OR, dest, src, imm, DoOr);
}

/*
 * void IR_EncodeXor (UINT dest, UINT src1, UINT src2);
 */

void IR_EncodeXor (UINT dest, UINT src1, UINT src2)
{
	IR_EncodeALU_D_S_S(IR_XOR, dest, src1, src2, DoXor);
}

/*
 * void IR_EncodeXori (UINT dest, UINT src, UINT32 imm);
 */

void IR_EncodeXori (UINT dest, UINT src, UINT32 imm)
{
	IR_EncodeALU_D_S_I(IR_XOR, dest, src, imm, DoXor);
}

/*
 * void IR_EncodeNot (UINT dest, UINT src);
 */

void IR_EncodeNot (UINT dest, UINT src)
{
	IR_EncodeALU_D_S(IR_NOT, dest, src, DoNot);
}

/*
 * void IR_EncodeRoli (UINT dest, UINT src, UINT32 sa);
 */

void IR_EncodeRoli (UINT dest, UINT src, UINT32 sa)
{
	IR_EncodeALU_D_S_I(IR_ROL, dest, src, sa, DoRol);
}

/*
 * void IR_EncodeShli (UINT dest, UINT src, UINT32 sa);
 */

void IR_EncodeShli (UINT dest, UINT src, UINT32 sa)
{
	IR_EncodeALU_D_S_I(IR_SHL, dest, src, sa, DoShl);
}

/*
 * void IR_EncodeShri (UINT dest, UINT src, UINT32 sa);
 */

void IR_EncodeShri (UINT dest, UINT src, UINT32 sa)
{
	IR_EncodeALU_D_S_I(IR_SHR, dest, src, sa, DoShr);
}

/*
 * void IR_EncodeBrev (UINT dest, UINT src, IR_SIZE size);
 */

void IR_EncodeBrev (UINT dest, UINT src, IR_SIZE size)
{
	ASSERT(size == IR_REV_8_IN_16 || size == IR_REV_8_IN_32);

	if (IsConstant(src))
	{
		UINT32	res;

		src = GetConstant(src);

		if (size == IR_REV_8_IN_16)
			res = DoReverse8In16(src);
		else
			res = DoReverse8In32(src);

		IR_EncodeLoadi(dest, res);
	}
	else
	{
		IR * ir = NewInst();

		IR_OPERATION(ir)		= IR_BREV;
		IR_SIZE(ir)				= size;
		IR_OPERAND_DEST(ir)		= dest;
		IR_OPERAND_SRC1(ir)		= src;

		SetUpDFlowVectors(ir);

		ResetConstant(dest);
	}
}

/*
 * void IR_EncodeCmp (IR_CONDITION cont, UINT dest, UINT src1, UINT src2);
 */

void IR_EncodeCmp (IR_CONDITION cond, UINT dest, UINT src1, UINT src2)
{
	if (IsConstant(src1) && IsConstant(src2))
	{
		BOOL	res;

		src1 = GetConstant(src1);
		src2 = GetConstant(src2);

		res = DoCompare(cond, src1, src2);

		IR_EncodeLoadi(dest, res);
	}
	else
	{
		IR	* ir = NewInst();

		IR_OPERATION(ir)	= IR_CMP;
		IR_CONDITION(ir)	= cond;
		IR_OPERAND_DEST(ir)	= dest;

		if (IsConstant(src1))
		{
			IR_OPERAND_SRC1(ir)	= GetConstant(src1);
			IR_CONSTANT(ir)		= 1;
		}
		else
		{
			ASSERT(!IS_F_REG(src1));
			IR_OPERAND_SRC1(ir)	= src1;
		}

		if (IsConstant(src2))
		{
			IR_OPERAND_SRC2(ir)	= GetConstant(src2);
			IR_CONSTANT(ir)		= 2;
		}
		else
		{
			ASSERT(!IS_F_REG(src2));
			IR_OPERAND_SRC2(ir)	= src2;
		}

		SetUpDFlowVectors(ir);

		ResetConstant(dest);
	}
}

/*
 * void IR_EncodeCmpi (IR_CONDITION cond, UINT dest, UINT src, UINT32 imm);
 */

void IR_EncodeCmpi (IR_CONDITION cond, UINT dest, UINT src, UINT32 imm)
{
	if (IsConstant(src))
	{
		BOOL res;

		src = GetConstant(src);
		res = DoCompare(cond, src, imm);

		IR_EncodeLoadi(dest, res);
	}
	else
	{
		IR * ir = NewInst();

		ASSERT(cond < NUM_IR_CONDITIONS);

		ASSERT(!IS_F_REG(src));

		IR_OPERATION(ir)		= IR_CMP;
		IR_CONDITION(ir)		= cond;
		IR_OPERAND_DEST(ir)		= dest;
		IR_OPERAND_SRC1(ir)		= src;
		IR_OPERAND_SRC2(ir)		= imm;
		IR_CONSTANT(ir)			= 2;

		SetUpDFlowVectors(ir);

		ResetConstant(dest);
	}
}

/*
 * void IR_EncodeConvert (IR_SIZE size, UINT dest, UINT src);
 */

void IR_EncodeConvert (IR_SIZE size, UINT dest, UINT src)
{
	if (IsConstant(src))
	{
		ASSERT(0);
	}
	else
	{
		IR * ir = NewInst();

		IR_OPERATION(ir)		= IR_CONVERT;
		IR_SIZE(ir)				= size;
		IR_OPERAND_DEST(ir)		= dest;
		IR_OPERAND_SRC1(ir)		= src;

		SetUpDFlowVectors(ir);

		ResetConstant(dest);
	}
}

/*
 * void IR_EncodeLoadX (UINT dest, UINT base, UINT32 offs);
 */

void IR_EncodeLoad8 (UINT dest, UINT base, UINT32 offs)
{
	IR_EncodeLoad(IR_INT8, dest, base, offs);
}

void IR_EncodeLoad16 (UINT dest, UINT base, UINT32 offs)
{
	IR_EncodeLoad(IR_INT16, dest, base, offs);
}

void IR_EncodeLoad32 (UINT dest, UINT base, UINT32 offs)
{
	IR_EncodeLoad(IR_INT32, dest, base, offs);
}

void IR_EncodeLoad64 (UINT dest, UINT base, UINT32 offs)
{
	IR_EncodeLoad(IR_INT64, dest, base, offs);
}

/*
 * void IR_EncodeStoreX (UINT src, UINT base, UINT32 offs);
 */

void IR_EncodeStore8 (UINT src, UINT base, UINT32 offs)
{
	IR_EncodeStore(IR_INT8, src, base, offs);
}

void IR_EncodeStore16 (UINT src, UINT base, UINT32 offs)
{
	IR_EncodeStore(IR_INT16, src, base, offs);
}

void IR_EncodeStore32 (UINT src, UINT base, UINT32 offs)
{
	IR_EncodeStore(IR_INT32, src, base, offs);
}

void IR_EncodeStore64 (UINT src, UINT base, UINT32 offs)
{
	IR_EncodeStore(IR_INT64, src, base, offs);
}

/*
 * void IR_EncodeLoadPtrX (UINT src, UINT32 ptr);
 */

void IR_EncodeLoadPtr32 (UINT dest, UINT32 ptr)
{
	IR * ir = NewInst();

	IR_OPERATION(ir)	= IR_LOADPTR;
	IR_OPERAND_DEST(ir)	= dest;
	IR_OPERAND_SRC3(ir)	= ptr;
	IR_CONSTANT(ir)		= 3;
	IR_SIZE(ir)			= IR_INT32;

	// TEMP
	{
		SetDFlowOutBit(ir, dest);
	}

	ResetConstant(dest);
}

/*
 * void IR_EncodeStorePtrX (UINT src, UINT32 ptr);
 */

void IR_EncodeStorePtr32 (UINT src, UINT32 ptr)
{
	IR	* ir = NewInst();

	IR_OPERATION(ir)	= IR_STOREPTR;

	if (IsConstant(src))
	{
		IR_OPERAND_SRC1(ir)	= GetConstant(src);
		IR_DATA_CONST(ir)	= TRUE;
	}
	else
	{
		IR_OPERAND_SRC1(ir)	= src;
		IR_DATA_CONST(ir)	= FALSE;
	}

	IR_OPERAND_SRC3(ir)	= ptr;
	IR_CONSTANT(ir)		= 3;
	IR_SIZE(ir)			= IR_INT32;

	// TEMP
	{
		if (!IR_DATA_CONST(ir))
			SetDFlowInBit(ir, IR_OPERAND_SRC1(ir));
	}

	IR_MUST_EMIT(ir) = TRUE;
}

/*
 * void IR_EncodeCallRead (UINT32 handler, UINT dest);
 */

void IR_EncodeCallRead (UINT32 handler, UINT dest)
{
	IR * ir = NewInst();

	IR_OPERATION(ir)		= IR_CALLREAD;
	IR_OPERAND_DEST(ir)		= dest;
	IR_OPERAND_SRC3(ir)		= handler;
	IR_CONSTANT(ir)			= 3;

	SetUpDFlowVectors(ir);

	ResetConstant(dest);
}

/*
 * void IR_EncodeBranch (UINT target)
 */

void IR_EncodeBranch (UINT target)
{
	IR * ir = NewInst();

	IR_OPERATION(ir)		= IR_BRANCH;

	if (IsConstant(target))
	{
		IR_OPERAND_SRC2(ir)	= GetConstant(target);
		IR_TARGET_CONST(ir)	= TRUE;
	}
	else
	{
		IR_OPERAND_SRC2(ir)	= target;
		IR_TARGET_CONST(ir)	= FALSE;
	}

	// TEMP
	{
		if (IR_TARGET_CONST(ir) == FALSE)
			SetDFlowInBit(ir, IR_OPERAND_SRC2(ir));
	}

	IR_MUST_EMIT(ir) = TRUE;
}

/*
 * void IR_EncodeBCond (UINT flags, UINT target, UINT next);
 */

void IR_EncodeBCond (UINT flag, UINT target, UINT next)
{
	if (IsConstant(flag))
	{
		if (GetConstant(flag) != 0)
			IR_EncodeBranch(target);	// Branch always taken
		else						
			IR_EncodeBranch(next);		// Branch never taken
	}
	else
	{
		IR * ir = NewInst();

		IR_OPERATION(ir)		= IR_BCOND;
		IR_OPERAND_SRC1(ir)		= flag;

		if (IsConstant(target))
		{
			IR_OPERAND_SRC2(ir) = GetConstant(target);
			IR_TARGET_CONST(ir)	= TRUE;
		}
		else
		{
			IR_OPERAND_SRC2(ir) = target;
			IR_TARGET_CONST(ir)	= FALSE;
		}

		if (IsConstant(next))
		{
			IR_OPERAND_SRC3(ir) = GetConstant(next);
			IR_NEXT_CONST(ir)	= TRUE;
		}
		else
		{
			IR_OPERAND_SRC3(ir) = next;
			IR_NEXT_CONST(ir)	= FALSE;
		}

		IR_MUST_EMIT(ir) = TRUE;

		// TEMP
		{
			SetDFlowInBit(ir, IR_OPERAND_SRC1(ir));
			if (IR_TARGET_CONST(ir) == FALSE)
				SetDFlowInBit(ir, IR_OPERAND_SRC2(ir));
			if (IR_NEXT_CONST(ir) == FALSE)
				SetDFlowInBit(ir, IR_OPERAND_SRC3(ir));
		}
	}
}

/*
 * void IR_EncodeSync (UINT cycles);
 */

void IR_EncodeSync (UINT cycles)
{
	ASSERT(cycles >= 0);

	if (cycles > 0)
	{
		IR * ir = NewInst();

		IR_OPERATION(ir)		= IR_SYNC;
		IR_OPERAND_SRC3(ir)		= cycles;
		IR_CONSTANT(ir)			= 3;
		IR_MUST_EMIT(ir)		= TRUE;
	}
}

/*******************************************************************************
 Interface

 What it says.
*******************************************************************************/

/*
 * INT IR_Init (MEMPOOL * pool);
 *
 * Initializes the middle end. It must be called once before any other middle
 * end function is (why are you laughing at such an important comment?)
 *
 * Parameters:
 *
 *		pool		= IR instruction list memory pool.
 *
 * Returns:
 *
 *		DRPPC_OKAY.
 */

INT IR_Init (MEMPOOL * pool)
{
	cache = pool;

	BuildDefaultSearchMask();

	return DRPPC_OKAY;
}

/*
 * INT IR_BeginBB (void);
 *
 * It resets the current IR instruction cache -- that is, it resets the list
 * sentinel to be linked to itself both on prev and next, installing a NOP in
 * it.
 *
 * Parameters:
 *
 *	None.
 *
 * Returns:
 *
 *	The error code: DRPPC_OKAY if no error, otherwise another error code. At
 *	the moment it cannot fail.
 */

INT IR_BeginBB (void)
{
	UINT i;

	MemPool_Reset(cache);

	sentinel = (IR *)MemPool_Grab(cache, sizeof(IR));

	memset(sentinel, 0, sizeof(IR));

	IR_OPERATION(sentinel) = IR_NOP;

	sentinel->prev = sentinel;
	sentinel->next = sentinel;

	for (i = 0; i < NUM_DFLOW_BITS; i++)
		ResetConstant(i);

	return DRPPC_OKAY;
}

/*
 * INT IR_EndBB (IR ** ir_bb);
 *
 * Parameters:
 *
 * Returns:
 *
 * Notes:
 */

INT IR_EndBB (IR ** ir_bb)
{
#if PRINT_IR_DISASM
	printf("\n ========== Unoptimized BB ========== \n");
	PrintCurrentBB();
	printf(" ==================================== \n\n");
#endif

	RemoveDeadCode();
//	DoRegAllocPass();
//	RemoveDeadCode();

#if PRINT_IR_DISASM
	printf("\n ========== Optimized BB ========== \n");
	PrintCurrentBB();
	printf(" ================================== \n\n");
#endif

	*ir_bb = sentinel;

	return DRPPC_OKAY;
}
