/*
 * back/ia32/emit.c
 *
 * Intel IA-32 Opcode Emitter. Requires Emit8, Emit16, Emit32 symbols.
 */

#include "drppc.h"
#include "toplevel.h"
#include "emit.h"
#include "../ccache.h"

#ifdef _MSC_VER
#pragma warning(disable:4761)	// integral size mismatch
#pragma warning(disable:4244)	// conversion from '' to '', possible loss of data
#pragma warning(disable:4505)	// unreferenced local function has been removed
#endif

/*******************************************************************************
 Prefix, Mod R/M and SIB Emission
*******************************************************************************/

/*
 * Prefixes
 */

typedef enum
{
	PREFIX_LOCK				= 0xF0,	// Group 1
	PREFIX_REPNE			= 0xF2,	// Group 1
	PREFIX_REPE				= 0xF3,	// Group 1
	PREFIX_BRANCH_NOT_TAKEN	= 0x2E,	// Group 2, SSE2
	PREFIX_BRANCH_TAKEN		= 0x3E,	// Group 2, SSE2
	PREFIX_DATA_SIZE		= 0x66,	// Group 3
	PREFIX_ADDR_SIZE		= 0x67,	// Group 4

} X86_PREFIX;

/*
 * SIB Scale Values
 */

#define SCALE_1				0
#define SCALE_2				1
#define SCALE_4				2
#define SCALE_8				3

/*
 * Helpful Macros
 */

#define MODRM(m, r, z)	(((m & 3) << 6) | ((r & 7) << 3) | (z & 7))
#define SIB(s, i, b)	(((s & 3) << 6) | ((i & 7) << 3) | (b & 7))

/*******************************************************************************
 Opcode Format Emission
*******************************************************************************/

/*
 * Addressing Modes
 *
 *	 Mod	R/M			Meaning
 * --------------------------------------------------------------
 *    0     r32          [r32]        Indirect Register
 *           4            sib         Indirect SIB
 *           5           disp32       Indirect Absolute
 * --------------------------------------------------------------
 *	  1     r32        [r32]+disp8    Indirect Register Relative 8
 *           4          sib+disp8     Indirect SIB Relative 8
 * --------------------------------------------------------------
 *	  2     r32        [r32]+disp32   Indirect Register Relative 32
 *           4          sib+disp32    Indirect SIB Relative 32
 * --------------------------------------------------------------
 *	  3      -            reg         Direct
 * --------------------------------------------------------------
 *
 * SIB AM is encoded as:
 *
 *  [base+index*scale]
 *
 * SIB Relative AM is encoded as:
 *
 *  [base+index*scale+disp]
 *
 */

static BOOL IsINT8 (UINT32 d)
{
	return (UINT32)((INT32)d) == (UINT32)((INT32)((INT8)d));
}

static BOOL IsINT16 (UINT32 d)
{
	return (UINT32)((INT32)d) == (UINT32)((INT32)((INT16)d));
}



static void X86ShiftRotMemRegRelByImm_8 (UINT op, UINT addr, UINT disp, UINT imm)
{
	Emit8(0xC0);
	if (IsINT8(disp))
	{
		Emit8(MODRM(1, op, addr));
		Emit8((UINT8)disp);
	}
	else
	{
		Emit8(MODRM(2, op, addr));
		Emit32((UINT32)disp);
	}
	
	Emit8(imm);
}

static void X86ShiftRotMemRegRelByImm_16 (UINT op, UINT addr, UINT disp,  UINT imm)
{
	Emit8(0xC1);
	if (IsINT8(disp))
	{
		Emit8(MODRM(1, op, addr));
		Emit8((UINT8)disp);
	}
	else
	{
		Emit8(MODRM(2, op, addr));
		Emit32((UINT32)disp);
	}
	Emit8(imm);
}

static void X86ShiftRotMemRegRelByImm_32 (UINT op, UINT addr, UINT disp,  UINT imm)
{
	Emit8(0xC1);
	if (IsINT8(disp))
	{
		Emit8(MODRM(1, op, addr));
		Emit8((UINT8)disp);
	}
	else
	{
		Emit8(MODRM(2, op, addr));
		Emit32((UINT32)disp);
	}
	Emit8(imm);
}



// Op Reg

static void X86OpReg (UINT size, UINT op, UINT reg)
{
	switch(size)
	{
	case 8:
		Emit8(0xF6);
		Emit8(op | (reg & 7));
		break;
	case 16:
		Emit8(PREFIX_DATA_SIZE);
		Emit8(0xF7);
		Emit8(op | (reg & 7));
		break;
	case 32:
		Emit8(0xF7);
		Emit8(op | (reg & 7));
		break;
	default:
		ASSERT(0);
	}
}

// Op [Addr]

static void X86OpMemAbs(UINT size, UINT op, UINT addr)
{
	switch(size)
	{
	case 8:
		Emit8(0xF6);
		Emit8(MODRM(0, op, 5));
		Emit32(addr);
		break;
	case 16:
		Emit8(PREFIX_DATA_SIZE);
		Emit8(0xF7);
		Emit8(MODRM(0, op, 5));
		Emit32(addr);
		break;
	case 32:
		Emit8(0xF7);
		Emit8(MODRM(0, op, 5));
		Emit32(addr);
		break;
	default:
		ASSERT(0);
	}
}

// Op [Reg]

static void X86OpMemReg(UINT size, UINT op, UINT addr)
{
	switch(size)
	{
	case 8:
		Emit8(0xF6);
		Emit8(MODRM(0, op, addr));
		break;
	case 16:
		Emit8(PREFIX_DATA_SIZE);
		Emit8(0xF7);
		Emit8(MODRM(0, op, addr));
		break;
	case 32:
		Emit8(0xF7);
		Emit8(MODRM(0, op, addr));
		break;
	default:
		ASSERT(0);
	}
}

// Op [Reg+Disp]

static void X86OpMemRegRel(UINT size, UINT op, UINT addr, UINT disp)
{
	ASSERT(IsINT8(disp));

	switch(size)
	{
	case 8:
		Emit8(0xF6);
		Emit8(MODRM(1, op, addr));
		Emit8(disp);
		break;
	case 16:
		Emit8(PREFIX_DATA_SIZE);
		Emit8(0xF7);
		Emit8(MODRM(1, op, addr));
		Emit8(disp);
		break;
	case 32:
		Emit8(0xF7);
		Emit8(MODRM(1, op, addr));
		Emit8(disp);
		break;
	default:
		ASSERT(0);
	}
}

// ShiftRot Reg, CL

static void X86ShiftRotRegByCL(UINT size, UINT op, UINT reg)
{
	switch(size)
	{
	case 8:
		Emit8(0xD2);
		Emit8(MODRM(3, op, reg));
		break;
	case 16:
		Emit8(PREFIX_DATA_SIZE);
		Emit8(0xD3);
		Emit8(MODRM(3, op, reg));
		break;
	case 32:
		Emit8(0xD3);
		Emit8(MODRM(3, op, reg));
		break;
	default:
		ASSERT(0);
	}
}

// ShiftRot [Addr], CL

static void X86ShiftRotMemAbsByCL(UINT size, UINT op, UINT addr)
{
	switch(size)
	{
	case 8:
		Emit8(0xD2);
		Emit8(MODRM(0, op, 5));
		Emit32(addr);
		break;
	case 16:
		Emit8(PREFIX_DATA_SIZE);
		Emit8(0xD3);
		Emit8(MODRM(0, op, 5));
		Emit32(addr);
		break;
	case 32:
		Emit8(0xD3);
		Emit8(MODRM(0, op, 5));
		Emit32(addr);
		break;
	default:
		ASSERT(0);
	}
}

// ShiftRot [Reg], CL

static void X86ShiftRotMemRegByCL(UINT size, UINT op, UINT addr)
{
	switch(size)
	{
	case 8:
		Emit8(0xD2);
		Emit8(MODRM(0, op, addr));
		break;
	case 16:
		Emit8(PREFIX_DATA_SIZE);
		Emit8(0xD3);
		Emit8(MODRM(0, op, addr));
		break;
	case 32:
		Emit8(0xD3);
		Emit8(MODRM(0, op, addr));
		break;
	default:
		ASSERT(0);
	}
}

// ShiftRot Reg, 1

static void X86ShiftRotRegBy1_8(UINT op, UINT reg)
{
	Emit8(0xD0);
	Emit8(MODRM(3, op, reg));
}

static void X86ShiftRotRegBy1_16(UINT op, UINT reg)
{
	Emit8(PREFIX_DATA_SIZE);
	Emit8(0xD1);
	Emit8(MODRM(3, op, reg));
}

static void X86ShiftRotRegBy1_32(UINT op, UINT reg)
{
	Emit8(0xD1);
	Emit8(MODRM(3, op, reg));
}

// ShiftRot [Addr], 1

static void X86ShiftRotMemAbsBy1(UINT size, UINT op, UINT addr)
{
	switch(size)
	{
	case 8:
		Emit8(0xD0);
		Emit8(MODRM(0, op, 5));
		Emit32(addr);
		break;
	case 16:
		Emit8(PREFIX_DATA_SIZE);
		Emit8(0xD1);
		Emit8(MODRM(0, op, 5));
		Emit32(addr);
		break;
	case 32:
		Emit8(0xD1);
		Emit8(MODRM(0, op, 5));
		Emit32(addr);
		break;
	default:
		ASSERT(0);
	}
}

// ShiftRot [Reg], 1

static void X86ShiftRotMemRegBy1(UINT size, UINT op, UINT addr)
{
	switch(size)
	{
	case 8:
		Emit8(0xD0);
		Emit8(MODRM(0, op, addr));
		break;
	case 16:
		Emit8(PREFIX_DATA_SIZE);
		Emit8(0xD1);
		Emit8(MODRM(0, op, addr));
		break;
	case 32:
		Emit8(0xD1);
		Emit8(MODRM(0, op, addr));
		break;
	default:
		ASSERT(0);
	}
}

static void X86ShiftRotMemRegRelBy1_8(UINT op, UINT addr, UINT disp)
{
	Emit8(0xD0);
	if (IsINT8(disp))
	{
		Emit8(MODRM(1, op, addr));
		Emit8((UINT8)disp);
	}
	else
	{
		Emit8(MODRM(2, op, addr));
		Emit32(disp);
	}
}

static void X86ShiftRotMemRegRelBy1_16(UINT op, UINT addr, UINT disp)
{
	Emit8(PREFIX_DATA_SIZE);
	Emit8(0xD1);
	if (IsINT8(disp))
	{
		Emit8(MODRM(1, op, addr));
		Emit8((UINT8)disp);
	}
	else
	{
		Emit8(MODRM(2, op, addr));
		Emit32(disp);
	}
}

static void X86ShiftRotMemRegRelBy1_32(UINT op, UINT addr, UINT disp)
{
	Emit8(0xD1);
	if (IsINT8(disp))
	{
		Emit8(MODRM(1, op, addr));
		Emit8((UINT8)disp);
	}
	else
	{
		Emit8(MODRM(2, op, addr));
		Emit32(disp);
	}
}

// ShiftRot Reg, Imm

static void X86ShiftRotRegByImm(UINT size, UINT op, UINT reg, UINT imm)
{
	switch(size)
	{
	case 8:
		Emit8(0xC0);
		Emit8(MODRM(3, op, reg));
		Emit8(imm);
		break;
	case 16:
		Emit8(PREFIX_DATA_SIZE);
		Emit8(0xC1);
		Emit8(MODRM(3, op, reg));
		Emit8(imm);
		break;
	case 32:
		Emit8(0xC1);
		Emit8(MODRM(3, op, reg));
		Emit8(imm);
		break;
	default:
		ASSERT(0);
	}
}

// ShiftRot [Addr], Imm

static void X86ShiftRotMemAbsByImm(UINT size, UINT op, UINT addr, UINT imm)
{
	switch(size)
	{
	case 8:
		Emit8(0xC0);
		Emit8(MODRM(0, op, 5));
		Emit32(addr);
		Emit8(imm);
		break;
	case 16:
		Emit8(PREFIX_DATA_SIZE);
		Emit8(0xC1);
		Emit8(MODRM(0, op, 5));
		Emit32(addr);
		Emit8(imm);
		break;
	case 32:
		Emit8(0xC1);
		Emit8(MODRM(0, op, 5));
		Emit32(addr);
		Emit8(imm);
		break;
	default:
		ASSERT(0);
	}
}

// ShiftRot [Reg], Imm

static void X86ShiftRotMemRegByImm(UINT size, UINT op, UINT addr, UINT imm)
{
	switch(size)
	{
	case 8:
		Emit8(0xC0);
		Emit8(MODRM(0, op, addr));
		Emit8(imm);
		break;
	case 16:
		Emit8(0xC1);
		Emit8(MODRM(0, op, addr));
		Emit8(imm);
		break;
	case 32:
		Emit8(0xC1);
		Emit8(MODRM(0, op, addr));
		Emit8(imm);
		break;
	default:
		ASSERT(0);
	}
}

// Op [Addr], Reg	(0)
// Op Reg, [Addr]	(2)

static void X86OpRegToMemAbs(UINT size, UINT op, UINT reg, UINT addr)
{
	switch(size)
	{
	case 8:
		Emit8(op);
		Emit8(MODRM(0, reg, 5));
		Emit32(addr);
		break;
	case 16:
		Emit8(PREFIX_DATA_SIZE);
		Emit8(op | 1);
		Emit8(MODRM(0, reg, 5));
		Emit32(addr);
		break;
	case 32:
		Emit8(op | 1);
		Emit8(MODRM(0, reg, 5));
		Emit32(addr);
		break;
	default:
		ASSERT(0);
	}
}

// Op [Reg], Reg	(0)
// Op Reg, [Reg]	(2)

static void X86OpRegToMemReg(UINT size, UINT op, UINT reg, UINT target)
{
	switch(size)
	{
	case 8:
		Emit8(op);
		Emit8(MODRM(0, reg, target));
		break;
	case 16:
		Emit8(PREFIX_DATA_SIZE);
		Emit8(op | 1);
		Emit8(MODRM(0, reg, target));
		break;
	case 32:
		Emit8(op | 1);
		Emit8(MODRM(0, reg, target));
		break;
	default:
		ASSERT(0);
	}
}

// Op [Base+Index*Scale], Reg	(0)
// Op Reg, [Base+Index*Scale]	(2)

static void X86OpRegToMemSib(UINT size, UINT op, UINT reg, UINT sib)
{
	switch(size)
	{
	case 8:
		Emit8(op);
		Emit8(MODRM(0, reg, 4));
		Emit8(sib);
		break;
	case 16:
		Emit8(PREFIX_DATA_SIZE);
		Emit8(op | 1);
		Emit8(MODRM(0, reg, 4));
		Emit8(sib);
		break;
	case 32:
		Emit8(op | 1);
		Emit8(MODRM(0, reg, 4));
		Emit8(sib);
		break;
	default:
		ASSERT(0);
	}
}

// Op [Reg+Disp], Reg	(0)
// Op Reg, [Reg+Disp]	(2)

static void X86OpRegToMemRegRel(UINT size, UINT op, UINT reg, UINT target, UINT disp)
{
/*	if(disp == 0)
		X86OpRegToMemReg(size, op, reg, target);
	else
*/
	{
		if(IsINT8(disp))	// 8-bit displacement
		{
			switch(size)
			{
			case 8:
				ASSERT(0);
				Emit8(op);
				Emit8(MODRM(1, reg, target));
				Emit8(disp);
				break;
			case 16:
				ASSERT(0);
				Emit8(PREFIX_DATA_SIZE);
				Emit8(op | 1);
				Emit8(MODRM(1, reg, target));
				Emit8(disp);
				break;
			case 32:
				Emit8(op | 1);
				Emit8(MODRM(1, reg, target));
				Emit8(disp);
				break;
			default:
				ASSERT(0);
			}
		}
		else				// 32-bit displacement
		{
			switch(size)
			{
			case 8:
				ASSERT(0);
				Emit8(op);
				Emit8(MODRM(2, reg, target));
				Emit32(disp);
				break;
			case 16:
				ASSERT(0);
				Emit8(PREFIX_DATA_SIZE);
				Emit8(op | 1);
				Emit8(MODRM(2, reg, target));
				Emit32(disp);
				break;
			case 32:
				ASSERT(0);
				Emit8(op | 1);
				Emit8(MODRM(2, reg, target));
				Emit32(disp);
				break;
			default:
				ASSERT(0);
			}
		}
	}
}

// Op [Base+Index*Scale+Disp], Reg	(0)
// Op Reg, [Base+Index*Scale+Disp]	(2)

static void X86OpRegToMemSibRel(UINT size, UINT op, UINT reg, UINT sib, UINT disp)
{
/*	if(disp == 0)
		X86OpRegToMemSib(size, op, reg, sib);
	else
*/
	{
		if(IsINT8(disp))	// 8-bit displacement
		{
			switch(size)
			{
			case 8:
				Emit8(op);
				Emit8(MODRM(1, reg, 4));
				Emit8(sib);
				Emit8(disp);
				break;
			case 16:
				Emit8(PREFIX_DATA_SIZE);
				Emit8(op | 1);
				Emit8(MODRM(1, reg, 4));
				Emit8(sib);
				Emit8(disp);
				break;
			case 32:
				Emit8(op | 1);
				Emit8(MODRM(1, reg, 4));
				Emit8(sib);
				Emit8(disp);
				break;
			default:
				ASSERT(0);
			}
		}
		else				// 32-bit displacement
		{
			switch(size)
			{
			case 8:
				Emit8(op);
				Emit8(MODRM(2, reg, 4));
				Emit8(sib);
				Emit32(disp);
				break;
			case 16:
				Emit8(PREFIX_DATA_SIZE);
				Emit8(op | 1);
				Emit8(MODRM(2, reg, 4));
				Emit8(sib);
				Emit32(disp);
				break;
			case 32:
				Emit8(op | 1);
				Emit8(MODRM(2, reg, 4));
				Emit8(sib);
				Emit32(disp);
				break;
			default:
				ASSERT(0);
			}
		}
	}
}

// Op [Addr], Imm

static void X86OpImmToMemAbs(UINT size, UINT op, UINT addr, UINT imm)
{
	BOOL	cond = IsINT8(imm);

	switch(size)
	{
	case 8:
		Emit8(0x80);
		Emit8(MODRM(0, op, 5));
		Emit32(addr);
		Emit8(imm);
		break;
	case 16:
		Emit8(PREFIX_DATA_SIZE);
		Emit8(0x81 | (cond ? 2 : 0));
		Emit8(MODRM(0, op, 5));
		Emit32(addr);
		if(cond)
			Emit8(imm);
		else
			Emit16(imm);
		break;
	case 32:
		Emit8(0x81 | (cond ? 2 : 0));
		Emit8(MODRM(0, op, 5));
		Emit32(addr);
		if(cond)
			Emit8(imm);
		else
			Emit32(imm);
		break;
	default:
		ASSERT(0);
	}
}

// Op [Reg], Imm

static void X86OpImmToMemReg(UINT size, UINT op, UINT addr, UINT imm)
{
	BOOL	cond = IsINT8(imm);

	switch(size)
	{
	case 8:
		Emit8(0x80);
		Emit8(MODRM(0, op, addr));
		Emit8(imm);
		break;
	case 16:
		Emit8(PREFIX_DATA_SIZE);
		Emit8(0x81 | (cond ? 2 : 0));
		Emit8(MODRM(0, op, addr));
		if(cond)
			Emit8(imm);
		else
			Emit16(imm);
		break;
	case 32:
		Emit8(0x81 | (cond ? 2 : 0));
		Emit8(MODRM(0, op, addr));
		if(cond)
			Emit8(imm);
		else
			Emit32(imm);
		break;
	default:
		ASSERT(0);
	}
}

// OP [Reg+Disp], Imm

static void X86OpImmToMemRegRel (UINT size, UINT op, UINT addr, UINT disp, UINT imm)
{
	BOOL i = IsINT8(imm);

	/*
	if (disp == 0) // Doesn't work for CMP
	{
		X86OpImmToMemReg(size, op, addr, imm);
		return;
	}
	*/

	if(IsINT8(disp))	// 8-bit displacement
	{
		switch(size)
		{
		case 8:
			Emit8(0x80 | (i ? 2 : 0));
			Emit8(MODRM(1, op, addr));
			Emit8(disp);
			break;
		case 16:
			Emit8(PREFIX_DATA_SIZE);
			Emit8(0x80 | (i ? 2 : 0) | 1);
			Emit8(MODRM(1, op, addr));
			Emit8(disp);
			break;
		case 32:
			Emit8(0x80 | (i ? 2 : 0) | 1);
			Emit8(MODRM(1, op, addr));
			Emit8(disp);
			break;
		default:
			ASSERT(0);
		}
	}
	else				// 32-bit displacement
	{
		switch(size)
		{
		case 8:
			Emit8(0x80 | (i ? 2 : 0));
			Emit8(MODRM(2, op, addr));
			Emit32(disp);
			break;
		case 16:
			Emit8(PREFIX_DATA_SIZE);
			Emit8(0x80 | (i ? 2 : 0) | 1);
			Emit8(MODRM(2, op, addr));
			Emit32(disp);
			break;
		case 32:
			Emit8(0x80 | (i ? 2 : 0) | 1);
			Emit8(MODRM(2, op, addr));
			Emit32(disp);
			break;
		default:
			ASSERT(0);
		}
	}

	if(i)
		Emit8(imm);
	else
		Emit32(imm);
}

// Op Reg,Reg

static void X86OpRegToReg_8(UINT8 op, UINT reg1, UINT reg2)
{
	Emit8(op);
	Emit8(MODRM(3, reg1, reg2));
}

static void X86OpRegToReg_16(UINT8 op, UINT reg1, UINT reg2)
{
	Emit8(PREFIX_DATA_SIZE);
	Emit8(op | 1);
	Emit8(MODRM(3, reg1, reg2));
}

static void X86OpRegToReg_32(UINT8 op, UINT reg1, UINT reg2)
{
	Emit8(op | 1);
	Emit8(MODRM(3, reg1, reg2));
}

// Op Reg, Imm

static void X86OpImmToReg_8(UINT8 op, UINT reg, UINT imm, BOOL simplify)
{
	Emit8(0x80);
	Emit8(op | (reg & 7));
	Emit8(imm);
}

static void X86OpImmToReg_16(UINT8 op, UINT reg, UINT imm, BOOL simplify)
{
	Emit8(PREFIX_DATA_SIZE);
	if(simplify && IsINT8(imm))
	{
		Emit8(0x83);
		Emit8(op | (reg & 7));
		Emit8(imm);
	}
	else
	{
		Emit8(0x81);
		Emit8(op | (reg & 7));
		Emit16(imm);
	}
}

static void X86OpImmToReg_32(UINT8 op, UINT reg, UINT imm, BOOL simplify)
{
	if(simplify && IsINT8(imm))
	{
		Emit8(0x83);
		Emit8(op | (reg & 7));
		Emit8(imm);
	}
	else
	{
		Emit8(0x81);
		Emit8(op | (reg & 7));
		Emit32(imm);
	}
}

// Op EAX/AX/AL, Imm

static void X86OpImmToAcc_8 (UINT op, UINT imm)
{
	Emit8(op);
	Emit8(imm);
}

static void X86OpImmToAcc_16 (UINT op, UINT imm)
{
	Emit8(PREFIX_DATA_SIZE);
	Emit8(op | 1);
	Emit16(imm);
}

static void X86OpImmToAcc_32 (UINT op, UINT imm)
{
	Emit8(op | 1);
	Emit32(imm);
}

/*******************************************************************************
 Integer Instructions
*******************************************************************************/

X86_EMITTER( BswapReg_32 )(UINT reg)
{
	Emit8(0x0F);
	Emit8(0xC8 | (reg & 7));
}

X86_EMITTER( JccRel8 )(UINT cc, UINT addr)
{
	UINT8 * here = GetEmitPtr();

	Emit8(0x70 | (cc & 15));
	Emit8(((UINT32)addr - (UINT32)here) - 2);
}

X86_EMITTER( JccRel32 )(UINT cc, UINT addr)
{
	UINT8 * here = GetEmitPtr();

	Emit8(0x0F);
	Emit8(0x80 | (cc & 15));
	Emit32(((UINT32)addr - (UINT32)here) - 6);
}

X86_EMITTER( SetccMemAbs_8 )(UINT cc, UINT addr)
{
	Emit8(0x0F);
	Emit8(0x90 | (cc & 15));
	Emit8(MODRM(0, 0, 5));
	Emit32(addr);
}

X86_EMITTER( SetccMemRegRel_8 )(UINT cc, UINT base, UINT offs)
{
	ASSERT(IsINT8(offs));

	Emit8(0x0F);
	Emit8(0x90 | (cc & 15));
	Emit8(MODRM(1, 0, base));
	Emit8(offs);
}

X86_EMITTER( XchgRegToReg_16 )(UINT reg1, UINT reg2)
{
	if(reg1 != reg2)
	{
		if(reg1 == EAX)			// xchg [e]ax, reg2
		{
			Emit8(PREFIX_DATA_SIZE);
			Emit8(0x90 | (reg2 & 7));
		}
		else if (reg2 == EAX)	// xchg [e]ax, reg1
		{
			Emit8(PREFIX_DATA_SIZE);
			Emit8(0x90 | (reg1 & 7));
		}
		else
			X86OpRegToReg_16(0x86, reg1, reg2);
	}
}

X86_EMITTER( XchgRegToReg_32 )(UINT reg1, UINT reg2)
{
	if(reg1 != reg2)
	{
		if(reg1 == EAX)			// xchg [e]ax, reg2
			Emit8(0x90 | (reg2 & 7));
		else if (reg2 == EAX)	// xchg [e]ax, reg1
			Emit8(0x90 | (reg1 & 7));
		else
			X86OpRegToReg_32(0x86, reg1, reg2);
	}
}

X86_EMITTER( TestRegWithReg_8 )(UINT reg1, UINT reg2)
{
	X86OpRegToReg_8(0x84, reg1, reg2);
}

X86_EMITTER( TestRegWithReg_16 )(UINT reg1, UINT reg2)
{
	X86OpRegToReg_16(0x84, reg1, reg2);
}

X86_EMITTER( TestRegWithReg_32 )(UINT reg1, UINT reg2)
{
	X86OpRegToReg_32(0x84, reg1, reg2);
}

X86_EMITTER( CmpRegToMemAbs_8 )(UINT reg, UINT addr)
{
	X86OpRegToMemAbs(8, 0x3A, reg, addr);
}

X86_EMITTER( CmpRegToMemAbs_16 )(UINT reg, UINT addr)
{
	X86OpRegToMemAbs(16, 0x3A, reg, addr);
}

X86_EMITTER( CmpRegToMemAbs_32 )(UINT reg, UINT addr)
{
	X86OpRegToMemAbs(32, 0x3A, reg, addr);
}

X86_EMITTER( CmpRegToMemRegRel_8 )(UINT reg, UINT addr, UINT offs)
{
	X86OpRegToMemRegRel(8, 0x3A, reg, addr, offs);
}

X86_EMITTER( NotMemAbs_32 )(UINT32 addr)
{
	Emit8(0xF7);
	Emit8(MODRM(0, 2, 5));
	Emit32(addr);
}

X86_EMITTER( NotMemRegRel_32 )(UINT rel, UINT8 offs)
{
	Emit8(0xF7);
	Emit8(MODRM(1, 2, rel));
	Emit8(offs);
}

X86_EMITTER( NegMemAbs_32 )(UINT32 addr)
{
	Emit8(0xF7);
	Emit8(MODRM(0, 3, 5));
	Emit32(addr);
}

X86_EMITTER( NegMemRegRel_32 )(UINT rel, UINT8 offs)
{
	Emit8(0xF7);
	Emit8(MODRM(1, 3, rel));
	Emit8(offs);
}

X86_EMITTER( CmpRegToMemRegRel_16 )(UINT reg, UINT addr, UINT offs)
{
	X86OpRegToMemRegRel(16, 0x3A, reg, addr, offs);
}

X86_EMITTER( CmpRegToMemRegRel_32 )(UINT reg, UINT addr, UINT offs)
{
	X86OpRegToMemRegRel(32, 0x3A, reg, addr, offs);
}

X86_EMITTER( CmpMemAbsToImm_8 )(UINT addr, UINT imm)
{
	X86OpImmToMemAbs(8, 7, addr, imm);
}

X86_EMITTER( CmpMemAbsToImm_16 )(UINT addr, UINT imm)
{
	X86OpImmToMemAbs(16, 7, addr, imm);
}

X86_EMITTER( CmpMemAbsToImm_32 )(UINT addr, UINT imm)
{
	X86OpImmToMemAbs(32, 7, addr, imm);
}

X86_EMITTER( CmpMemRegRelToImm_8 )(UINT addr, UINT offs, UINT imm)
{
	X86OpImmToMemRegRel(8, 7, addr, offs, imm);
}

X86_EMITTER( CmpMemRegRelToImm_16 )(UINT addr, UINT offs, UINT imm)
{
	X86OpImmToMemRegRel(16, 7, addr, offs, imm);
}

X86_EMITTER( CmpMemRegRelToImm_32 )(UINT addr, UINT offs, UINT imm)
{
	X86OpImmToMemRegRel(32, 7, addr, offs, imm);
}

X86_EMITTER( TestImmWithMemRegRel_8 )(UINT addr, UINT offs, UINT imm)
{
	ASSERT(IsINT8(offs));

	Emit8(0xF6);
	Emit8(MODRM(1, 0, addr));
	Emit8(offs);
	Emit8(imm);
}

X86_EMITTER( TestImmWithMemRegRel_16 )(UINT addr, UINT offs, UINT imm)
{
	ASSERT(IsINT8(offs));

	Emit8(PREFIX_DATA_SIZE);
	Emit8(0xF7);
	Emit8(MODRM(1, 0, addr));
	Emit8(offs);
	Emit16(imm);
}

X86_EMITTER( TestImmWithMemRegRel_32 )(UINT addr, UINT offs, UINT imm)
{
	ASSERT(IsINT8(offs));

	Emit8(0xF7);
	Emit8(MODRM(1, 0, addr));
	Emit8(offs);
	Emit32(imm);
}

X86_EMITTER( CmovccRegToReg_16 )(UINT cc, UINT dest, UINT src)
{
	Emit8(PREFIX_DATA_SIZE);
	Emit8(0x0F);
	Emit8(0x40 | (cc & 15));
	Emit8(MODRM(3, dest, src));
}

X86_EMITTER( CmovccRegToReg_32 )(UINT cc, UINT dest, UINT src)
{
	Emit8(0x0F);
	Emit8(0x40 | (cc & 15));
	Emit8(MODRM(3, dest, src));
}

// ---- And --------

X86_EMITTER( AndRegToMemAbs_8 )(UINT reg, UINT addr)
{
	X86OpRegToMemAbs(8, 0x20, reg, addr);
}

X86_EMITTER( AndRegToMemAbs_16 )(UINT reg, UINT addr)
{
	X86OpRegToMemAbs(16, 0x20, reg, addr);
}

X86_EMITTER( AndRegToMemAbs_32 )(UINT reg, UINT addr)
{
	X86OpRegToMemAbs(32, 0x20, reg, addr);
}

X86_EMITTER( AndRegToMemRegRel_8 )(UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(8, 0x20, reg, addr, disp);
}

X86_EMITTER( AndRegToMemRegRel_16 )(UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(16, 0x20, reg, addr, disp);
}

X86_EMITTER( AndRegToMemRegRel_32 )(UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(32, 0x20, reg, addr, disp);
}

X86_EMITTER( AndImmToMemAbs_8 )(UINT addr, UINT imm)
{
	X86OpImmToMemAbs(8, 4, addr, imm);
}

X86_EMITTER( AndImmToMemAbs_16 )(UINT addr, UINT imm)
{
	X86OpImmToMemAbs(16, 4, addr, imm);
}

X86_EMITTER( AndImmToMemAbs_32 )(UINT addr, UINT imm)
{
	X86OpImmToMemAbs(32, 4, addr, imm);
}

X86_EMITTER( AndImmToMemRegRel_8 )(UINT addr, UINT disp, UINT imm)
{
	X86OpImmToMemRegRel(8, 4, addr, disp, imm);
}

X86_EMITTER( AndImmToMemRegRel_16 )(UINT addr, UINT disp, UINT imm)
{
	X86OpImmToMemRegRel(16, 4, addr, disp, imm);
}

X86_EMITTER( AndImmToMemRegRel_32 )(UINT addr, UINT disp, UINT imm)
{
	X86OpImmToMemRegRel(32, 4, addr, disp, imm);
}

// ---- Add --------

X86_EMITTER( AddRegToReg_8 )(UINT dest, UINT src)
{
	X86OpRegToReg_8(0x00, src, dest);
}

X86_EMITTER( AddRegToReg_16 )(UINT dest, UINT src)
{
	X86OpRegToReg_16(0x00, src, dest);
}

X86_EMITTER( AddRegToReg_32 )(UINT dest, UINT src)
{
	X86OpRegToReg_32(0x00, src, dest);
}



X86_EMITTER( SubRegToMemAbs_8 )(UINT reg, UINT addr)
{
	X86OpRegToMemAbs(8, 0x28, reg, addr);
}

X86_EMITTER( SubRegToMemAbs_16 )(UINT reg, UINT addr)
{
	X86OpRegToMemAbs(16, 0x28, reg, addr);
}

X86_EMITTER( SubRegToMemAbs_32 )(UINT reg, UINT addr)
{
	X86OpRegToMemAbs(32, 0x28, reg, addr);
}



X86_EMITTER( SubRegToMemRegRel_8 )(UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(8, 0x28, reg, addr, disp);
}

X86_EMITTER( SubRegToMemRegRel_16 )(UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(16, 0x28, reg, addr, disp);
}

X86_EMITTER( SubRegToMemRegRel_32 )(UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(32, 0x28, reg, addr, disp);
}



X86_EMITTER( AddRegToMemAbs_8 )(UINT reg, UINT addr)
{
	X86OpRegToMemAbs(8, 0x00, reg, addr);
}

X86_EMITTER( AddRegToMemAbs_16 )(UINT reg, UINT addr)
{
	X86OpRegToMemAbs(16, 0x00, reg, addr);
}

X86_EMITTER( AddRegToMemAbs_32 )(UINT reg, UINT addr)
{
	X86OpRegToMemAbs(32, 0x00, reg, addr);
}

X86_EMITTER( AddRegToMemRegRel_8 )(UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(8, 0x00, reg, addr, disp);
}

X86_EMITTER( AddRegToMemRegRel_16 )(UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(16, 0x00, reg, addr, disp);
}

X86_EMITTER( AddRegToMemRegRel_32 )(UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(32, 0x00, reg, addr, disp);
}

X86_EMITTER( AddMemRegRelToReg_8 )(UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(8, 0x02, reg, addr, disp);
}

X86_EMITTER( AddMemRegRelToReg_16 )(UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(16, 0x02, reg, addr, disp);
}

X86_EMITTER( AddMemRegRelToReg_32 )(UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(32, 0x02, reg, addr, disp);
}

X86_EMITTER( AddImmToReg_8 )(UINT reg, UINT imm)
{
	if (reg == EAX && !IsINT8(imm))
		X86OpImmToAcc_8(0x04, imm);
	else
		X86OpImmToReg_8(0xC0, reg, imm, TRUE);
}

X86_EMITTER( AddImmToReg_16 )(UINT reg, UINT imm)
{
	if (reg == EAX && !IsINT8(imm))
		X86OpImmToAcc_16(0x04, imm);
	else
		X86OpImmToReg_16(0xC0, reg, imm, TRUE);
}

X86_EMITTER( AddImmToReg_32 )(UINT reg, UINT imm)
{
	if (reg == EAX && !IsINT8(imm))
		X86OpImmToAcc_32(0x04, imm);
	else
		X86OpImmToReg_32(0xC0, reg, imm, TRUE);
}

X86_EMITTER( AddImmToMemAbs_8 )(UINT addr, UINT imm)
{
	X86OpImmToMemAbs(8, 0, addr, imm);
}

X86_EMITTER( AddImmToMemAbs_16 )(UINT addr, UINT imm)
{
	X86OpImmToMemAbs(16, 0, addr, imm);
}

X86_EMITTER( AddImmToMemAbs_32 )(UINT addr, UINT imm)
{
	X86OpImmToMemAbs(32, 0, addr, imm);
}

X86_EMITTER( AddImmToMemReg_8 )(UINT addr, UINT imm)
{
	X86OpImmToMemReg(8, 0, addr, imm);
}

X86_EMITTER( AddImmToMemReg_16 )(UINT addr, UINT imm)
{
	X86OpImmToMemReg(16, 0, addr, imm);
}

X86_EMITTER( AddImmToMemReg_32 )(UINT addr, UINT imm)
{
	X86OpImmToMemReg(32, 0, addr, imm);
}

X86_EMITTER( AddImmToMemRegRel_8 )(UINT addr, UINT disp, UINT imm)
{
	X86OpImmToMemRegRel(8, 0, addr, disp, imm);
}

X86_EMITTER( AddImmToMemRegRel_16 )(UINT addr, UINT disp, UINT imm)
{
	X86OpImmToMemRegRel(16, 0, addr, disp, imm);
}

X86_EMITTER( AddImmToMemRegRel_32 )(UINT addr, UINT disp, UINT imm)
{
	X86OpImmToMemRegRel(32, 0, addr, disp, imm);
}

// ----- Mul -----

X86_EMITTER( MulMemAbs_8 )(UINT addr)
{
	X86OpMemAbs(8, 4, addr);
}

X86_EMITTER( MulMemAbs_16 )(UINT addr)
{
	X86OpMemAbs(16, 4, addr);
}

X86_EMITTER( MulMemAbs_32 )(UINT addr)
{
	X86OpMemAbs(32, 4, addr);
}

X86_EMITTER( MulMemRegRel_8 )(UINT addr, UINT disp)
{
	X86OpMemRegRel(8, 4, addr, disp);
}

X86_EMITTER( MulMemRegRel_16 )(UINT addr, UINT disp)
{
	X86OpMemRegRel(16, 4, addr, disp);
}

X86_EMITTER( MulMemRegRel_32 )(UINT addr, UINT disp)
{
	X86OpMemRegRel(32, 4, addr, disp);
}

X86_EMITTER( XchgRegToReg_8 )(UINT reg1, UINT reg2)
{
	//if(reg1 != reg2)
		X86OpRegToReg_8(0x86, reg1, reg2);
}

// ----- Or -----

/*

X86_EMITTER( OrRegToReg_8 )(UINT reg1, UINT reg2)
{
	X86OpRegToReg_8(0x08, reg1, reg2);
}

X86_EMITTER( OrRegToReg_16 )(UINT reg1, UINT reg2)
{
	X86OpRegToReg_16(0x08, reg1, reg2);
}

X86_EMITTER( OrRegToReg_32 )(UINT reg1, UINT reg2)
{
	X86OpRegToReg_32(0x08, reg1, reg2);
}

*/

X86_EMITTER( OrRegToMemAbs_8 )(UINT reg, UINT addr)
{
	X86OpRegToMemAbs(8, 0x08, reg, addr);
}

X86_EMITTER( OrRegToMemAbs_16 )(UINT reg, UINT addr)
{
	X86OpRegToMemAbs(16, 0x08, reg, addr);
}

X86_EMITTER( OrRegToMemAbs_32 )(UINT reg, UINT addr)
{
	X86OpRegToMemAbs(32, 0x08, reg, addr);
}

X86_EMITTER( OrRegToMemRegRel_8 )(UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(8, 0x08, reg, addr, disp);
}

X86_EMITTER( OrRegToMemRegRel_16 )(UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(16, 0x08, reg, addr, disp);
}

X86_EMITTER( OrRegToMemRegRel_32 )(UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(32, 0x08, reg, addr, disp);
}

/*

X86_EMITTER( OrMemAbsToReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemAbs(size, 0x0A, reg, addr);
}

X86_EMITTER( OrMemAbsToReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemAbs(size, 0x0A, reg, addr);
}

X86_EMITTER( OrMemAbsToReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemAbs(size, 0x0A, reg, addr);
}

*/

/*

X86_EMITTER( OrImmToReg )(UINT size, UINT reg, UINT imm)
{
	if(reg == EAX && !IsINT8(imm))
		X86OpImmToAcc(size, 0x0C, imm);
	else
		X86OpImmToReg(size, 0xC8, reg, imm, TRUE);
}

*/

X86_EMITTER( OrImmToMemAbs_8 )(UINT addr, UINT imm)
{
	X86OpImmToMemAbs(8, 1, addr, imm);
}

X86_EMITTER( OrImmToMemAbs_16 )(UINT addr, UINT imm)
{
	X86OpImmToMemAbs(16, 1, addr, imm);
}

X86_EMITTER( OrImmToMemAbs_32 )(UINT addr, UINT imm)
{
	X86OpImmToMemAbs(32, 1, addr, imm);
}

X86_EMITTER( OrImmToMemRegRel_8 )(UINT addr, UINT disp, UINT imm)
{
	X86OpImmToMemRegRel(8, 1, addr, disp, imm);
}

X86_EMITTER( OrImmToMemRegRel_16 )(UINT addr, UINT disp, UINT imm)
{
	X86OpImmToMemRegRel(16, 1, addr, disp, imm);
}

X86_EMITTER( OrImmToMemRegRel_32 )(UINT addr, UINT disp, UINT imm)
{
	X86OpImmToMemRegRel(32, 1, addr, disp, imm);
}

// ----- Pop -----

X86_EMITTER( PopReg_16 )(UINT reg)
{
	Emit8(PREFIX_DATA_SIZE);
	Emit8(0x58 | (reg & 7));
}

X86_EMITTER( PopReg_32 )(UINT reg)
{
		Emit8(0x58 | (reg & 7));
}

X86_EMITTER( PopMemAbs_16 )(UINT addr)
{
	Emit8(PREFIX_DATA_SIZE);
	Emit8(0x8F);
	Emit8(MODRM(0, 0, 5));
	Emit32(addr);
}

X86_EMITTER( PopMemAbs_32 )(UINT addr)
{
	Emit8(0x8F);
	Emit8(MODRM(0, 0, 5));
	Emit32(addr);
}

X86_EMITTER( PopMemRegRel_16 )(UINT reg, UINT offs)
{
	Emit8(PREFIX_DATA_SIZE);
	Emit8(0x8F);
	Emit8(MODRM(IsINT8(offs) ? 1 : 2, 0, reg));
	IsINT8(offs) ? Emit8(offs) : Emit32(offs);
}

X86_EMITTER( PopMemRegRel_32 )(UINT reg, UINT offs)
{
	Emit8(0x8F);
	Emit8(MODRM(IsINT8(offs) ? 1 : 2, 0, reg));
	IsINT8(offs) ? Emit8(offs) : Emit32(offs);
}

X86_EMITTER( PopMemReg_16 )(UINT addr)
{
	Emit8(PREFIX_DATA_SIZE);
	Emit8(0x8F);
	Emit8(MODRM(0, 0, addr));
}

X86_EMITTER( PopMemReg_32 )(UINT addr)
{
	Emit8(PREFIX_DATA_SIZE);
	Emit8(0x8F);
	Emit8(MODRM(0, 0, addr));
}

X86_EMITTER( Popa )(void)
{
	Emit8(0x61);
}

X86_EMITTER( Pusha )(void)
{
	Emit8(0x60);
}

X86_EMITTER( PushReg_16 )(UINT reg)
{
	Emit8(PREFIX_DATA_SIZE);
	Emit8(0x50 | (reg & 7));
}

X86_EMITTER( PushReg_32 )(UINT reg)
{
	Emit8(0x50 | (reg & 7));
}

X86_EMITTER( PushImm_16 )(UINT imm)
{
	Emit8(PREFIX_DATA_SIZE);
//	if(IsINT8(imm))
//	{
//		Emit8(0x6A);
//		Emit8(imm & 0xFF);
//	}
//	else
//	{
		Emit8(0x68);
		Emit16(imm);
//	}
}

X86_EMITTER( PushImm_32 )(UINT imm)
{
//	if(IsINT8(imm))
//	{
//		Emit8(0x6A);
//		Emit8(imm & 0xFF);
//	}
//	else
//	{
		Emit8(0x68);
		Emit32(imm);
//	}
}

X86_EMITTER( PushMemRegRel_16 )(UINT reg, UINT offs)
{
	Emit8(PREFIX_DATA_SIZE);
	Emit8(0xFF);
	Emit8(MODRM(IsINT8(offs) ? 1 : 2, 6, reg));
	IsINT8(offs) ? Emit8(offs) : Emit32(offs);
}

X86_EMITTER( PushMemRegRel_32 )(UINT reg, UINT offs)
{
	Emit8(0xFF);
	Emit8(MODRM(IsINT8(offs) ? 1 : 2, 6, reg));
	IsINT8(offs) ? Emit8(offs) : Emit32(offs);
}

X86_EMITTER( PushMemAbs_16 )(UINT addr)
{
	Emit8(PREFIX_DATA_SIZE);
	Emit8(0xFF);
	Emit8(MODRM(0, 6, 5));
	Emit32(addr);
}

X86_EMITTER( PushMemAbs_32 )(UINT addr)
{
	Emit8(0xFF);
	Emit8(MODRM(0, 6, 5));
	Emit32(addr);
}

X86_EMITTER( PushMemReg_16 )(UINT addr)
{
	Emit8(PREFIX_DATA_SIZE);
	Emit8(0xFF);
	Emit8(MODRM(0, 6, addr));
}

X86_EMITTER( PushMemReg_32 )(UINT addr)
{
	Emit8(0xFF);
	Emit8(MODRM(0, 6, addr));
}

// ----- Ret -----

X86_EMITTER( Ret )(void)
{
	Emit8(0xC3);
}

// ----- Rol -----

X86_EMITTER( RolMemAbsByImm_32 )(UINT addr, UINT imm)
{
	if (imm == 1)
		X86ShiftRotMemAbsBy1(32, 0, addr);
	else
		X86ShiftRotMemAbsByImm(32, 0, addr, imm);
}

X86_EMITTER( RolMemRegRelByImm_8 )(UINT addr, UINT disp, UINT imm)
{
	if (imm == 1)
		X86ShiftRotMemRegRelBy1_8(0, addr, disp);
	else
		X86ShiftRotMemRegRelByImm_8(0, addr, disp, imm);
}

X86_EMITTER( RolMemRegRelByImm_16 )(UINT addr, UINT disp, UINT imm)
{
	if (imm == 1)
		X86ShiftRotMemRegRelBy1_16(0, addr, disp);
	else
		X86ShiftRotMemRegRelByImm_16(0, addr, disp, imm);
}

X86_EMITTER( RolMemRegRelByImm_32 )(UINT addr, UINT disp, UINT imm)
{
	if (imm == 1)
		X86ShiftRotMemRegRelBy1_32(0, addr, disp);
	else
		X86ShiftRotMemRegRelByImm_32(0, addr, disp, imm);
}

// ----- Ror -----

X86_EMITTER( RorMemAbsByImm_32 )(UINT addr, UINT imm)
{
	if (imm == 1)
		X86ShiftRotMemAbsBy1(32, 1, addr);
	else
		X86ShiftRotMemAbsByImm(32, 1, addr, imm);
}

X86_EMITTER( RorMemRegRelByImm_8 )(UINT addr, UINT disp, UINT imm)
{
	if (imm == 1)
		X86ShiftRotMemRegRelBy1_8(1, addr, disp);
	else
		X86ShiftRotMemRegRelByImm_8(1, addr, disp, imm);
}

X86_EMITTER( RorMemRegRelByImm_16 )(UINT addr, UINT disp, UINT imm)
{
	if (imm == 1)
		X86ShiftRotMemRegRelBy1_16(1, addr, disp);
	else
		X86ShiftRotMemRegRelByImm_16(1, addr, disp, imm);
}

X86_EMITTER( RorMemRegRelByImm_32 )(UINT addr, UINT disp, UINT imm)
{
	if (imm == 1)
		X86ShiftRotMemRegRelBy1_32(1, addr, disp);
	else
		X86ShiftRotMemRegRelByImm_32(1, addr, disp, imm);
}

// ----- Shl -----

X86_EMITTER( ShlMemAbsByImm_32 )(UINT addr, UINT imm)
{
	if (imm == 1)
		X86ShiftRotMemAbsBy1(32, 4, addr);
	else
		X86ShiftRotMemAbsByImm(32, 4, addr, imm);
}

X86_EMITTER( ShlMemRegRelByImm_8 )(UINT addr, UINT disp, UINT imm)
{
	if (imm == 1)
		X86ShiftRotMemRegRelBy1_8(4, addr, disp);
	else
		X86ShiftRotMemRegRelByImm_8(4, addr, disp, imm);
}

X86_EMITTER( ShlMemRegRelByImm_16 )(UINT addr, UINT disp,  UINT imm)
{
	if (imm == 1)
		X86ShiftRotMemRegRelBy1_16(4, addr, disp);
	else
		X86ShiftRotMemRegRelByImm_16(4, addr, disp, imm);
}

X86_EMITTER( ShlMemRegRelByImm_32 )(UINT addr, UINT disp,  UINT imm)
{
	if (imm == 1)
		X86ShiftRotMemRegRelBy1_32(4, addr, disp);
	else
		X86ShiftRotMemRegRelByImm_32(4, addr, disp, imm);
}

// ----- Shr -----

X86_EMITTER( ShrMemAbsByImm_32 )(UINT addr, UINT imm)
{
	if (imm == 1)
		X86ShiftRotMemAbsBy1(32, 5, addr);
	else
		X86ShiftRotMemAbsByImm(32, 5, addr, imm);
}

X86_EMITTER( ShrMemRegRelByImm_8 )(UINT addr, UINT disp, UINT imm)
{
	if (imm == 1)
		X86ShiftRotMemRegRelBy1_8(5, addr, disp);
	else
		X86ShiftRotMemRegRelByImm_8(5, addr, disp, imm);
}

X86_EMITTER( ShrMemRegRelByImm_16 )(UINT addr, UINT disp,  UINT imm)
{
	if (imm == 1)
		X86ShiftRotMemRegRelBy1_16(5, addr, disp);
	else
		X86ShiftRotMemRegRelByImm_16(5, addr, disp, imm);
}

X86_EMITTER( ShrMemRegRelByImm_32 )(UINT addr, UINT disp,  UINT imm)
{
	if (imm == 1)
		X86ShiftRotMemRegRelBy1_32(5, addr, disp);
	else
		X86ShiftRotMemRegRelByImm_32(5, addr, disp, imm);
}

/*** Mov **********************************************************************/

X86_EMITTER( MovRegToReg_8 )(UINT reg2, UINT reg1)
{
	X86OpRegToReg_8(0x88, reg1, reg2);
}

X86_EMITTER( MovRegToReg_16 )(UINT reg2, UINT reg1)
{
	X86OpRegToReg_16(0x88, reg1, reg2);
}

X86_EMITTER( MovRegToReg_32 )(UINT reg2, UINT reg1)
{
	X86OpRegToReg_32(0x88, reg1, reg2);
}

X86_EMITTER( MovImmToMemAbs_8 )(UINT addr, UINT imm)
{
	Emit8(0xC6);
	Emit8(MODRM(0, 0, 5));
	Emit32(addr);
	Emit8(imm);
}

X86_EMITTER( MovImmToMemAbs_16 )(UINT addr, UINT imm)
{
	Emit8(PREFIX_DATA_SIZE);
	Emit8(0xC7);
	Emit8(MODRM(0, 0, 5));
	Emit32(addr);
	Emit16(imm);
}

X86_EMITTER( MovImmToMemAbs_32 )(UINT addr, UINT imm)
{
	Emit8(0xC7);
	Emit8(MODRM(0, 0, 5));
	Emit32(addr);
	Emit32(imm);
}

X86_EMITTER( MovImmToMemRegRel_8 )(UINT reg, UINT offs, UINT32 imm)
{
	if (IsINT8(offs))
	{
		Emit8(0xC6);
		Emit8(MODRM(1, 0, reg));
		Emit8(offs);
		Emit8(imm);
	}
	else
	{
		Emit8(0xC6);
		Emit8(MODRM(2, 0, reg));
		Emit32(offs);
		Emit8(imm);
	}
}

X86_EMITTER( MovImmToMemRegRel_16 )(UINT reg, UINT offs, UINT32 imm)
{
	if (IsINT8(offs))
	{
		Emit8(PREFIX_DATA_SIZE);
		Emit8(0xC6|1);
		Emit8(MODRM(1, 0, reg));
		Emit8(offs);
		Emit16(imm);
	}
	else
	{
		Emit8(PREFIX_DATA_SIZE);
		Emit8(0xC6|1);
		Emit8(MODRM(2, 0, reg));
		Emit32(offs);
		Emit16(imm);
	}
}

X86_EMITTER( MovImmToMemRegRel_32 )(UINT reg, UINT offs, UINT32 imm)
{
	if (IsINT8(offs))
	{
		Emit8(0xC6|1);
		Emit8(MODRM(1, 0, reg));
		Emit8(offs);
		Emit32(imm);
	}
	else
	{
		Emit8(0xC6|1);
		Emit8(MODRM(2, 0, reg));
		Emit32(offs);
		Emit32(imm);
	}
}

X86_EMITTER( MovImmToReg_8 )(UINT reg, UINT imm)
{
	Emit8(0xB0 | (reg & 7));
	Emit8(imm);
}

X86_EMITTER( MovImmToReg_16 )(UINT reg, UINT imm)
{
	Emit8(PREFIX_DATA_SIZE);
	Emit8(0xB8 | (reg & 7));
	Emit16(imm);
}

X86_EMITTER( MovImmToReg_32 )(UINT reg, UINT imm)
{
	Emit8(0xB8 | (reg & 7));
	Emit32(imm);
}

X86_EMITTER( MovRegToMemAbs_8 )(UINT reg, UINT addr)
{
	if(reg == EAX)
	{
		Emit8(0xA2);
		Emit32(addr);
	}
	else
		X86OpRegToMemAbs(8, 0x88, reg, addr);
}

X86_EMITTER( MovRegToMemAbs_16 )(UINT reg, UINT addr)
{
	if(reg == EAX)
	{
		Emit8(PREFIX_DATA_SIZE);
		Emit8(0xA3);
		Emit32(addr);
	}
	else
		X86OpRegToMemAbs(16, 0x88, reg, addr);
}

X86_EMITTER( MovRegToMemAbs_32 )(UINT reg, UINT addr)
{
	if(reg == EAX)
	{
		Emit8(0xA3);
		Emit32(addr);
	}
	else
		X86OpRegToMemAbs(32, 0x88, reg, addr);
}

X86_EMITTER( MovMemAbsToReg_8 )(UINT reg, UINT addr)
{
	if(reg == EAX)
	{
		Emit8(0xA0);
		Emit32(addr);
	}
	else
		X86OpRegToMemAbs(8, 0x8A, reg, addr);
}

X86_EMITTER( MovMemAbsToReg_16 )(UINT reg, UINT addr)
{
	if(reg == EAX)
	{
		Emit8(PREFIX_DATA_SIZE);
		Emit8(0xA1);
		Emit32(addr);
	}
	else
		X86OpRegToMemAbs(16, 0x8A, reg, addr);
}

X86_EMITTER( MovMemAbsToReg_32 )(UINT reg, UINT addr)
{
	if(reg == EAX)
	{
		Emit8(0xA1);
		Emit32(addr);
	}
	else
		X86OpRegToMemAbs(32, 0x8A, reg, addr);
}

X86_EMITTER( MovRegToMemRegRel_8 )(UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(8, 0x88, reg, addr, disp);
}

X86_EMITTER( MovRegToMemRegRel_16 )(UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(16, 0x88, reg, addr, disp);
}

X86_EMITTER( MovRegToMemRegRel_32 )(UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(32, 0x88, reg, addr, disp);
}

X86_EMITTER( MovMemRegRelToReg_8 )(UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(8, 0x8A, reg, addr, disp);
}

X86_EMITTER( MovMemRegRelToReg_16 )(UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(16, 0x8A, reg, addr, disp);
}

X86_EMITTER( MovMemRegRelToReg_32 )(UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(32, 0x8A, reg, addr, disp);
}

/*** Movsx ********************************************************************/

X86_EMITTER( MovsxRegToReg_8To16 )(UINT reg1, UINT reg2)
{
	Emit8(PREFIX_DATA_SIZE);
	Emit8(0x0F);
	Emit8(0xBE);
	Emit8(0xC0 | ((reg2 & 7) << 3) | (reg1 & 7));
}

X86_EMITTER( MovsxRegToReg_8To32 )(UINT reg1, UINT reg2)
{
	Emit8(0x0F);
	Emit8(0xBE);
	Emit8(0xC0 | ((reg2 & 7) << 3) | (reg1 & 7));
}

X86_EMITTER( MovsxRegToReg_16To32 )(UINT reg1, UINT reg2)
{
	Emit8(0x0F);
	Emit8(0xBF);
	Emit8(0xC0 | ((reg2 & 7) << 3) | (reg1 & 7));
}

X86_EMITTER( MovsxMemAbsToReg_8To16 )(UINT reg, UINT addr)
{
	Emit8(0x0F);
	Emit8(0xBE);
	Emit8(MODRM(0, reg, 5));
	Emit32(addr);
}

X86_EMITTER( MovsxMemAbsToReg_8To32 )(UINT reg, UINT addr)
{
	Emit8(PREFIX_DATA_SIZE);
	Emit8(0x0F);
	Emit8(0xBF);
	Emit8(MODRM(0, reg, 5));
	Emit32(addr);
}

X86_EMITTER( MovsxMemAbsToReg_16To32 )(UINT reg, UINT addr)
{
	Emit8(0x0F);
	Emit8(0xBF);
	Emit8(MODRM(0, reg, 5));
	Emit32(addr);
}
/*
X86_EMITTER( MovsxMemRegRelToReg_8To16 )(UINT reg, UINT addr)
{
	Emit8(0x0F);
	Emit8(0xBE);
	Emit8(MODRM(0, reg, 5));
	Emit32(addr);
}

X86_EMITTER( MovsxMemRegRelToReg_8To32 )(UINT reg, UINT addr)
{
	Emit8(PREFIX_DATA_SIZE);
	Emit8(0x0F);
	Emit8(0xBF);
	Emit8(MODRM(0, reg, 5));
	Emit32(addr);
}

X86_EMITTER( MovsxMemRegRelToReg_16To32 )(UINT reg, UINT addr)
{
	Emit8(0x0F);
	Emit8(0xBF);
	Emit8(MODRM(0, reg, 5));
	Emit32(addr);
}
*/
/*** Movzx ********************************************************************/

X86_EMITTER( MovzxReg1ToReg2 )(UINT size, UINT reg1, UINT reg2)
{
	switch(size)
	{
	case X86_8_TO_16:
		Emit8(PREFIX_DATA_SIZE);
		Emit8(0x0F);
		Emit8(0xB6);
		Emit8(0xC0 | ((reg2 & 7) << 3) | (reg1 & 7));
		break;
	case X86_8_TO_32:
		Emit8(0x0F);
		Emit8(0xB6);
		Emit8(0xC0 | ((reg2 & 7) << 3) | (reg1 & 7));
		break;
	case X86_16_TO_32:
		Emit8(0x0F);
		Emit8(0xB7);
		Emit8(0xC0 | ((reg2 & 7) << 3) | (reg1 & 7));
		break;
	default:
		ASSERT(0);
	}
}

X86_EMITTER( MovzxMemAbsToReg )(UINT size, UINT reg, UINT addr)
{
	switch(size)
	{
	case X86_8_TO_16:
		Emit8(0x0F);
		Emit8(0xB6);
		Emit8(MODRM(0, reg, 5));
		Emit32(addr);
		break;
	case X86_8_TO_32:
		Emit8(PREFIX_DATA_SIZE);
		Emit8(0x0F);
		Emit8(0xB7);
		Emit8(MODRM(0, reg, 5));
		Emit32(addr);
		break;
	case X86_16_TO_32:
		Emit8(0x0F);
		Emit8(0xB7);
		Emit8(MODRM(0, reg, 5));
		Emit32(addr);
		break;
	default:
		ASSERT(0);
	}
}

/*** Call *********************************************************************/

X86_EMITTER( CallDirect )(UINT addr)
{
	UINT8 * here = GetEmitPtr();

	Emit8(0xE8);
	Emit32(((UINT32)addr - (UINT32)here) - 5);
}

/*** Xor **********************************************************************/

X86_EMITTER( XorRegToReg_8 )(UINT reg2, UINT reg1)
{
	X86OpRegToReg_8(0x30, reg1, reg2);
}

X86_EMITTER( XorRegToReg_16 )(UINT reg2, UINT reg1)
{
	X86OpRegToReg_16(0x30, reg1, reg2);
}

X86_EMITTER( XorRegToReg_32 )(UINT reg2, UINT reg1)
{
	X86OpRegToReg_32(0x30, reg1, reg2);
}

X86_EMITTER( XorRegToMemRegRel_32 )(UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(32, 0x30, reg, addr, disp);
}

X86_EMITTER( XorRegToMemAbs_32 )(UINT reg, UINT addr)
{
	X86OpRegToMemAbs(32, 0x30, reg, addr);
}

X86_EMITTER( XorImmToMemAbs_8 )(UINT addr, UINT imm)
{
	X86OpImmToMemAbs(8, 6, addr, imm);
}

X86_EMITTER( XorImmToMemAbs_16 )(UINT addr, UINT imm)
{
	X86OpImmToMemAbs(16, 6, addr, imm);
}

X86_EMITTER( XorImmToMemAbs_32 )(UINT addr, UINT imm)
{
	X86OpImmToMemAbs(32, 6, addr, imm);
}

X86_EMITTER( XorImmToMemRegRel_8 )(UINT addr, UINT disp, UINT imm)
{
	X86OpImmToMemRegRel(8, 6, addr, disp, imm);
}

X86_EMITTER( XorImmToMemRegRel_16 )(UINT addr, UINT disp, UINT imm)
{
	X86OpImmToMemRegRel(16, 6, addr, disp, imm);
}

X86_EMITTER( XorImmToMemRegRel_32 )(UINT addr, UINT disp, UINT imm)
{
	X86OpImmToMemRegRel(32, 6, addr, disp, imm);
}


/*

X86_EMITTER( AdcReg1ToReg2 )(UINT size, UINT reg1, UINT reg2)
{
	X86OpReg1ToReg2(size, 0x10, reg1, reg2);
}

X86_EMITTER( AdcImmToReg )(UINT size, UINT reg, UINT imm)
{
	if(reg == EAX && !IsINT8(imm))
		X86OpImmToAcc(size, 0x14, imm);
	else
		X86OpImmToReg(size, 0xD0, reg, imm, TRUE);
}

X86_EMITTER( AdcRegToMemAbs )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemAbs(size, 0x10, reg, addr);
}

X86_EMITTER( AdcMemAbsToReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemAbs(size, 0x12, reg, addr);
}

X86_EMITTER( AdcRegToMemReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemReg(size, 0x10, reg, addr);
}

X86_EMITTER( AdcMemRegToReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemReg(size, 0x12, reg, addr);
}

X86_EMITTER( AdcRegToMemRegRel )(UINT size, UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(size, 0x10, reg, addr, disp);
}

X86_EMITTER( AdcMemRegRelToReg )(UINT size, UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(size, 0x12, reg, addr, disp);
}

X86_EMITTER( AdcRegToMemSib )(UINT size, UINT reg, UINT sib)
{
	X86OpRegToMemSib(size, 0x10, reg, sib);
}

X86_EMITTER( AdcMemSibToReg )(UINT size, UINT reg, UINT sib)
{
	X86OpRegToMemSib(size, 0x12, reg, sib);
}

X86_EMITTER( AdcRegToMemSibRel )(UINT size, UINT reg, UINT sib, UINT disp)
{
	X86OpRegToMemSibRel(size, 0x10, reg, sib, disp);
}

X86_EMITTER( AdcMemSibRelToReg )(UINT size, UINT reg, UINT sib, UINT disp)
{
	X86OpRegToMemSibRel(size, 0x12, reg, sib, disp);
}

X86_EMITTER( AdcImmToMemAbs )(UINT size, UINT addr, UINT imm)
{
	X86OpImmToMemAbs(size, 2, addr, imm);
}

X86_EMITTER( AdcImmToMemReg )(UINT size, UINT addr, UINT imm)
{
	X86OpImmToMemReg(size, 2, addr, imm);
}

X86_EMITTER( AddMemAbsToReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemAbs(size, 0x02, reg, addr);
}

X86_EMITTER( AddRegToMemReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemReg(size, 0x00, reg, addr);
}

X86_EMITTER( AddMemRegToReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemReg(size, 0x02, reg, addr);
}

X86_EMITTER( AddRegToMemSib )(UINT size, UINT reg, UINT sib)
{
	X86OpRegToMemSib(size, 0x00, reg, sib);
}

X86_EMITTER( AddMemSibToReg )(UINT size, UINT reg, UINT sib)
{
	X86OpRegToMemSib(size, 0x02, reg, sib);
}

X86_EMITTER( AddRegToMemSibRel )(UINT size, UINT reg, UINT sib, UINT disp)
{
	X86OpRegToMemSibRel(size, 0x00, reg, sib, disp);
}

X86_EMITTER( AddMemSibRelToReg )(UINT size, UINT reg, UINT sib, UINT disp)
{
	X86OpRegToMemSibRel(size, 0x02, reg, sib, disp);
}

X86_EMITTER( AddImmToMemReg )(UINT size, UINT addr, UINT imm)
{
	X86OpImmToMemReg(size, 0, addr, imm);
}

X86_EMITTER( AndReg1ToReg2 )(UINT size, UINT reg1, UINT reg2)
{
	X86OpReg1ToReg2(size, 0x20, reg1, reg2);
}

X86_EMITTER( AndImmToReg )(UINT size, UINT reg, UINT imm)
{
	if(reg == EAX && !IsINT8(imm))
		X86OpImmToAcc(size, 0x24, imm);
	else
		X86OpImmToReg(size, 0xE0, reg, imm, TRUE);
}

X86_EMITTER( AndMemAbsToReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemAbs(size, 0x22, reg, addr);
}

X86_EMITTER( AndRegToMemReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemReg(size, 0x20, reg, addr);
}

X86_EMITTER( AndMemRegToReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemReg(size, 0x22, reg, addr);
}

X86_EMITTER( AndMemRegRelToReg )(UINT size, UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(size, 0x22, reg, addr, disp);
}

X86_EMITTER( AndRegToMemSib )(UINT size, UINT reg, UINT sib)
{
	X86OpRegToMemSib(size, 0x20, reg, sib);
}

X86_EMITTER( AndMemSibToReg )(UINT size, UINT reg, UINT sib)
{
	X86OpRegToMemSib(size, 0x22, reg, sib);
}

X86_EMITTER( AndRegToMemSibRel )(UINT size, UINT reg, UINT sib, UINT disp)
{
	X86OpRegToMemSibRel(size, 0x20, reg, sib, disp);
}

X86_EMITTER( AndMemSibRelToReg )(UINT size, UINT reg, UINT sib, UINT disp)
{
	X86OpRegToMemSibRel(size, 0x22, reg, sib, disp);
}

X86_EMITTER( AndImmToMemReg )(UINT size, UINT addr, UINT imm)
{
	X86OpImmToMemReg(size, 4, addr, imm);
}

X86_EMITTER( CallReg )(UINT reg)
{
	Emit8(0xFF);
	Emit8(0xD0 | (reg & 7));
}

X86_EMITTER( CallMemAbs )(UINT addr)
{
	Emit8(0xFF);
	Emit8(MODRM(0, 2, 5));
	Emit32(addr);
}

X86_EMITTER( CallMemReg )(UINT reg)
{
	Emit8(0xFF);
	Emit8(MODRM(0, 2, reg));
}

X86_EMITTER( Cbw )(void)
{
	Emit8(0x66);
	Emit8(0x98);
}

X86_EMITTER( Cdq )(void)
{
	Emit8(0x99);
}

X86_EMITTER( Clc )(void)
{
	Emit8(0xF8);
}

X86_EMITTER( CmpReg1ToReg2 )(UINT size, UINT reg1, UINT reg2)
{
	X86OpReg1ToReg2(size, 0x38, reg1, reg2);
}

X86_EMITTER( CmpRegToImm )(UINT size, UINT reg, UINT imm)
{
	if(reg == EAX && !IsINT8(imm))
		X86OpImmToAcc(size, 0x3C, imm);
	else
		X86OpImmToReg(size, 0xF8, reg, imm, TRUE);
}

X86_EMITTER( CmpMemAbsToReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemAbs(size, 0x38, reg, addr);
}

X86_EMITTER( CmpMemRegToReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemReg(size, 0x38, reg, addr);
}

X86_EMITTER( CmpRegToMemReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemReg(size, 0x3A, reg, addr);
}

X86_EMITTER( CmpMemRegRelToReg )(UINT size, UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(size, 0x38, reg, addr, disp);
}

X86_EMITTER( CmpRegToMemRegRel )(UINT size, UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(size, 0x3A, reg, addr, disp);
}

X86_EMITTER( CmpMemSibToReg )(UINT size, UINT reg, UINT sib)
{
	X86OpRegToMemSib(size, 0x38, reg, sib);
}

X86_EMITTER( CmpRegToMemSib )(UINT size, UINT reg, UINT sib)
{
	X86OpRegToMemSib(size, 0x3A, reg, sib);
}

X86_EMITTER( CmpMemSibRelToReg )(UINT size, UINT reg, UINT sib, UINT disp)
{
	X86OpRegToMemSibRel(size, 0x38, reg, sib, disp);
}

X86_EMITTER( CmpRegToMemSibRel )(UINT size, UINT reg, UINT sib, UINT disp)
{
	X86OpRegToMemSibRel(size, 0x3A, reg, sib, disp);
}

X86_EMITTER( CmovccMemAbsToReg )(UINT size, UINT cc, UINT reg, UINT addr)
{
	switch(size)
	{
	case 16:
		Emit8(PREFIX_DATA_SIZE);
	case 32:
		Emit8(0x0F);
		Emit8(0x40 | (cc & 15));
		Emit8(MODRM(0, reg, 5));
		Emit32(addr);
		break;
	default:
		ASSERT(0);
	}
}

X86_EMITTER( CmovccMemRegToReg )(UINT size, UINT cc, UINT reg, UINT addr)
{
	switch(size)
	{
	case 16:
		Emit8(PREFIX_DATA_SIZE);
	case 32:
		Emit8(0x0F);
		Emit8(0x40 | (cc & 15));
		Emit8(MODRM(0, reg, addr));
		break;
	default:
		ASSERT(0);
	}
}

X86_EMITTER( Cwd )(void)
{
	Emit8(0x66);
	Emit8(0x99);
}

X86_EMITTER( Cwde )(void)
{
	Emit8(0x98);
}

X86_EMITTER( DecReg )(UINT size, UINT reg)
{
	switch(size)
	{
	case 8:
		Emit8(0xFE);
		Emit8(0xC8 | (reg & 7));
		break;
	case 16:
		Emit8(PREFIX_DATA_SIZE);
		Emit8(0xFF);
		Emit8(0xC8 | (reg & 7));
		break;
	case 32:
		Emit8(0x48 | (reg & 7));
		break;
	default:
		ASSERT(0);
	}
}

X86_EMITTER( DecMemAbs )(UINT size, UINT addr)
{
	switch(size)
	{
	case 8:
		Emit8(0xFE);
		Emit8(MODRM(0, 1, 5));
		Emit32(addr);
		break;
	case 16:
		Emit8(PREFIX_DATA_SIZE);
		Emit8(0xFF);
		Emit8(MODRM(0, 1, 5));
		Emit32(addr);
		break;
	case 32:
		Emit8(0xFF);
		Emit8(MODRM(0, 1, 5));
		Emit32(addr);
		break;
	default:
		ASSERT(0);
	}
}

X86_EMITTER( DivReg )(UINT size, UINT reg)
{
	X86OpReg(size, 0xF0, reg);
}

X86_EMITTER( DivMemAbs )(UINT size, UINT addr)
{
	X86OpMemAbs(size, 6, addr);
}

X86_EMITTER( DivMemReg )(UINT size, UINT addr)
{
	X86OpMemReg(size, 6, addr);
}

X86_EMITTER( IdivReg )(UINT size, UINT reg)
{
	X86OpReg(size, 0xF8,reg);
}

X86_EMITTER( IdivMemAbs )(UINT size, UINT addr)
{
	X86OpMemAbs(size, 7, addr);
}

X86_EMITTER( IdivMemReg )(UINT size, UINT addr)
{
	X86OpMemReg(size, 7, addr);
}

X86_EMITTER( ImulReg )(UINT size, UINT reg)
{
	X86OpReg(size, 0xE8, reg);
}

X86_EMITTER( ImulReg1ToReg2 )(UINT size, UINT reg1, UINT reg2)
{
	if(size != 32)
		ASSERT(0);

	Emit8(0x0F);
	Emit8(0xAF);
	Emit8(MODRM(3, reg1, reg2));
}

X86_EMITTER( ImulRegToMemAbs )(UINT size, UINT reg, UINT addr)
{
	if(size != 32)
		ASSERT(0);

	Emit8(0x0F);
	Emit8(0xAF);
	Emit8(MODRM(0, reg, 5));
	Emit32(addr);
}

X86_EMITTER( ImulRegToMemReg )(UINT size, UINT reg, UINT addr)
{
	if(size != 32)
		ASSERT(0);

	Emit8(0x0F);
	Emit8(0xAF);
	Emit8(MODRM(0, reg, addr));
}

X86_EMITTER( ImulReg1WithImmToReg2 )(UINT size, UINT reg1, UINT imm, UINT reg2)
{
	switch(size)
	{
	// NASM disassembler doesn't recognize the 8-bit form
	case 32:
		Emit8(0x69);
		Emit8(MODRM(3, reg1, reg2));
		Emit32(imm);
		break;
	default:
		ASSERT(0);
	}
}

X86_EMITTER( ImulMemAbsWithImmToReg )(UINT size, UINT addr, UINT imm, UINT reg)
{
	switch(size)
	{
	// NASM disassembler doesn't recognize the 8-bit form
	case 32:
		Emit8(0x69);
		Emit8(MODRM(0, reg, 5));
		Emit32(addr);
		Emit32(imm);
		break;
	default:
		ASSERT(0);
	}
}

X86_EMITTER( ImulMemAbs )(UINT size, UINT addr)
{
	X86OpMemAbs(size, 5, addr);
}

X86_EMITTER( ImulMemReg )(UINT size, UINT addr)
{
	X86OpMemReg(size, 5, addr);
}

X86_EMITTER( IncReg )(UINT size, UINT reg)
{
	switch(size)
	{
	case 8:
		Emit8(0xFE);
		Emit8(0xC0 | (reg & 7));
		break;
	case 16:
		Emit8(PREFIX_DATA_SIZE);
		Emit8(0xFF);
		Emit8(0xC0 | (reg & 7));
		break;
	case 32:
		Emit8(0x40 | (reg & 7));
		break;
	default:
		ASSERT(0);
	}
}

X86_EMITTER( IncMemAbs )(UINT size, UINT addr)
{
	switch(size)
	{
	case 8:
		Emit8(0xFE);
		Emit8(MODRM(0, 0, 5));
		Emit32(addr);
		break;
	case 16:
		Emit8(PREFIX_DATA_SIZE);
		Emit8(0xFF);
		Emit8(MODRM(0, 0, 5));
		Emit32(addr);
		break;
	case 32:
		Emit8(0xFF);
		Emit8(MODRM(0, 0, 5));
		Emit32(addr);
		break;
	default:
		ASSERT(0);
	}
}

X86_EMITTER( JmpRel8 )(UINT addr)
{
	UINT8 * here = GetEmitPtr();

	Emit8(0xEB);
	Emit8(((UINT32)addr - (UINT32)here) - 2);
}

X86_EMITTER( JmpRel32 )(UINT addr)
{
	UINT8 * here = GetEmitPtr();

	Emit8(0xE9);
	Emit32(((UINT32)addr - (UINT32)here) - 5);
}

X86_EMITTER( JmpReg )(UINT reg)
{
	Emit8(0xFF);
	Emit8(MODRM(3, 4, reg));
}

X86_EMITTER( Lahf )(void)
{
	Emit8(0x9F);
}

X86_EMITTER( LeaMemSibToReg )(UINT reg, UINT sib)
{
	X86OpRegToMemSib(8, 0x8D, reg, sib);
}

X86_EMITTER( LeaMemSibRelToReg )(UINT reg, UINT sib, UINT disp)
{
	X86OpRegToMemSibRel(8, 0x8D, reg, sib, disp);
}

X86_EMITTER( MovRegToMemReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemReg(size, 0x88, reg, addr);
}

X86_EMITTER( MovMemRegToReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemReg(size, 0x8A, reg, addr);
}

X86_EMITTER( MovRegToMemSib )(UINT size, UINT reg, UINT sib)
{
	X86OpRegToMemSib(size, 0x88, reg, sib);
}

X86_EMITTER( MovMemSibToReg )(UINT size, UINT reg, UINT sib)
{
	X86OpRegToMemSib(size, 0x8A, reg, sib);
}

X86_EMITTER( MovRegToMemSibRel )(UINT size, UINT reg, UINT sib, UINT disp)
{
	X86OpRegToMemSibRel(size, 0x88, reg, sib, disp);
}

X86_EMITTER( MovMemSibRelToReg )(UINT size, UINT reg, UINT sib, UINT disp)
{
	X86OpRegToMemSibRel(size, 0x8A, reg, sib, disp);
}

X86_EMITTER( MovImmToMemReg )(UINT size, UINT addr, UINT imm)
{
	switch(size)
	{
	case 8:
		Emit8(0xC6);
		Emit8(MODRM(0, 0, addr));
		Emit8(imm);
		break;
	case 16:
		Emit8(PREFIX_DATA_SIZE);
		Emit8(0xC7);
		Emit8(MODRM(0, 0, addr));
		Emit16(imm);
		break;
	case 32:
		Emit8(0xC7);
		Emit8(MODRM(0, 0, addr));
		Emit32(imm);
		break;
	default:
		ASSERT(0);
	}
}

X86_EMITTER( MulReg )(UINT size, UINT reg)
{
	X86OpReg(size, 0xE0,reg);
}

X86_EMITTER( NegReg )(UINT size, UINT reg)
{
	X86OpReg(size, 0xD8,reg);
}

X86_EMITTER( Nop )(void)
{
	Emit8(0x90);
}

X86_EMITTER( NotReg )(UINT size, UINT reg)
{
	X86OpReg(size, 0xD0,reg);
}

X86_EMITTER( NotMemAbs )(UINT size, UINT addr)
{
	X86OpMemAbs(size, 2, addr);
}

X86_EMITTER( NotMemReg )(UINT size, UINT addr)
{
	X86OpMemReg(size, 2, addr);
}

X86_EMITTER( OrRegToMemReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemReg(size, 0x08, reg, addr);
}

X86_EMITTER( OrMemRegToReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemReg(size, 0x0A, reg, addr);
}

X86_EMITTER( OrMemRegRelToReg )(UINT size, UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(size, 0x0A, reg, addr, disp);
}

X86_EMITTER( OrRegToMemSib )(UINT size, UINT reg, UINT sib)
{
	X86OpRegToMemSib(size, 0x08, reg, sib);
}

X86_EMITTER( OrMemSibToReg )(UINT size, UINT reg, UINT sib)
{
	X86OpRegToMemSib(size, 0x0A, reg, sib);
}

X86_EMITTER( OrRegToMemSibRel )(UINT size, UINT reg, UINT sib, UINT disp)
{
	X86OpRegToMemSibRel(size, 0x08, reg, sib, disp);
}

X86_EMITTER( OrMemSibRelToReg )(UINT size, UINT reg, UINT sib, UINT disp)
{
	X86OpRegToMemSibRel(size, 0x0A, reg, sib, disp);
}

X86_EMITTER( Popf )(void)
{
	Emit8(0x9D);
}

X86_EMITTER( Pushf )(void)
{
	Emit8(0x9C);
}

X86_EMITTER( RclRegByCL )(UINT size, UINT reg)
{
	X86ShiftRotRegByCL(size, 2, reg);
}

X86_EMITTER( RclRegByImm )(UINT size, UINT reg, UINT imm)
{
	if(imm == 1)
		X86ShiftRotRegBy1(size, 2, reg);
	else
		X86ShiftRotRegByImm(size, 2, reg, imm);
}

X86_EMITTER( RclMemAbsByCL )(UINT size, UINT addr)
{
	X86ShiftRotMemAbsByCL(size, 2, addr);
}

X86_EMITTER( RclMemAbsByImm )(UINT size, UINT addr, UINT imm)
{
	if(imm == 1)
		X86ShiftRotMemAbsBy1(size, 2, addr);
	else
		X86ShiftRotMemAbsByImm(size, 2, addr, imm);
}

X86_EMITTER( RclMemRegByCL )(UINT size, UINT addr)
{
	X86ShiftRotMemRegByCL(size, 2, addr);
}

X86_EMITTER( RclMemRegByImm )(UINT size, UINT addr, UINT imm)
{
	if(imm == 1)
		X86ShiftRotMemRegBy1(size, 2, addr);
	else
		X86ShiftRotMemRegByImm(size, 2, addr, imm);
}

X86_EMITTER( RcrRegByCL )(UINT size, UINT reg)
{
	X86ShiftRotRegByCL(size, 3, reg);
}

X86_EMITTER( RcrRegByImm )(UINT size, UINT reg, UINT imm)
{
	if(imm == 1)
		X86ShiftRotRegBy1(size, 3, reg);
	else
		X86ShiftRotRegByImm(size, 3, reg, imm);
}

X86_EMITTER( RcrMemAbsByCL )(UINT size, UINT addr)
{
	X86ShiftRotMemAbsByCL(size, 3, addr);
}

X86_EMITTER( RcrMemAbsByImm )(UINT size, UINT addr, UINT imm)
{
	if(imm == 1)
		X86ShiftRotMemAbsBy1(size, 3, addr);
	else
		X86ShiftRotMemAbsByImm(size, 3, addr, imm);
}

X86_EMITTER( RcrMemRegByCL )(UINT size, UINT addr)
{
	X86ShiftRotMemRegByCL(size, 3, addr);
}

X86_EMITTER( RcrMemRegByImm )(UINT size, UINT addr, UINT imm)
{
	if(imm == 1)
		X86ShiftRotMemRegBy1(size, 3, addr);
	else
		X86ShiftRotMemRegByImm(size, 3, addr, imm);
}

X86_EMITTER( RolRegByCL )(UINT size, UINT reg)
{
	X86ShiftRotRegByCL(size, 0, reg);
}

X86_EMITTER( RolRegByImm )(UINT size, UINT reg, UINT imm)
{
	if(imm == 1)
		X86ShiftRotRegBy1(size, 0, reg);
	else
		X86ShiftRotRegByImm(size, 0, reg, imm);
}

X86_EMITTER( RolMemAbsByCL )(UINT size, UINT addr)
{
	X86ShiftRotMemAbsByCL(size, 0, addr);
}

X86_EMITTER( RolMemRegByCL )(UINT size, UINT addr)
{
	X86ShiftRotMemRegByCL(size, 0, addr);
}

X86_EMITTER( RorRegByCL )(UINT size, UINT reg)
{
	X86ShiftRotRegByCL(size, 1, reg);
}

X86_EMITTER( RorRegByImm )(UINT size, UINT reg, UINT imm)
{
	if(imm == 1)
		X86ShiftRotRegBy1(size, 1, reg);
	else
		X86ShiftRotRegByImm(size, 1, reg, imm);
}

X86_EMITTER( RorMemAbsByCL )(UINT size, UINT addr)
{
	X86ShiftRotMemAbsByCL(size, 1, addr);
}

X86_EMITTER( RorMemRegByCL )(UINT size, UINT addr)
{
	X86ShiftRotMemRegByCL(size, 1, addr);
}

X86_EMITTER( Sahf )(void)
{
	Emit8(0x9E);
}

X86_EMITTER( SarRegByCL )(UINT size, UINT reg)
{
	X86ShiftRotRegByCL(size, 7, reg);
}

X86_EMITTER( SarRegByImm )(UINT size, UINT reg, UINT imm)
{
	if(imm == 1)
		X86ShiftRotRegBy1(size, 7, reg);
	else
		X86ShiftRotRegByImm(size, 7, reg, imm);
}

X86_EMITTER( SarMemAbsByCL )(UINT size, UINT addr)
{
	X86ShiftRotMemAbsByCL(size, 7, addr);
}

X86_EMITTER( SarMemAbsByImm )(UINT size, UINT addr, UINT imm)
{
	if(imm == 1)
		X86ShiftRotMemAbsBy1(size, 7, addr);
	else
		X86ShiftRotMemAbsByImm(size, 7, addr, imm);
}

X86_EMITTER( SarMemRegByCL )(UINT size, UINT addr)
{
	X86ShiftRotMemRegByCL(size, 7, addr);
}

X86_EMITTER( SarMemRegByImm )(UINT size, UINT addr, UINT imm)
{
	if(imm == 1)
		X86ShiftRotMemRegBy1(size, 7, addr);
	else
		X86ShiftRotMemRegByImm(size, 7, addr, imm);
}

X86_EMITTER( SbbReg1ToReg2 )(UINT size, UINT reg1, UINT reg2)
{
	X86OpReg1ToReg2(size, 0x18, reg1, reg2);
}

X86_EMITTER( SbbImmToReg )(UINT size, UINT reg, UINT imm)
{
	if(reg == EAX && !IsINT8(imm))
		X86OpImmToAcc(size, 0x1C, imm);
	else
		X86OpImmToReg(size, 0xD8, reg, imm, TRUE);
}

X86_EMITTER( SbbRegToMemAbs )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemAbs(size, 0x18, reg, addr);
}

X86_EMITTER( SbbMemAbsToReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemAbs(size, 0x1A, reg, addr);
}

X86_EMITTER( SbbRegToMemReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemReg(size, 0x18, reg, addr);
}

X86_EMITTER( SbbMemRegToReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemReg(size, 0x1A, reg, addr);
}

X86_EMITTER( SbbRegToMemRegRel )(UINT size, UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(size, 0x18, reg, addr, disp);
}

X86_EMITTER( SbbMemRegRelToReg )(UINT size, UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(size, 0x1A, reg, addr, disp);
}

X86_EMITTER( SbbRegToMemSib )(UINT size, UINT reg, UINT sib)
{
	X86OpRegToMemSib(size, 0x18, reg, sib);
}

X86_EMITTER( SbbMemSibToReg )(UINT size, UINT reg, UINT sib)
{
	X86OpRegToMemSib(size, 0x1A, reg, sib);
}

X86_EMITTER( SbbRegToMemSibRel )(UINT size, UINT reg, UINT sib, UINT disp)
{
	X86OpRegToMemSibRel(size, 0x18, reg, sib, disp);
}

X86_EMITTER( SbbMemSibRelToReg )(UINT size, UINT reg, UINT sib, UINT disp)
{
	X86OpRegToMemSibRel(size, 0x1A, reg, sib, disp);
}

X86_EMITTER( SbbImmToMemAbs )(UINT size, UINT addr, UINT imm)
{
	X86OpImmToMemAbs(size, 3, addr, imm);
}

X86_EMITTER( SbbImmToMemReg )(UINT size, UINT addr, UINT imm)
{
	X86OpImmToMemReg(size, 3, addr, imm);
}

X86_EMITTER( SetccReg )(UINT cc, UINT reg)
{
	Emit8(0x0F);
	Emit8(0x90 | (cc & 15));
	Emit8(0xC0 | (reg & 7));
}

X86_EMITTER( ShlRegByCL )(UINT size, UINT reg)
{
	X86ShiftRotRegByCL(size, 4, reg);
}

X86_EMITTER( ShlRegByImm )(UINT size, UINT reg, UINT imm)
{
	if(imm == 1)
		X86ShiftRotRegBy1(size, 4, reg);
	else
		X86ShiftRotRegByImm(size, 4, reg, imm);
}

X86_EMITTER( ShlMemAbsByCL )(UINT size, UINT addr)
{
	X86ShiftRotMemAbsByCL(size, 4, addr);
}

X86_EMITTER( ShlMemRegByCL )(UINT size, UINT addr)
{
	X86ShiftRotMemRegByCL(size, 4, addr);
}

X86_EMITTER( ShrRegByCL )(UINT size, UINT reg)
{
	X86ShiftRotRegByCL(size, 0xE8, reg);
}

X86_EMITTER( ShrRegByImm )(UINT size, UINT reg, UINT imm)
{
	if(imm == 1)
		X86ShiftRotRegBy1(size, 0xE8, reg);
	else
		X86ShiftRotRegByImm(size, 0xE8, reg, imm);
}

X86_EMITTER( ShrMemAbsByCL )(UINT size, UINT addr)
{
	X86ShiftRotMemAbsByCL(size, 5, addr);
}

X86_EMITTER( ShrMemRegByCL )(UINT size, UINT addr)
{
	X86ShiftRotMemRegByCL(size, 5, addr);
}

X86_EMITTER( Stc )(void)
{
	Emit8(0xF9);
}

X86_EMITTER( SubReg1ToReg2 )(UINT size, UINT reg1, UINT reg2)
{
	X86OpReg1ToReg2(size, 0x28, reg1, reg2);
}

X86_EMITTER( SubImmToReg )(UINT size, UINT reg, UINT imm)
{
	if(reg == EAX && !IsINT8(imm))
		X86OpImmToAcc(size, 0x2C, imm);
	else
		X86OpImmToReg(size, 0xE8, reg, imm, TRUE);
}

X86_EMITTER( SubMemAbsToReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemAbs(size, 0x2A, reg, addr);
}

X86_EMITTER( SubMemRegToReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemReg(size, 0x2A, reg, addr);
}

X86_EMITTER( SubRegToMemRegRel )(UINT size, UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(size, 0x28, reg, addr, disp);
}

X86_EMITTER( SubMemRegRelToReg )(UINT size, UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(size, 0x2A, reg, addr, disp);
}

X86_EMITTER( SubRegToMemSib )(UINT size, UINT reg, UINT sib)
{
	X86OpRegToMemSib(size, 0x28, reg, sib);
}

X86_EMITTER( SubMemSibToReg )(UINT size, UINT reg, UINT sib)
{
	X86OpRegToMemSib(size, 0x2A, reg, sib);
}

X86_EMITTER( SubRegToMemSibRel )(UINT size, UINT reg, UINT sib, UINT disp)
{
	X86OpRegToMemSibRel(size, 0x28, reg, sib, disp);
}

X86_EMITTER( SubMemSibRelToReg )(UINT size, UINT reg, UINT sib, UINT disp)
{
	X86OpRegToMemSibRel(size, 0x2A, reg, sib, disp);
}

X86_EMITTER( SubImmToMemAbs )(UINT size, UINT addr, UINT imm)
{
	X86OpImmToMemAbs(size, 5, addr, imm);
}

X86_EMITTER( SubImmToMemReg )(UINT size, UINT addr, UINT imm)
{
	X86OpImmToMemReg(size, 5, addr, imm);
}

X86_EMITTER( TestImmToReg )(UINT size, UINT reg, UINT imm)
{
	if(reg == EAX)
		X86OpImmToAcc(size, 0xA8, imm);
	else
	{
		switch(size)
		{
		case 8:
			Emit8(0xF6);
			Emit8(MODRM(3, 0, reg));
			Emit8(imm);
			break;
		case 16:
			Emit8(PREFIX_DATA_SIZE);
			Emit8(0xF7);
			Emit8(MODRM(3, 0, reg));
			Emit16(imm);
			break;
		case 32:
			Emit8(0xF7);
			Emit8(MODRM(3, 0, reg));
			Emit32(imm);
			break;
		default:
			ASSERT(0);
		}
	}
}

X86_EMITTER( TestRegToMemAbs )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemAbs(size, 0x84, reg, addr);
}

X86_EMITTER( TestRegToMemReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemReg(size, 0x84, reg, addr);
}

X86_EMITTER( TestRegToMemRegRel )(UINT size, UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(size, 0x84, reg, addr, disp);
}

X86_EMITTER( TestRegToMemSib )(UINT size, UINT reg, UINT sib)
{
	X86OpRegToMemSib(size, 0x84, reg, sib);
}

X86_EMITTER( TestRegToMemSibRel )(UINT size, UINT reg, UINT sib, UINT disp)
{
	X86OpRegToMemSibRel(size, 0x84, reg, sib, disp);
}

X86_EMITTER( TestImmToMemAbs )(UINT size, UINT addr, UINT imm)
{
	switch(size)
	{
	case 8:
		Emit8(0xF6);
		Emit8(MODRM(0, 0, 5));
		Emit32(addr);
		Emit8(imm);
		break;
	case 16:
		Emit8(PREFIX_DATA_SIZE);
		Emit8(0xF7);
		Emit8(MODRM(0, 0, 5));
		Emit32(addr);
		Emit8(imm);
		break;
	case 32:
		Emit8(0xF7);
		Emit8(MODRM(0, 0, 5));
		Emit32(addr);
		Emit8(imm);
		break;
	default:
		ASSERT(0);
	}
}

X86_EMITTER( XchgRegToMemAbs )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemAbs(size, 0x86, reg, addr);
}

X86_EMITTER( XchgRegToMemReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemReg(size, 0x86, reg, addr);
}

X86_EMITTER( XorImmToReg )(UINT size, UINT reg, UINT imm)
{
	if(reg == EAX && !IsINT8(imm))
		X86OpImmToAcc(size, 0x34, imm);
	else
		X86OpImmToReg(size, 0xF0, reg, imm, TRUE);
}

X86_EMITTER( XorRegToMemAbs )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemAbs(size, 0x30, reg, addr);
}

X86_EMITTER( XorMemAbsToReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemAbs(size, 0x32, reg, addr);
}

X86_EMITTER( XorRegToMemReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemReg(size, 0x30, reg, addr);
}

X86_EMITTER( XorMemRegToReg )(UINT size, UINT reg, UINT addr)
{
	X86OpRegToMemReg(size, 0x32, reg, addr);
}

X86_EMITTER( XorRegToMemRegRel )(UINT size, UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(size, 0x30, reg, addr, disp);
}

X86_EMITTER( XorMemRegRelToReg )(UINT size, UINT reg, UINT addr, UINT disp)
{
	X86OpRegToMemRegRel(size, 0x32, reg, addr, disp);
}

X86_EMITTER( XorRegToMemSib )(UINT size, UINT reg, UINT sib)
{
	X86OpRegToMemSib(size, 0x30, reg, sib);
}

X86_EMITTER( XorMemSibToReg )(UINT size, UINT reg, UINT sib)
{
	X86OpRegToMemSib(size, 0x32, reg, sib);
}

X86_EMITTER( XorRegToMemSibRel )(UINT size, UINT reg, UINT sib, UINT disp)
{
	X86OpRegToMemSibRel(size, 0x30, reg, sib, disp);
}

X86_EMITTER( XorMemSibRelToReg )(UINT size, UINT reg, UINT sib, UINT disp)
{
	X86OpRegToMemSibRel(size, 0x32, reg, sib, disp);
}

*/


/*******************************************************************************
 FPU Opcodes

 What it says.
*******************************************************************************/

/*** Fld **********************************************************************/

X86_EMITTER( FldMemAbs_32 )(UINT32 addr)
{
	Emit8(0xD9);
	Emit8(MODRM(0, 0, 5));
	Emit32(addr);
}

X86_EMITTER( FldMemAbs_64 )(UINT32 addr)
{
	Emit8(0xDD);
	Emit8(MODRM(0, 0, 5));
	Emit32(addr);
}

X86_EMITTER( FldMemAbs_80 )(UINT32 addr)
{
	Emit8(0xDB);
	Emit8(MODRM(0, 0, 5));
	Emit32(addr);
}

X86_EMITTER( FldST )(UINT sti)
{
	Emit8(0xD9);
	Emit8(0xC0 + (sti & 7));
}

/*** Fst[p] ********************************************************************/

X86_EMITTER( FstMemAbs_32 )(UINT32 addr)
{
	Emit8(0xD9);
	Emit8(MODRM(0, 2, 5));
	Emit32(addr);
}

X86_EMITTER( FstMemAbs_64 )(UINT32 addr)
{
	Emit8(0xDD);
	Emit8(MODRM(0, 2, 5));
	Emit32(addr);
}

X86_EMITTER( FstST )(UINT sti)
{
	Emit8(0xDD);
	Emit8(0xD0 + (sti & 7));
}

X86_EMITTER( FstpMemAbs_32 )(UINT32 addr)
{
	Emit8(0xD9);
	Emit8(MODRM(0, 3, 5));
	Emit32(addr);
}

X86_EMITTER( FstpMemAbs_64 )(UINT32 addr)
{
	Emit8(0xDD);
	Emit8(MODRM(0, 3, 5));
	Emit32(addr);
}

X86_EMITTER( FstpMemAbs_80 )(UINT32 addr)
{
	Emit8(0xDB);
	Emit8(MODRM(0, 3, 5));
	Emit32(addr);
}

X86_EMITTER( FstpST )(UINT sti)
{
	Emit8(0xDD);
	Emit8(0xD8 + (sti & 7));
}

/*** Fdiv[p] ******************************************************************/

X86_EMITTER( Fdivp )(void)
{
	Emit8(0xDE);
	Emit8(0xF9);
}

/*** Fmul[p] ******************************************************************/

X86_EMITTER( Fmulp )(void)
{
	Emit8(0xDE);
	Emit8(0xC9);
}

/*** Fsub[p] ******************************************************************/

X86_EMITTER( Fsubp )(void)
{
	Emit8(0xDE);
	Emit8(0xE9);	
}
