/*
 * middle/ir.h
 *
 * Intermediate Representation language definitions and interface.
 */

#ifndef INCLUDED_IR_H
#define INCLUDED_IR_H

#include SOURCE_CPU_HEADER	// for DFLOW_* definitions
#include "mempool.h"

/*
 * NUM_DFLOW_WORDS
 *
 * Specifies the number of words needed to hold NUM_DFLOW_BITS bits.
 */

#if ((NUM_DFLOW_BITS & 31) == 0)
#define NUM_DFLOW_WORDS ((NUM_DFLOW_BITS / 32) + 1) // FIXME: doesn't work without the +1 !
#else
#define NUM_DFLOW_WORDS	((NUM_DFLOW_BITS / 32) + 1)
#endif

/*
 * enum IR_OP
 *
 * Enumerates all of the IR operations. Operand format is listed to the right.
 */

typedef enum
{
	/*
	 * Special
	 */

	IR_NOP,			// None
	IR_SYNC,		// call UpdateTimers [Src3] with Src1 as argument

	/*
	 * Data Computation
	 */

	IR_ADD,			// Dest = Src1 + Src2|Const
	IR_SUB,			// Dest = Src1 - Src2|Const
	IR_NEG,			// Dest = - Src1
	IR_MULS,		// Dest = Src1 * Src2|Const, signed
	IR_MULU,		// Dest = Src1 * Src2|Const, unsigned

	IR_AND,			// Dest = Src1 & Src2|Const
	IR_OR,			// Dest = Src1 | Src2|Const
	IR_XOR,			// Dest = Src1 ^ Src2|Const
	IR_NOT,

	IR_SHL,			// Dest = shift left Src1 by Src2|Const
	IR_SHR,			// Dest = shift right Src1 by Src2|Const (logical)
	IR_SAR,			// Dest = shift right Src1 by Src2|Const (aritmetical)
	IR_ROL,			// Dest = rotate left Src1 by Src2|Const
	IR_ROR,			// Dest = rotate right Src1 by Src2|Const

	IR_BREV,		// Dest = byte reverse Src1				(Modifier: size)

	IR_CMP,			// Dest = compare Src1 with Src2|Const	(Modifier: condition)

	IR_CVTSTOD,		// Dest = convert single to double Src1

	/*
	 * Data Moves
	 */

	IR_LOADI,		// Dest = Const
	IR_MOVE,		// Dest = Src

	IR_LOAD,		// Dest = [Src1+Const] (Modifier: size)
	IR_STORE,		// Src1 = [Src2+Const] (Modifier: size)

	IR_LOADPTR,		// Dest = [Ptr]	(Modifier: size)
	IR_STOREPTR,	// [Ptr] = Src2	(Modifier: size)

	IR_CALLREAD,	// Dest = Src3() (size = IR_INT32, always)

	/*
	 * Branches
	 */

	IR_BRANCH,		// branch to Src3|Const
	IR_BCOND,		// branch to Src3|Const if Src1

	/*
	 * Floating Point
	 */

	IR_CONVERT,		// convert from one FP mode to another (Modifier: size)

	NUM_IR_OPS,

} IR_OP;

/*
 * enum IR_CONDITION
 *
 * Condition codes for compare and branch instructions.
 */

typedef enum
{
    IR_CONDITION_EQ,		// Equal
    IR_CONDITION_NE,		// Not Equal
    IR_CONDITION_LTU,		// Less Than, Unsigned
    IR_CONDITION_GTU,		// Greater Than, Unsigned
    IR_CONDITION_LTS,		// Less Than, Signed
    IR_CONDITION_GTS,		// Greater Than, Signed
    IR_CONDITION_LTEU,		// Less Than or Equal, Unsigned
    IR_CONDITION_GTEU,		// Greater Than or Equal, Unsigned
    IR_CONDITION_LTES,		// Less Than or Equal, Signed
    IR_CONDITION_GTES,		// Greater Than or Equal, Signed

	NUM_IR_CONDITIONS

} IR_CONDITION;

/*
 * enum IR_SIZE
 *
 * Operand and/or instruction size specifiers. Used by loads, stores, etc.
 */

typedef enum
{
	/*
	 * Integer data-size specifiers
	 */

	IR_INT8				= 0,
	IR_INT16			= 1,
	IR_INT32			= 2,
	IR_INT64			= 3,

	/*
	 * Floating point data-size specifiers
	 */

	IR_FLOAT32			= 0,
	IR_FLOAT64			= 1,

	/*
	 * Used by IR_EXTS
	 */

	IR_EXT_8_TO_16		= 0,
	IR_EXT_8_TO_32		= 1,
	IR_EXT_16_TO_32		= 2,

	/*
	 * Used by IR_BREV
	 */

	IR_REV_8_IN_16		= 0,
	IR_REV_8_IN_32		= 1,

	/*
	 * Used by IR_CONVERT
	 */

	IR_S_TO_D			= 0,
	IR_D_TO_S			= 1,

} IR_SIZE;

/*
 * struct IR;
 *
 * This structure holds informations about a single intermediate language
 * instruction, and links to the prev and next intermediate instructions in
 * the list.
 */

typedef struct ir
{
	IR_OP		op;

	union
	{
		struct
		{
			UINT	constant	: 2;
			UINT	size		: 2;
			UINT	condition	: 4;
			UINT	data_const	: 1;
			UINT	addr_const	: 1;
			UINT	target_const: 1;
			UINT	next_const	: 1;
			UINT	must_emit	: 1;

		} fields;

		UINT32	dummy_size_specifier;

	} modifier;

	UINT32		operand[4];

	UINT32		dflow_in [NUM_DFLOW_WORDS];
	UINT32		dflow_out[NUM_DFLOW_WORDS];

	struct ir	*prev, *next;

} IR;

/*
 * Macros to access the various fields of the IR struct.
 */

#define IR_OPERATION(i)			i->op

#define IR_CONSTANT(i)			i->modifier.fields.constant
#define IR_SIZE(i)				i->modifier.fields.size
#define IR_CONDITION(i)			i->modifier.fields.condition
#define IR_DATA_CONST(i)		i->modifier.fields.data_const
#define IR_ADDR_CONST(i)		i->modifier.fields.addr_const
#define IR_TARGET_CONST(i)		i->modifier.fields.target_const
#define IR_NEXT_CONST(i)		i->modifier.fields.next_const
#define	IR_MUST_EMIT(i)			i->modifier.fields.must_emit

#define IR_OPERAND_DEST(i)		i->operand[0]
#define IR_OPERAND_SRC1(i)		i->operand[1]
#define IR_OPERAND_SRC2(i)		i->operand[2]
#define IR_OPERAND_SRC3(i)		i->operand[3]
#define IR_OPERAND_CONST(i)		i->operand[IR_CONSTANT(i)]

/*
 * Macros to classify register IDs.
 */

#define IS_FLAG_REG(r)		((r) >= DFLOW_FLAG_BASE && (r) <= DFLOW_FLAG_END)
#define IS_R_REG(r)			((r) >= DFLOW_INTEGER_BASE && (r) <= DFLOW_INTEGER_END)
#define IS_F_REG(r)			((r) >= DFLOW_FP_BASE && (r) <= DFLOW_FP_END)
#define IS_TEMP_REG(r)		((r) >= DFLOW_TEMP_BASE && (r) <= DFLOW_TEMP_END)
#define IS_NATIVE_REG(r)	((r) >= DFLOW_NATIVE_BASE && (r) <= DFLOW_NATIVE_END)

/*******************************************************************************
 Interface
*******************************************************************************/

// Main interface
extern INT	IR_Init(MEMPOOL *);
extern INT	IR_BeginBB(void);
extern INT	IR_EndBB(IR **);

// dflow[] vector manipulation
extern UINT	GetDFlowInBit(IR *, UINT);
extern UINT	GetDFlowOutBit(IR *, UINT);
extern void	SetDFlowInBit(IR *, UINT);
extern void	SetDFlowOutBit(IR *, UINT);
extern void	ClearDFlowInBit(IR *, UINT);
extern void	ClearDFlowOutBit(IR *, UINT);

// Loadi
extern void IR_EncodeLoadi(UINT, UINT32);
// Move
extern void IR_EncodeMove(UINT, UINT);
// Add
extern void IR_EncodeAdd(UINT, UINT, UINT);
extern void IR_EncodeAddi(UINT, UINT, UINT32);
// Sub
extern void IR_EncodeSub(UINT, UINT, UINT);
// Neg
extern void IR_EncodeNeg(UINT, UINT);
// Mul
extern void IR_EncodeMulu(UINT, UINT, UINT);
extern void IR_EncodeMului(UINT, UINT, UINT32);
// And
extern void IR_EncodeAnd(UINT, UINT, UINT);
extern void IR_EncodeAndi(UINT, UINT, UINT32);
// Or
extern void IR_EncodeOr(UINT, UINT, UINT);
extern void IR_EncodeOri(UINT, UINT, UINT32);
// Xor
extern void IR_EncodeXor(UINT, UINT, UINT);
extern void IR_EncodeXori(UINT, UINT, UINT32);
// Not
extern void IR_EncodeNot(UINT, UINT);
// Rol
extern void IR_EncodeRoli(UINT, UINT, UINT32);
// Shl
extern void IR_EncodeShli(UINT, UINT, UINT32);
// Shr
extern void IR_EncodeShri(UINT, UINT, UINT32);
// Brev
extern void IR_EncodeBrev(UINT, UINT, IR_SIZE);
// Cmp
extern void IR_EncodeCmp(IR_CONDITION, UINT, UINT, UINT);
extern void IR_EncodeCmpi(IR_CONDITION, UINT, UINT, UINT32);
// Convert
extern void IR_EncodeConvert(IR_SIZE, UINT, UINT);
// Load
extern void IR_EncodeLoad8(UINT, UINT, UINT32);
extern void IR_EncodeLoad16(UINT, UINT, UINT32);
extern void IR_EncodeLoad32(UINT, UINT, UINT32);
extern void IR_EncodeLoad64(UINT, UINT, UINT32);
// Store
extern void IR_EncodeStore8(UINT, UINT, UINT32);
extern void IR_EncodeStore16(UINT, UINT, UINT32);
extern void IR_EncodeStore32(UINT, UINT, UINT32);
extern void IR_EncodeStore64(UINT, UINT, UINT32);
// Load Direct
extern void IR_EncodeLoadPtr32(UINT, UINT32);
extern void IR_EncodeStorePtr32(UINT, UINT32);
// Call Handlers
extern void IR_EncodeCallRead(UINT32, UINT);
// Branch
extern void IR_EncodeBranch(UINT);
extern void IR_EncodeBCond(UINT, UINT, UINT);
// Syncronize
extern void IR_EncodeSync(UINT);

#endif // INCLUDED_IR_H
