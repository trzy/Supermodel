/*
 * back/ia32/emit.h
 *
 * Intel IA-32 Opcode Emitter.
 */

#ifndef _EMIT_H_
#define _EMIT_H_

#include "drppc.h"
#include "toplevel.h"

/*
 * Register Numbers
 */

enum { AL, CL, DL, BL, AH, CH, DH, BH };

enum { AX, CX, DX, BX, SP, BP, SI, DI };

enum { EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI };

/*
 * Condition Codes
 */

typedef enum
{
	// integer conditions

	CC_O	= 0,	// Overflow
	CC_NO	= 1,	// Not Overflow
	CC_B	= 2,	// Below
	CC_C	= 2,	// Carry
	CC_NAE	= 2,	// Bot Above or Equal
	CC_NB	= 3,	// Not Below
	CC_NC	= 3,	// Not Carry
	CC_AE	= 3,	// Above or Equal
	CC_E	= 4,	// Equal
	CC_Z	= 4,	// Zero
	CC_NE	= 5,	// Not Equal
	CC_NZ	= 5,	// Not Zero
	CC_BE	= 6,	// Below or Equal
	CC_NA	= 6,	// Not Above
	CC_NBE	= 7,	// Not Below or Equal
	CC_A	= 7,	// Above
	CC_S	= 8,	// Sign
	CC_NS	= 9,	// Not Sign
	CC_P	= 10,	// Parity
	CC_PE	= 10,	// Even Parity
	CC_NP	= 11,	// Not Parity
	CC_PO	= 11,	// Odd Parity
	CC_L	= 12,	// Less
	CC_NGE	= 12,	// Not Greater or Equal
	CC_NL	= 13,	// Not Less
	CC_GE	= 13,	// Greater or Equal
	CC_LE	= 14,	// Less or Equal
	CC_NG	= 14,	// Not Greater
	CC_NLE	= 15,	// Not Less or Equal
	CC_G	= 15,	// Greater

	// fp conditions for FCMOVcc

	FCC_B	= 0,	// Below
	FCC_E	= 1,	// Equal
	FCC_BE	= 2,	// Below or Equal
	FCC_U	= 3,	// Unordered
	FCC_NB	= 4,	// Not Below
	FCC_NE	= 5,	// Not Equal
	FCC_NBE	= 6,	// Not Below or Equal
	FCC_NU	= 7,	// Not Unordered

} X86_CC;

/*
 * Operand size for 'movsx' and'movzx' instructions
 */

#define X86_8_TO_16		0
#define X86_8_TO_32		1
#define X86_16_TO_32	2

/*
 * FP Rounding Modes
 */

#define X86_ROUNDM_NEAR	0
#define X86_ROUNDM_DOWN	1
#define X86_ROUNDM_UP	2
#define X86_ROUNDM_CHOP	3

/*******************************************************************************
 Instructions
*******************************************************************************/

#define X86_EMITTER(name)	void _##name

extern X86_EMITTER( AndRegToMemAbs_8 )(UINT reg, UINT addr);
extern X86_EMITTER( AndRegToMemAbs_16 )(UINT reg, UINT addr);
extern X86_EMITTER( AndRegToMemAbs_32 )(UINT reg, UINT addr);
extern X86_EMITTER( AndRegToMemRegRel_8 )(UINT reg, UINT addr, UINT disp);
extern X86_EMITTER( AndRegToMemRegRel_16 )(UINT reg, UINT addr, UINT disp);
extern X86_EMITTER( AndRegToMemRegRel_32 )(UINT reg, UINT addr, UINT disp);
extern X86_EMITTER( AndImmToMemAbs_8 )(UINT addr, UINT imm);
extern X86_EMITTER( AndImmToMemAbs_16 )(UINT addr, UINT imm);
extern X86_EMITTER( AndImmToMemAbs_32 )(UINT addr, UINT imm);
extern X86_EMITTER( AndImmToMemRegRel_8 )(UINT addr, UINT disp, UINT imm);
extern X86_EMITTER( AndImmToMemRegRel_16 )(UINT addr, UINT disp, UINT imm);
extern X86_EMITTER( AndImmToMemRegRel_32 )(UINT addr, UINT disp, UINT imm);
extern X86_EMITTER( AddRegToReg_8 )(UINT dest, UINT src);
extern X86_EMITTER( AddRegToReg_16 )(UINT dest, UINT src);
extern X86_EMITTER( AddRegToReg_32 )(UINT dest, UINT src);
extern X86_EMITTER( AddRegToMemAbs_8 )(UINT reg, UINT addr);
extern X86_EMITTER( AddRegToMemAbs_16 )(UINT reg, UINT addr);
extern X86_EMITTER( AddRegToMemAbs_32 )(UINT reg, UINT addr);
extern X86_EMITTER( AddRegToMemRegRel_8 )(UINT reg, UINT addr, UINT disp);
extern X86_EMITTER( AddRegToMemRegRel_16 )(UINT reg, UINT addr, UINT disp);
extern X86_EMITTER( AddRegToMemRegRel_32 )(UINT reg, UINT addr, UINT disp);
extern X86_EMITTER( AddMemRegRelToReg_8 )(UINT reg, UINT addr, UINT disp);
extern X86_EMITTER( AddMemRegRelToReg_16 )(UINT reg, UINT addr, UINT disp);
extern X86_EMITTER( AddMemRegRelToReg_32 )(UINT reg, UINT addr, UINT disp);
extern X86_EMITTER( AddImmToReg_8 )(UINT reg, UINT imm);
extern X86_EMITTER( AddImmToReg_16 )(UINT reg, UINT imm);
extern X86_EMITTER( AddImmToReg_32 )(UINT reg, UINT imm);
extern X86_EMITTER( AddImmToMemAbs_8 )(UINT addr, UINT imm);
extern X86_EMITTER( AddImmToMemAbs_16 )(UINT addr, UINT imm);
extern X86_EMITTER( AddImmToMemAbs_32 )(UINT addr, UINT imm);
extern X86_EMITTER( AddImmToMemReg_8 )(UINT addr, UINT imm);
extern X86_EMITTER( AddImmToMemReg_16 )(UINT addr, UINT imm);
extern X86_EMITTER( AddImmToMemReg_32 )(UINT addr, UINT imm);
extern X86_EMITTER( AddImmToMemRegRel_8 )(UINT addr, UINT disp, UINT imm);
extern X86_EMITTER( AddImmToMemRegRel_16 )(UINT addr, UINT disp, UINT imm);
extern X86_EMITTER( AddImmToMemRegRel_32 )(UINT addr, UINT disp, UINT imm);
extern X86_EMITTER( BswapReg_32 )(UINT reg);
extern X86_EMITTER( CallDirect )(UINT addr);
extern X86_EMITTER( CmovccRegToReg_16 )(UINT cc, UINT dest, UINT src);
extern X86_EMITTER( CmovccRegToReg_32 )(UINT cc, UINT dest, UINT src);
extern X86_EMITTER( CmpRegToMemAbs_8 )(UINT reg, UINT addr);
extern X86_EMITTER( CmpRegToMemAbs_16 )(UINT reg, UINT addr);
extern X86_EMITTER( CmpRegToMemAbs_32 )(UINT reg, UINT addr);
extern X86_EMITTER( CmpRegToMemRegRel_8 )(UINT reg, UINT addr, UINT offs);
extern X86_EMITTER( CmpRegToMemRegRel_16 )(UINT reg, UINT addr, UINT offs);
extern X86_EMITTER( CmpRegToMemRegRel_32 )(UINT reg, UINT addr, UINT offs);
extern X86_EMITTER( CmpMemAbsToImm_8 )(UINT addr, UINT imm);
extern X86_EMITTER( CmpMemAbsToImm_16 )(UINT addr, UINT imm);
extern X86_EMITTER( CmpMemAbsToImm_32 )(UINT addr, UINT imm);
extern X86_EMITTER( CmpMemRegRelToImm_8 )(UINT addr, UINT offs, UINT imm);
extern X86_EMITTER( CmpMemRegRelToImm_16 )(UINT addr, UINT offs, UINT imm);
extern X86_EMITTER( CmpMemRegRelToImm_32 )(UINT addr, UINT offs, UINT imm);
extern X86_EMITTER( FldMemAbs_32 )(UINT32 addr);
extern X86_EMITTER( FldMemAbs_64 )(UINT32 addr);
extern X86_EMITTER( FldMemAbs_80 )(UINT32 addr);
extern X86_EMITTER( FldST )(UINT sti);
extern X86_EMITTER( FstMemAbs_32 )(UINT32 addr);
extern X86_EMITTER( FstMemAbs_64 )(UINT32 addr);
extern X86_EMITTER( FstST )(UINT sti);
extern X86_EMITTER( FstpMemAbs_32 )(UINT32 addr);
extern X86_EMITTER( FstpMemAbs_64 )(UINT32 addr);
extern X86_EMITTER( FstpMemAbs_80 )(UINT32 addr);
extern X86_EMITTER( FstpST )(UINT sti);
extern X86_EMITTER( Fsubp )(void);
extern X86_EMITTER( Fmulp )(void);
extern X86_EMITTER( Fdivp )(void);
extern X86_EMITTER( JccRel8 )(UINT cc, UINT addr);
extern X86_EMITTER( JccRel32 )(UINT cc, UINT addr);
extern X86_EMITTER( MovRegToReg_8 )(UINT reg2, UINT reg1);
extern X86_EMITTER( MovRegToReg_16 )(UINT reg2, UINT reg1);
extern X86_EMITTER( MovRegToReg_32 )(UINT reg2, UINT reg1);
extern X86_EMITTER( MovImmToMemAbs_8 )(UINT addr, UINT imm);
extern X86_EMITTER( MovImmToMemAbs_16 )(UINT addr, UINT imm);
extern X86_EMITTER( MovImmToMemAbs_32 )(UINT addr, UINT imm);
extern X86_EMITTER( MovImmToMemRegRel_8 )(UINT reg, UINT offs, UINT32 imm);
extern X86_EMITTER( MovImmToMemRegRel_16 )(UINT reg, UINT offs, UINT32 imm);
extern X86_EMITTER( MovImmToMemRegRel_32 )(UINT reg, UINT offs, UINT32 imm);
extern X86_EMITTER( MovImmToReg_8 )(UINT reg, UINT imm);
extern X86_EMITTER( MovImmToReg_16 )(UINT reg, UINT imm);
extern X86_EMITTER( MovImmToReg_32 )(UINT reg, UINT imm);
extern X86_EMITTER( MovRegToMemAbs_8 )(UINT reg, UINT addr);
extern X86_EMITTER( MovRegToMemAbs_16 )(UINT reg, UINT addr);
extern X86_EMITTER( MovRegToMemAbs_32 )(UINT reg, UINT addr);
extern X86_EMITTER( MovMemAbsToReg_8 )(UINT reg, UINT addr);
extern X86_EMITTER( MovMemAbsToReg_16 )(UINT reg, UINT addr);
extern X86_EMITTER( MovMemAbsToReg_32 )(UINT reg, UINT addr);
extern X86_EMITTER( MovRegToMemRegRel_8 )(UINT reg, UINT addr, UINT disp);
extern X86_EMITTER( MovRegToMemRegRel_16 )(UINT reg, UINT addr, UINT disp);
extern X86_EMITTER( MovRegToMemRegRel_32 )(UINT reg, UINT addr, UINT disp);
extern X86_EMITTER( MovMemRegRelToReg_8 )(UINT reg, UINT addr, UINT disp);
extern X86_EMITTER( MovMemRegRelToReg_16 )(UINT reg, UINT addr, UINT disp);
extern X86_EMITTER( MovMemRegRelToReg_32 )(UINT reg, UINT addr, UINT disp);
extern X86_EMITTER( MovsxRegToReg_8To16 )(UINT, UINT);
extern X86_EMITTER( MovsxRegToReg_8To32 )(UINT, UINT);
extern X86_EMITTER( MovsxRegToReg_16To32 )(UINT, UINT);
extern X86_EMITTER( MovsxMemAbsToReg_8To16 )(UINT, UINT);
extern X86_EMITTER( MovsxMemAbsToReg_8To32 )(UINT, UINT);
extern X86_EMITTER( MovsxMemAbsToReg_16To32 )(UINT, UINT);
extern X86_EMITTER( MulMemAbs_8 )(UINT addr);
extern X86_EMITTER( MulMemAbs_16 )(UINT addr);
extern X86_EMITTER( MulMemAbs_32 )(UINT addr);
extern X86_EMITTER( MulMemRegRel_8 )(UINT addr, UINT disp);
extern X86_EMITTER( MulMemRegRel_16 )(UINT addr, UINT disp);
extern X86_EMITTER( MulMemRegRel_32 )(UINT addr, UINT disp);
extern X86_EMITTER( NegMemAbs_32 )(UINT32 addr);
extern X86_EMITTER( NegMemRegRel_32 )(UINT rel, UINT8 offs);
extern X86_EMITTER( NotMemAbs_32 )(UINT32 addr);
extern X86_EMITTER( NotMemRegRel_32 )(UINT rel, UINT8 offs);
extern X86_EMITTER( OrRegToMemAbs_8 )(UINT reg, UINT addr);
extern X86_EMITTER( OrRegToMemAbs_16 )(UINT reg, UINT addr);
extern X86_EMITTER( OrRegToMemAbs_32 )(UINT reg, UINT addr);
extern X86_EMITTER( OrRegToMemRegRel_8 )(UINT reg, UINT addr, UINT disp);
extern X86_EMITTER( OrRegToMemRegRel_16 )(UINT reg, UINT addr, UINT disp);
extern X86_EMITTER( OrRegToMemRegRel_32 )(UINT reg, UINT addr, UINT disp);
extern X86_EMITTER( OrImmToMemAbs_8 )(UINT addr, UINT imm);
extern X86_EMITTER( OrImmToMemAbs_16 )(UINT addr, UINT imm);
extern X86_EMITTER( OrImmToMemAbs_32 )(UINT addr, UINT imm);
extern X86_EMITTER( OrImmToMemRegRel_8 )(UINT addr, UINT disp, UINT imm);
extern X86_EMITTER( OrImmToMemRegRel_16 )(UINT addr, UINT disp, UINT imm);
extern X86_EMITTER( OrImmToMemRegRel_32 )(UINT addr, UINT disp, UINT imm);
extern X86_EMITTER( PopReg_16 )(UINT reg);
extern X86_EMITTER( PopReg_32 )(UINT reg);
extern X86_EMITTER( PopMemAbs_16 )(UINT addr);
extern X86_EMITTER( PopMemAbs_32 )(UINT addr);
extern X86_EMITTER( PopMemRegRel_16 )(UINT reg, UINT offs);
extern X86_EMITTER( PopMemRegRel_32 )(UINT reg, UINT offs);
extern X86_EMITTER( PopMemReg_16 )(UINT addr);
extern X86_EMITTER( PopMemReg_32 )(UINT addr);
extern X86_EMITTER( Popa )(void);
extern X86_EMITTER( Pusha )(void);
extern X86_EMITTER( PushReg_16 )(UINT reg);
extern X86_EMITTER( PushReg_32 )(UINT reg);
extern X86_EMITTER( PushImm_16 )(UINT imm);
extern X86_EMITTER( PushImm_32 )(UINT imm);
extern X86_EMITTER( PushMemRegRel_16 )(UINT reg, UINT offs);
extern X86_EMITTER( PushMemRegRel_32 )(UINT reg, UINT offs);
extern X86_EMITTER( PushMemAbs_16 )(UINT addr);
extern X86_EMITTER( PushMemAbs_32 )(UINT addr);
extern X86_EMITTER( PushMemReg_16 )(UINT addr);
extern X86_EMITTER( PushMemReg_32 )(UINT addr);
extern X86_EMITTER( Ret )(void);
extern X86_EMITTER( RolMemAbsByImm_32 )(UINT addr, UINT imm);
extern X86_EMITTER( RolMemRegRelByImm_8 )(UINT addr, UINT disp, UINT imm);
extern X86_EMITTER( RolMemRegRelByImm_16 )(UINT addr, UINT disp, UINT imm);
extern X86_EMITTER( RolMemRegRelByImm_32 )(UINT addr, UINT disp, UINT imm);
extern X86_EMITTER( RorMemAbsByImm_32 )(UINT addr, UINT imm);
extern X86_EMITTER( RorMemRegRelByImm_8 )(UINT addr, UINT disp, UINT imm);
extern X86_EMITTER( RorMemRegRelByImm_16 )(UINT addr, UINT disp, UINT imm);
extern X86_EMITTER( RorMemRegRelByImm_32 )(UINT addr, UINT disp, UINT imm);
extern X86_EMITTER( SetccMemAbs_8 )(UINT cc, UINT addr);
extern X86_EMITTER( SetccMemRegRel_8 )(UINT cc, UINT base, UINT offs);
extern X86_EMITTER( ShlMemAbsByImm_32 )(UINT addr, UINT imm);
extern X86_EMITTER( ShlMemRegRelByImm_8 )(UINT addr, UINT disp, UINT imm);
extern X86_EMITTER( ShlMemRegRelByImm_16 )(UINT addr, UINT disp,  UINT imm);
extern X86_EMITTER( ShlMemRegRelByImm_32 )(UINT addr, UINT disp,  UINT imm);
extern X86_EMITTER( ShrMemAbsByImm_32 )(UINT addr, UINT imm);
extern X86_EMITTER( ShrMemRegRelByImm_8 )(UINT addr, UINT disp, UINT imm);
extern X86_EMITTER( ShrMemRegRelByImm_16 )(UINT addr, UINT disp,  UINT imm);
extern X86_EMITTER( ShrMemRegRelByImm_32 )(UINT addr, UINT disp,  UINT imm);
extern X86_EMITTER( SubRegToMemAbs_8 )(UINT reg, UINT addr);
extern X86_EMITTER( SubRegToMemAbs_16 )(UINT reg, UINT addr);
extern X86_EMITTER( SubRegToMemAbs_32 )(UINT reg, UINT addr);
extern X86_EMITTER( SubRegToMemRegRel_8 )(UINT reg, UINT addr, UINT disp);
extern X86_EMITTER( SubRegToMemRegRel_16 )(UINT reg, UINT addr, UINT disp);
extern X86_EMITTER( SubRegToMemRegRel_32 )(UINT reg, UINT addr, UINT disp);
extern X86_EMITTER( TestRegWithReg_8 )(UINT reg1, UINT reg2);
extern X86_EMITTER( TestRegWithReg_16 )(UINT reg1, UINT reg2);
extern X86_EMITTER( TestRegWithReg_32 )(UINT reg1, UINT reg2);
extern X86_EMITTER( TestImmWithMemRegRel_8 )(UINT addr, UINT offs, UINT imm);
extern X86_EMITTER( TestImmWithMemRegRel_16 )(UINT addr, UINT offs, UINT imm);
extern X86_EMITTER( TestImmWithMemRegRel_32 )(UINT addr, UINT offs, UINT imm);
extern X86_EMITTER( XchgRegToReg_8 )(UINT reg1, UINT reg2);
extern X86_EMITTER( XchgRegToReg_16 )(UINT reg1, UINT reg2);
extern X86_EMITTER( XchgRegToReg_32 )(UINT reg1, UINT reg2);
extern X86_EMITTER( XorRegToReg_8 )(UINT reg2, UINT reg1);
extern X86_EMITTER( XorRegToReg_16 )(UINT reg2, UINT reg1);
extern X86_EMITTER( XorRegToReg_32 )(UINT reg2, UINT reg1);
extern X86_EMITTER( XorRegToMemRegRel_32 )(UINT reg, UINT addr, UINT disp);
extern X86_EMITTER( XorRegToMemAbs_32 )(UINT reg, UINT addr);
extern X86_EMITTER( XorImmToMemAbs_8 )(UINT addr, UINT imm);
extern X86_EMITTER( XorImmToMemAbs_16 )(UINT addr, UINT imm);
extern X86_EMITTER( XorImmToMemAbs_32 )(UINT addr, UINT imm);
extern X86_EMITTER( XorImmToMemRegRel_8 )(UINT addr, UINT disp, UINT imm);
extern X86_EMITTER( XorImmToMemRegRel_16 )(UINT addr, UINT disp, UINT imm);
extern X86_EMITTER( XorImmToMemRegRel_32 )(UINT addr, UINT disp, UINT imm);

#endif // _EMIT_H_
