/*
 * analys.c
 *
 * PowerPC instruction and basic block analysis.
 *
 * Two fields have been added to the instruction descriptor that are not present in the disassembler.
 * These are the "read" and "written" fields. They indicate which registers are read and written by
 * specifying either a mask for the analyzer function to use or a specific register for instructions
 * which access registers implicitly. 
 * 
 * IMPORTANT NOTES:
 *
 * Each time a register is accessed by an instruction, it is counted. Examples:
 *
 *		add		r0,r0,r1	; R0 is counted twice (once read, once written), R1 once (read)
 *		add		r1,r1,r1	; R1 is counted three times (twice read, once written)
 *		bclrl	0x14,0		; LR is counted twice (once read, once written)
 *
 * The following assumptions about implicit register usage are made:
 *
 *		- If an LK field is present and set, the LR register is written. This isn't immediately
 *		  apparent from the "written" field, but is implied by the FL_LK flag.
 *		- If an instruction is a conditional branch instruction, its BO field is tested to see if CTR
 *		  is read/written. Conditional branches are indicated by the FL_COND_BRANCH flag.
 *
 * Flags aren't really handled. Instructions which can automatically update CR0, XER, and FPSCR (for
 * floating point instructions) do not yet treat those registers as being accessed but instructions 
 * which explicitly use CR fields as operands do count those accesses.
 *
 * Some SPRs and other misc. registers are kept track of, most aren't. There are notes next to certain 
 * entries in the instruction table about this.
 *
 * Segment registers (SRn) are not kept track of at all.
 *
 * Load/store instructions which deal with multiple registers (LMW, STSWI, etc.) are not handled 
 * properly. The additional registers loaded/stored are not recognized.
 */
 
#include "model3.h"
#include "analys.h"	


/******************************************************************/
/* Instruction Data                                               */
/*																  */
/* Taken from our PowerPC disassembler.							  */
/******************************************************************/

/*
 * Register Usage Flags
 *
 * These indicate which registers the instruction uses either by
 * specifying a field to look in or a specific register.
 */
 
#define R_RT	(1 << 0)	// rT field
#define R_RA	(1 << 1)	// rA field
#define R_RA_0	(1 << 2)	// rA field or none if 0
#define R_RB	(1 << 3)	// rB field
#define R_FRT	(1 << 4)	// frT field (floating-point)
#define R_FRA	(1 << 5)	// frA field
#define R_FRB	(1 << 6)	// frB field
#define R_FRC	(1 << 7)	// frC field
#define R_LR	(1 << 8)	// LR register
#define R_CTR	(1 << 9)	// CTR register
#define R_CR	(1 << 10)	// CR register
#define R_MSR	(1 << 11)	// MSR register
#define R_XER	(1 << 12)	// XER register
#define R_FPSCR	(1 << 13)	// FPSCR register
#define R_SPR	(1 << 14)	// SPR/TBR field (further decoding needed)


/*
 * Masks
 *
 * These masks isolate fields in an instruction word.
 */

#define M_LI    0x03fffffc
#define M_AA    0x00000002
#define M_LK    0x00000001
#define M_BO    0x03e00000
#define M_BI    0x001f0000
#define M_BD    0x0000fffc
#define M_RT    0x03e00000
#define M_RA    0x001f0000
#define M_RB    0x0000f800
#define M_CRFD  0x03800000
#define M_L     0x00200000
#define M_TO    0x03e00000
#define M_D     0x0000ffff
#define M_SIMM  0x0000ffff
#define M_UIMM  0x0000ffff
#define M_NB    0x0000f800
#define M_SR    0x000f0000
#define M_SH    0x0000f800
#define M_CRFS  0x001c0000
#define M_IMM   0x0000f000
#define M_CRBD  0x03e00000
#define M_RC    0x00000001
#define M_CRBA  0x001f0000
#define M_CRBB  0x0000f800
#define M_SPR   0x001FF800
#define M_TBR   0x001FF800
#define M_CRM   0x000FF000
#define M_FM    0x01FE0000
#define M_OE    0x00000400
#define M_REGC  0x000007c0
#define M_MB    0x000007c0
#define M_ME    0x0000003e
#define M_XO    0x000007fe

/*
 * Field Defining Macros
 *
 * These macros generate instruction words with their associated fields filled
 * in with the passed value.
 */

#define D_OP(op)    ((op & 0x3f) << 26)
#define D_XO(xo)    ((xo & 0x3ff) << 1)
#define D_RT(r)     ((r & 0x1f) << (31 - 10))
#define D_RA(r)		((r & 0x1f) << (31 - 15))
#define D_UIMM(u)	(u & 0xffff)

/*
 * Macros to Get Field Values
 *
 * These macros return the values of fields in an opcode. They all return
 * unsigned values and do not perform any sign extensions.
 */

#define G_RT(op)    ((op & M_RT) >> (31 - 10))
#define G_RA(op)    ((op & M_RA) >> (31 - 15))
#define G_RB(op)    ((op & M_RB) >> (31 - 20))
#define G_SIMM(op)  (op & M_SIMM)
#define G_UIMM(op)  (op & M_UIMM)
#define G_LI(op)    ((op & M_LI) >> 2)
#define G_BO(op)    ((op & M_BO) >> (31 - 10))
#define G_BI(op)    ((op & M_BI) >> (31 - 15))
#define G_BD(op)    ((op & M_BD) >> 2)
#define G_CRFD(op)  ((op & M_CRFD) >> (31 - 8))
#define G_L(op)     ((op & M_L) >> (31 - 10))
#define G_CRBD(op)  ((op & M_CRBD) >> (31 - 10))
#define G_CRBA(op)  ((op & M_CRBA) >> (31 - 15))
#define G_CRBB(op)  ((op & M_CRBB) >> (31 - 20))
#define G_REGC(op)  ((op & M_REGC) >> (31 - 25))
#define G_D(op)     (op & M_D)
#define G_NB(op)    ((op & M_NB) >> (31 - 20))
#define G_CRFS(op)  ((op & M_CRFS) >> (31 - 13))
#define G_SPR(op)   ((op & M_SPR) >> (31 - 20))
#define G_SR(op)    ((op & M_SR) >> (31 - 15))
#define G_CRM(op)   ((op & M_CRM) >> (31 - 19))
#define G_FM(op)    ((op & M_FM) >> (31 - 14))
#define G_IMM(op)   ((op & M_IMM) >> (31 - 19))
#define G_SH(op)    ((op & M_SH) >> (31 - 20))
#define G_MB(op)    ((op & M_MB) >> (31 - 25))
#define G_ME(op)    ((op & M_ME) >> 1)
#define G_TO(op)    ((op & M_TO) >> (31 - 10))

/*
 * Operand Formats
 *
 * These convey information on what operand fields are present and how they
 * ought to be printed.
 *
 * I'm fairly certain all of these are used, but that is not guaranteed.
 */

enum
{
    F_NONE,         // <no operands>
    F_LI,           // LI*4+PC if AA=0 else LI*4
    F_BCx,          // BO, BI, target_addr  used only by BCx
    F_RT_RA_0_SIMM, // rT, rA|0, SIMM       rA|0 means if rA == 0, print 0
    F_ADDIS,        // rT, rA, SIMM (printed as unsigned)   only used by ADDIS
    F_RT_RA_SIMM,   // rT, rA, SIMM         
    F_RA_RT_UIMM,   // rA, rT, UIMM         
    F_CMP_SIMM,     // crfD, L, A, SIMM
    F_CMP_UIMM,     // crfD, L, A, UIMM
    F_RT_RA_0_RB,   // rT, rA|0, rB
    F_RT_RA_RB,     // rT, rA, rB
    F_RT_D_RA_0,    // rT, d(rA|0)
    F_RT_D_RA,      // rT, d(rA)
    F_RA_RT_RB,     // rA, rT, rB
    F_FRT_D_RA_0,   // frT, d(RA|0)
    F_FRT_D_RA,     // frT, d(RA)
    F_FRT_RA_0_RB,  // frT, rA|0, rB
    F_FRT_RA_RB,    // frT, rA, rB
    F_TWI,          // TO, rA, SIMM         only used by TWI instruction
    F_CMP,          // crfD, L, rA, rB
    F_RA_RT,        // rA, rT
    F_RA_0_RB,      // rA|0, rB
    F_FRT_FRB,      // frT, frB
    F_FCMP,         // crfD, frA, frB
    F_CRFD_CRFS,    // crfD, crfS
    F_MCRXR,        // crfD                 only used by MCRXR
    F_RT,           // rT
    F_MFSR,         // rT, SR               only used by MFSR
    F_MTSR,         // SR, rT               only used by MTSR
    F_MFFSx,        // frT                  only used by MFFSx
    F_FCRBD,        // crbD                 FPSCR[crbD]
    F_MTFSFIx,      // crfD, IMM            only used by MTFSFIx
    F_RB,           // rB
    F_TW,           // TO, rA, rB           only used by TW
    F_RT_RA_0_NB,   // rT, rA|0, NB         print 32 if NB == 0
    F_SRAWIx,       // rA, rT, SH           only used by SRAWIx
    F_BO_BI,        // BO, BI
    F_CRBD_CRBA_CRBB,   // crbD, crbA, crbB
    F_RT_SPR,       // rT, SPR              and TBR
    F_MTSPR,        // SPR, rT              only used by MTSPR
    F_MTCRF,        // CRM, rT              only used by MTCRF
    F_MTFSFx,       // FM, frB              only used by MTFSFx
    F_RT_RA,        // rT, rA
    F_FRT_FRA_FRC_FRB,  // frT, frA, frC, frB
    F_FRT_FRA_FRB,  // frT, frA, frB
    F_FRT_FRA_FRC,  // frT, frA, frC
    F_RA_RT_SH_MB_ME,   // rA, rT, SH, MB, ME
    F_RLWNMx,       // rT, rA, rB, MB, ME   only used by RLWNMx
    F_RT_RB,        // rT, rB
};

/*
 * Flags
 */

#define FL_OE           (1 << 0)    // if there is an OE field
#define FL_RC           (1 << 1)    // if there is an RC field
#define FL_LK           (1 << 2)    // if there is an LK field
#define FL_AA           (1 << 3)    // if there is an AA field
#define FL_CHECK_RA_RT  (1 << 4)    // assert rA!=0 and rA!=rT
#define FL_CHECK_RA     (1 << 5)    // assert rA!=0
#define FL_CHECK_LSWI   (1 << 6)    // specific check for LSWI validity
#define FL_CHECK_LSWX   (1 << 7)    // specific check for LSWX validity
#define FL_COND_BRANCH	(1 << 8)	// a conditional branch


/*
 * Instruction Descriptor
 *
 * Describes the layout of an instruction.
 */

typedef struct
{
    CHAR    *mnem;  	// mnemonic
    UINT32  match;  	// bit pattern of instruction after it has been masked
    UINT32  mask;   	// mask of variable fields (AND with ~mask to compare w/
                    	// bit pattern to determine a match)
    INT     format; 	// operand format
    FLAGS   flags;  	// flags
    FLAGS	written;	// registers written (see register usage flags)
    FLAGS	read;		// registers read (ditto above)
} IDESCR;

/*
 * Instruction Table
 *
 * Defines all instructions.
 */

static IDESCR itab[] =
{
    // mnem		match				mask						format			flags				written		read
    { "add",    D_OP(31)|D_XO(266), M_RT|M_RA|M_RB|M_OE|M_RC,   F_RT_RA_RB,     FL_OE|FL_RC,		R_RT,		R_RA|R_RB	},
    { "addc",   D_OP(31)|D_XO(10),  M_RT|M_RA|M_RB|M_OE|M_RC,   F_RT_RA_RB,     FL_OE|FL_RC,	 	R_RT,		R_RA|R_RB	},
    { "adde",   D_OP(31)|D_XO(138), M_RT|M_RA|M_RB|M_OE|M_RC,   F_RT_RA_RB,     FL_OE|FL_RC,	 	R_RT,		R_RA|R_RB	},
    { "addi",   D_OP(14),           M_RT|M_RA|M_SIMM,           F_RT_RA_0_SIMM, 0,	           		R_RT,		R_RA		},
    { "addic",  D_OP(12),           M_RT|M_RA|M_SIMM,           F_RT_RA_SIMM,   0,	           		R_RT,		R_RA		},
    { "addic.", D_OP(13),           M_RT|M_RA|M_SIMM,           F_RT_RA_SIMM,   0,	           		R_RT,		R_RA		},
    { "addis",  D_OP(15),           M_RT|M_RA|M_SIMM,           F_ADDIS,        0,	           		R_RT,		R_RA_0		},
    { "addme",  D_OP(31)|D_XO(234), M_RT|M_RA|M_OE|M_RC,        F_RT_RA,        FL_OE|FL_RC,	 	R_RT,		R_RA|R_XER	},
    { "addze",  D_OP(31)|D_XO(202), M_RT|M_RA|M_OE|M_RC,        F_RT_RA,        FL_OE|FL_RC,	 	R_RT,		R_RA|R_XER	},
    { "and",    D_OP(31)|D_XO(28),  M_RT|M_RA|M_RB|M_RC,        F_RA_RT_RB,     FL_RC,	       		R_RA,		R_RT|R_RB	},
    { "andc",   D_OP(31)|D_XO(60),  M_RT|M_RA|M_RB|M_RC,        F_RA_RT_RB,     FL_RC,	       		R_RA,		R_RT|R_RB	},
    { "andi.",  D_OP(28),           M_RT|M_RA|M_UIMM,           F_RA_RT_UIMM,   0,           		R_RA,		R_RT		},
    { "andis.", D_OP(29),           M_RT|M_RA|M_UIMM,           F_RA_RT_UIMM,   0,           		R_RA,		R_RT		},
    { "b",      D_OP(18),           M_LI|M_AA|M_LK,             F_LI,           FL_AA|FL_LK, 		0,			0			},
    { "bc",     D_OP(16),           M_BO|M_BI|M_BD|M_AA|M_LK,   F_BCx,          FL_AA|FL_LK|FL_COND_BRANCH,	0,	0			},
    { "bcctr",  D_OP(19)|D_XO(528), M_BO|M_BI|M_LK,             F_BO_BI,        FL_LK|FL_COND_BRANCH,		0,	R_CTR       },
    { "bclr",   D_OP(19)|D_XO(16),  M_BO|M_BI|M_LK,             F_BO_BI,        FL_LK|FL_COND_BRANCH,		0,	R_LR	    },
    { "cmp",    D_OP(31)|D_XO(0),   M_CRFD|M_L|M_RA|M_RB,       F_CMP,          0,           		R_CR,		R_RA|R_RB	},
    { "cmpi",   D_OP(11),           M_CRFD|M_L|M_RA|M_SIMM,     F_CMP_SIMM,     0,           		R_CR,		R_RA		},
    { "cmpl",   D_OP(31)|D_XO(32),  M_CRFD|M_L|M_RA|M_RB,       F_CMP,          0,           		R_CR,		R_RA|R_RB	},
    { "cmpli",  D_OP(10),           M_CRFD|M_L|M_RA|M_UIMM,     F_CMP_UIMM,     0,           		R_CR,		R_RA		},
    { "cntlzw", D_OP(31)|D_XO(26),  M_RT|M_RA|M_RC,             F_RA_RT,        FL_RC,       		R_RA,		R_RT		},
    { "crand",  D_OP(19)|D_XO(257), M_CRBD|M_CRBA|M_CRBB,       F_CRBD_CRBA_CRBB,   0,       		R_CR,		R_CR		},
    { "crandc", D_OP(19)|D_XO(129), M_CRBD|M_CRBA|M_CRBB,       F_CRBD_CRBA_CRBB,   0,       		R_CR,		R_CR		},
    { "creqv",  D_OP(19)|D_XO(289), M_CRBD|M_CRBA|M_CRBB,       F_CRBD_CRBA_CRBB,   0,       		R_CR,		R_CR		},
    { "crnand", D_OP(19)|D_XO(225), M_CRBD|M_CRBA|M_CRBB,       F_CRBD_CRBA_CRBB,   0,       		R_CR,		R_CR		},
    { "crnor",  D_OP(19)|D_XO(33),  M_CRBD|M_CRBA|M_CRBB,       F_CRBD_CRBA_CRBB,   0,       		R_CR,		R_CR		},
    { "cror",   D_OP(19)|D_XO(449), M_CRBD|M_CRBA|M_CRBB,       F_CRBD_CRBA_CRBB,   0,       		R_CR,		R_CR		},
    { "crorc",  D_OP(19)|D_XO(417), M_CRBD|M_CRBA|M_CRBB,       F_CRBD_CRBA_CRBB,   0,       		R_CR,		R_CR		},
    { "crxor",  D_OP(19)|D_XO(193), M_CRBD|M_CRBA|M_CRBB,       F_CRBD_CRBA_CRBB,   0,       		R_CR,		R_CR		},
    { "dcba",   D_OP(31)|D_XO(758), M_RA|M_RB,                  F_RA_0_RB,      0,           		0,			R_RA_0|R_RB	},
    { "dcbf",   D_OP(31)|D_XO(86),  M_RA|M_RB,                  F_RA_0_RB,      0,           		0,			R_RA_0|R_RB	},
    { "dcbi",   D_OP(31)|D_XO(470), M_RA|M_RB,                  F_RA_0_RB,      0,           		0,			R_RA_0|R_RB	},
    { "dcbst",  D_OP(31)|D_XO(54),  M_RA|M_RB,                  F_RA_0_RB,      0,           		0,			R_RA_0|R_RB	},
    { "dcbt",   D_OP(31)|D_XO(278), M_RA|M_RB,                  F_RA_0_RB,      0,           		0,			R_RA_0|R_RB	},
    { "dcbtst", D_OP(31)|D_XO(246), M_RA|M_RB,                  F_RA_0_RB,      0,           		0,			R_RA_0|R_RB	},
    { "dcbz",   D_OP(31)|D_XO(1014),M_RA|M_RB,                  F_RA_0_RB,      0,           		0,			R_RA_0|R_RB	},
    { "divw",   D_OP(31)|D_XO(491), M_RT|M_RA|M_RB|M_OE|M_RC,   F_RT_RA_RB,     FL_OE|FL_RC, 		R_RT,		R_RA|R_RB	},
    { "divwu",  D_OP(31)|D_XO(459), M_RT|M_RA|M_RB|M_OE|M_RC,   F_RT_RA_RB,     FL_OE|FL_RC, 		R_RT,		R_RA|R_RB	},
    { "eciwx",  D_OP(31)|D_XO(310), M_RT|M_RA|M_RB,             F_RT_RA_0_RB,   0,           		R_RT,		R_RA_0|R_RB	},		// SPR EAR is used but we ignore it
    { "ecowx",  D_OP(31)|D_XO(438), M_RT|M_RA|M_RB,             F_RT_RA_0_RB,   0,           		0,			R_RT|R_RA_0|R_RB },	// ""
    { "eieio",  D_OP(31)|D_XO(854), 0,                          F_NONE,         0,           		0,			0			},
    { "eqv",    D_OP(31)|D_XO(284), M_RT|M_RA|M_RB|M_RC,        F_RA_RT_RB,     FL_RC,       		R_RA,		R_RT|R_RB	},
    { "extsb",  D_OP(31)|D_XO(954), M_RT|M_RA|M_RC,             F_RA_RT,        FL_RC,       		R_RA,		R_RT		},
    { "extsh",  D_OP(31)|D_XO(922), M_RT|M_RA|M_RC,             F_RA_RT,        FL_RC,       		R_RA,		R_RT		},
    { "fabs",   D_OP(63)|D_XO(264), M_RT|M_RB|M_RC,             F_FRT_FRB,      FL_RC,       		R_FRT,		R_FRB		},
    { "fadd",   D_OP(63)|D_XO(21),  M_RT|M_RA|M_RB|M_RC,        F_FRT_FRA_FRB,  FL_RC,       		R_FRT,		R_FRA|R_FRB	},
    { "fadds",  D_OP(59)|D_XO(21),  M_RT|M_RA|M_RB|M_RC,        F_FRT_FRA_FRB,  FL_RC,       		R_FRT,		R_FRA|R_FRB	},
    { "fcmpo",  D_OP(63)|D_XO(32),  M_CRFD|M_RA|M_RB,           F_FCMP,         0,           		R_CR,		R_FRA|R_FRB	},		// FPCC ignored
    { "fcmpu",  D_OP(63)|D_XO(0),   M_CRFD|M_RA|M_RB,           F_FCMP,         0,           		R_CR,		R_FRA|R_FRB	},		// ""
    { "fctiw",  D_OP(63)|D_XO(14),  M_RT|M_RB|M_RC,             F_FRT_FRB,      FL_RC,       		R_FRT,		R_FRB		},
    { "fctiwz", D_OP(63)|D_XO(15),  M_RT|M_RB|M_RC,             F_FRT_FRB,      FL_RC,       		R_FRT,		R_FRB		},
    { "fdiv",   D_OP(63)|D_XO(18),  M_RT|M_RA|M_RB|M_RC,        F_FRT_FRA_FRB,  FL_RC,       		R_FRT,		R_FRA|R_FRB	},
    { "fdivs",  D_OP(59)|D_XO(18),  M_RT|M_RA|M_RB|M_RC,        F_FRT_FRA_FRB,  FL_RC,       		R_FRT,		R_FRA|R_FRB	},
    { "fmadd",  D_OP(63)|D_XO(29),  M_RT|M_RA|M_RB|M_REGC|M_RC, F_FRT_FRA_FRC_FRB,  FL_RC,   		R_FRT,		R_FRA|R_FRC|R_FRB },
    { "fmadds", D_OP(59)|D_XO(29),  M_RT|M_RA|M_RB|M_REGC|M_RC, F_FRT_FRA_FRC_FRB,  FL_RC,   		R_FRT,		R_FRA|R_FRC|R_FRB },
    { "fmr",    D_OP(63)|D_XO(72),  M_RT|M_RB|M_RC,             F_FRT_FRB,      FL_RC,       		R_FRT,		R_FRB 		},
    { "fmsub",  D_OP(63)|D_XO(28),  M_RT|M_RA|M_RB|M_REGC|M_RC, F_FRT_FRA_FRC_FRB,  FL_RC,   		R_FRT,		R_FRA|R_FRC|R_FRB },
    { "fmsubs", D_OP(59)|D_XO(28),  M_RT|M_RA|M_RB|M_REGC|M_RC, F_FRT_FRA_FRC_FRB,  FL_RC,   		R_FRT,		R_FRA|R_FRC|R_FRB },
    { "fmul",   D_OP(63)|D_XO(25),  M_RT|M_RA|M_REGC|M_RC,      F_FRT_FRA_FRC,  FL_RC,       		R_FRT,		R_FRA|R_FRC	},
    { "fmuls",  D_OP(59)|D_XO(25),  M_RT|M_RA|M_REGC|M_RC,      F_FRT_FRA_FRC,  FL_RC,       		R_FRT,		R_FRA|R_FRC	},
    { "fnabs",  D_OP(63)|D_XO(136), M_RT|M_RB|M_RC,             F_FRT_FRB,      FL_RC,       		R_FRT,		R_FRB		},
    { "fneg",   D_OP(63)|D_XO(40),  M_RT|M_RB|M_RC,             F_FRT_FRB,      FL_RC,       		R_FRT,		R_FRB		},
    { "fnmadd", D_OP(63)|D_XO(31),  M_RT|M_RA|M_RB|M_REGC|M_RC, F_FRT_FRA_FRC_FRB,  FL_RC,   		R_FRT,		R_FRA|R_FRC|R_FRB },
    { "fnmadds",D_OP(59)|D_XO(31),  M_RT|M_RA|M_RB|M_REGC|M_RC, F_FRT_FRA_FRC_FRB,  FL_RC,   		R_FRT,		R_FRA|R_FRC|R_FRB },
    { "fnmsub", D_OP(63)|D_XO(30),  M_RT|M_RA|M_RB|M_REGC|M_RC, F_FRT_FRA_FRC_FRB,  FL_RC,   		R_FRT,		R_FRA|R_FRC|R_FRB },
    { "fnmsubs",D_OP(59)|D_XO(30),  M_RT|M_RA|M_RB|M_REGC|M_RC, F_FRT_FRA_FRC_FRB,  FL_RC,   		R_FRT,		R_FRA|R_FRC|R_FRB },
    { "fres",   D_OP(59)|D_XO(24),  M_RT|M_RB|M_RC,             F_FRT_FRB,      FL_RC,       		R_FRT,		R_FRB		},
    { "frsp",   D_OP(63)|D_XO(12),  M_RT|M_RB|M_RC,             F_FRT_FRB,      FL_RC,       		R_FRT,		R_FRB		},
    { "frsqrte",D_OP(63)|D_XO(26),  M_RT|M_RB|M_RC,             F_FRT_FRB,      FL_RC,       		R_FRT,		R_FRB		},
    { "fsel",   D_OP(63)|D_XO(23),  M_RT|M_RA|M_RB|M_REGC|M_RC, F_FRT_FRA_FRC_FRB,  FL_RC,   		R_FRT,		R_FRA|R_FRC|R_FRB },
    { "fsqrt",  D_OP(63)|D_XO(22),  M_RT|M_RB|M_RC,             F_FRT_FRB,      FL_RC,       		R_FRT,		R_FRB		},
    { "fsqrts", D_OP(59)|D_XO(22),  M_RT|M_RB|M_RC,             F_FRT_FRB,      FL_RC,       		R_FRT,		R_FRB		},
    { "fsub",   D_OP(63)|D_XO(20),  M_RT|M_RA|M_RB|M_RC,        F_FRT_FRA_FRB,  FL_RC,       		R_FRT,		R_FRA|R_FRB	},
    { "fsubs",  D_OP(59)|D_XO(20),  M_RT|M_RA|M_RB|M_RC,        F_FRT_FRA_FRB,  FL_RC,       		R_FRT,		R_FRA|R_FRB },
    { "icbi",   D_OP(31)|D_XO(982), M_RA|M_RB,                  F_RA_0_RB,      0,           		0,			R_RA_0|R_RB	},
    { "isync",  D_OP(19)|D_XO(150), 0,                          F_NONE,         0,           		0,			0			},
    { "lbz",    D_OP(34),           M_RT|M_RA|M_D,              F_RT_D_RA_0,    0,           		R_RT,		R_RA_0		},
    { "lbzu",   D_OP(35),           M_RT|M_RA|M_D,              F_RT_D_RA,      FL_CHECK_RA_RT, 	R_RT|R_RA,	R_RA		},
    { "lbzux",  D_OP(31)|D_XO(119), M_RT|M_RA|M_RB,             F_RT_RA_RB,     FL_CHECK_RA_RT, 	R_RT|R_RA,  R_RA|R_RB	},
    { "lbzx",   D_OP(31)|D_XO(87),  M_RT|M_RA|M_RB,             F_RT_RA_0_RB,   0,          		R_RT, 		R_RA_0|R_RB	},
    { "lfd",    D_OP(50),           M_RT|M_RA|M_D,              F_FRT_D_RA_0,   0,           		R_FRT, 		R_RA_0		},
    { "lfdu",   D_OP(51),           M_RT|M_RA|M_D,              F_FRT_D_RA,     FL_CHECK_RA, 		R_FRT|R_RA,	R_RA		},
    { "lfdux",  D_OP(31)|D_XO(631), M_RT|M_RA|M_RB,             F_FRT_RA_RB,    FL_CHECK_RA, 		R_FRT|R_RA,	R_RA|R_RB	},
    { "lfdx",   D_OP(31)|D_XO(599), M_RT|M_RA|M_RB,             F_FRT_RA_0_RB,  0,           		R_FRT, 		R_RA_0|R_RB	},
    { "lfs",    D_OP(48),           M_RT|M_RA|M_D,              F_FRT_D_RA_0,   0,           		R_FRT, 		R_RA_0		},
    { "lfsu",   D_OP(49),           M_RT|M_RA|M_D,              F_FRT_D_RA,     FL_CHECK_RA, 		R_FRT|R_RA,	R_RA		},
    { "lfsux",  D_OP(31)|D_XO(567), M_RT|M_RA|M_RB,             F_FRT_RA_RB,    FL_CHECK_RA, 		R_FRT|R_RA,	R_RA|R_RB	},
    { "lfsx",   D_OP(31)|D_XO(535), M_RT|M_RA|M_RB,             F_FRT_RA_0_RB,  0,           		R_FRT, 		R_RA_0|R_RB	},
    { "lha",    D_OP(42),           M_RT|M_RA|M_D,              F_RT_D_RA_0,    0,           		R_RT, 		R_RA_0		},
    { "lhau",   D_OP(43),           M_RT|M_RA|M_D,              F_RT_D_RA,      FL_CHECK_RA_RT, 	R_RT|R_RA,	R_RA		},
    { "lhaux",  D_OP(31)|D_XO(375), M_RT|M_RA|M_RB,             F_RT_RA_RB,     FL_CHECK_RA_RT, 	R_RT|R_RA,	R_RA|R_RB	},
    { "lhax",   D_OP(31)|D_XO(343), M_RT|M_RA|M_RB,             F_RT_RA_0_RB,   0,           		R_RT, 		R_RA_0|R_RB	},
    { "lhbrx",  D_OP(31)|D_XO(790), M_RT|M_RA|M_RB,             F_RT_RA_0_RB,   0,           		R_RT, 		R_RA_0|R_RB	},
    { "lhz",    D_OP(40),           M_RT|M_RA|M_D,              F_RT_D_RA_0,    0,           		R_RT, 		R_RA_0		},
    { "lhzu",   D_OP(41),           M_RT|M_RA|M_D,              F_RT_D_RA,      FL_CHECK_RA_RT, 	R_RT|R_RA,	R_RA		},
    { "lhzux",  D_OP(31)|D_XO(311), M_RT|M_RA|M_RB,             F_RT_RA_RB,     FL_CHECK_RA_RT, 	R_RT|R_RA,	R_RA|R_RB	},
    { "lhzx",   D_OP(31)|D_XO(279), M_RT|M_RA|M_RB,             F_RT_RA_0_RB,   0,           		R_RT, 		R_RA_0|R_RB	},
    { "lmw",    D_OP(46),           M_RT|M_RA|M_D,              F_RT_D_RA_0,    0,           		R_RT, 		R_RA_0		},		// multiple registers are actually loaded
    { "lswi",   D_OP(31)|D_XO(597), M_RT|M_RA|M_NB,             F_RT_RA_0_NB,   FL_CHECK_LSWI, 		R_RT, 		R_RA_0		},		// ""
    { "lswx",   D_OP(31)|D_XO(533), M_RT|M_RA|M_RB,             F_RT_RA_0_RB,   FL_CHECK_LSWX, 		R_RT, 		R_RA_0|R_RB	},		// ""
    { "lwarx",  D_OP(31)|D_XO(20),  M_RT|M_RA|M_RB,             F_RT_RA_0_RB,   0,           		R_RT, 		R_RA_0|R_RB	},
    { "lwbrx",  D_OP(31)|D_XO(534), M_RT|M_RA|M_RB,             F_RT_RA_0_RB,   0,           		R_RT, 		R_RA_0|R_RB	},
    { "lwz",    D_OP(32),           M_RT|M_RA|M_D,              F_RT_D_RA_0,    0,           		R_RT, 		R_RA_0		},
    { "lwzu",   D_OP(33),           M_RT|M_RA|M_D,              F_RT_D_RA,      FL_CHECK_RA_RT, 	R_RT|R_RA,	R_RA		},
    { "lwzux",  D_OP(31)|D_XO(55),  M_RT|M_RA|M_RB,             F_RT_RA_RB,     FL_CHECK_RA_RT, 	R_RT|R_RA,	R_RA|R_RB	},
    { "lwzx",   D_OP(31)|D_XO(23),  M_RT|M_RA|M_RB,             F_RT_RA_0_RB,   0,           		R_RT, 		R_RA_0|R_RB	},
    { "mcrf",   D_OP(19)|D_XO(0),   M_CRFD|M_CRFS,              F_CRFD_CRFS,    0,           		R_CR,		R_CR		},
    { "mcrfs",  D_OP(63)|D_XO(64),  M_CRFD|M_CRFS,              F_CRFD_CRFS,    0,           		R_CR,		R_CR		},
    { "mcrxr",  D_OP(31)|D_XO(512), M_CRFD,                     F_MCRXR,        0,           		R_CR,		R_XER		},
    { "mfcr",   D_OP(31)|D_XO(19),  M_RT,                       F_RT,           0,           		R_RT,		R_CR		},
    { "mffs",   D_OP(63)|D_XO(583), M_RT|M_RC,                  F_MFFSx,        FL_RC,       		R_FRT,		R_FPSCR		},
    { "mfmsr",  D_OP(31)|D_XO(83),  M_RT,                       F_RT,           0,           		R_RT,		R_MSR		},
    { "mfspr",  D_OP(31)|D_XO(339), M_RT|M_SPR,                 F_RT_SPR,       0,           		R_RT,		R_SPR		},
    { "mfsr",   D_OP(31)|D_XO(595), M_RT|M_SR,                  F_MFSR,         0,           		R_RT,		0			},		// SR not kept track of
    { "mfsrin", D_OP(31)|D_XO(659), M_RT|M_RB,                  F_RT_RB,        0,           		R_RT,		R_RB		},		// ""
    { "mftb",   D_OP(31)|D_XO(371), M_RT|M_TBR,                 F_RT_SPR,       0,           		R_RT,		R_SPR		},		// TBR field == SPR field
    { "mtcrf",  D_OP(31)|D_XO(144), M_RT|M_CRM,                 F_MTCRF,        0,           		R_CR,		R_RT		},
    { "mtfsb0", D_OP(63)|D_XO(70),  M_CRBD|M_RC,                F_FCRBD,        FL_RC,       		R_FPSCR,	0			},
    { "mtfsb1", D_OP(63)|D_XO(38),  M_CRBD|M_RC,                F_FCRBD,        FL_RC,       		R_FPSCR,	0			},
    { "mtfsf",  D_OP(63)|D_XO(711), M_FM|M_RB|M_RC,             F_MTFSFx,       FL_RC,       		R_FPSCR,	R_FRB		},
    { "mtfsfi", D_OP(63)|D_XO(134), M_CRFD|M_IMM|M_RC,          F_MTFSFIx,      FL_RC,       		R_FPSCR,	0			},
    { "mtmsr",  D_OP(31)|D_XO(146), M_RT,                       F_RT,           0,           		R_MSR,		R_RT		},
    { "mtspr",  D_OP(31)|D_XO(467), M_RT|M_SPR,                 F_MTSPR,        0,           		R_SPR,		R_RT		},
    { "mtsr",   D_OP(31)|D_XO(210), M_RT|M_SR,                  F_MTSR,         0,           		0,			R_RT		},		// SR not kept track of
    { "mtsrin", D_OP(31)|D_XO(242), M_RT|M_RB,                  F_RT_RB,        0,           		0,			R_RT|R_RB	},		// ""
    { "mulhw",  D_OP(31)|D_XO(75),  M_RT|M_RA|M_RB|M_RC,        F_RT_RA_RB,     FL_RC,       		R_RT,		R_RA|R_RB	},
    { "mulhwu", D_OP(31)|D_XO(11),  M_RT|M_RA|M_RB|M_RC,        F_RT_RA_RB,     FL_RC,       		R_RT,		R_RA|R_RB	},
    { "mulli",  D_OP(7),            M_RT|M_RA|M_SIMM,           F_RT_RA_SIMM,   0,           		R_RT,		R_RA		},
    { "mullw",  D_OP(31)|D_XO(235), M_RT|M_RA|M_RB|M_OE|M_RC,   F_RT_RA_RB,     FL_OE|FL_RC, 		R_RT,		R_RA|R_RB	},
    { "nand",   D_OP(31)|D_XO(476), M_RA|M_RT|M_RB|M_RC,        F_RA_RT_RB,     FL_RC,       		R_RA,		R_RT|R_RB	},
    { "neg",    D_OP(31)|D_XO(104), M_RT|M_RA|M_OE|M_RC,        F_RT_RA,        FL_OE|FL_RC, 		R_RT,		R_RA		},
    { "nor",    D_OP(31)|D_XO(124), M_RT|M_RA|M_RB|M_RC,        F_RA_RT_RB,     FL_RC,       		R_RA,		R_RT|R_RB	},
    { "or",     D_OP(31)|D_XO(444), M_RT|M_RA|M_RB|M_RC,        F_RA_RT_RB,     FL_RC,       		R_RA,		R_RT|R_RB	},
    { "orc",    D_OP(31)|D_XO(412), M_RT|M_RA|M_RB|M_RC,        F_RA_RT_RB,     FL_RC,       		R_RA,		R_RT|R_RB	},
    { "ori",    D_OP(24),           M_RT|M_RA|M_UIMM,           F_RA_RT_UIMM,   0,           		R_RA,		R_RT		},
    { "oris",   D_OP(25),           M_RT|M_RA|M_UIMM,           F_RA_RT_UIMM,   0,           		R_RA,		R_RT		},
    { "rfi",    D_OP(19)|D_XO(50),  0,                          F_NONE,         0,           		R_MSR,		0			},		// SRR0, SRR1 not kept track of
    { "rlwimi", D_OP(20),           M_RT|M_RA|M_SH|M_MB|M_ME|M_RC,  F_RA_RT_SH_MB_ME,   FL_RC,   	R_RA,		R_RT		},
    { "rlwinm", D_OP(21),           M_RT|M_RA|M_SH|M_MB|M_ME|M_RC,  F_RA_RT_SH_MB_ME,   FL_RC,   	R_RA,		R_RT		},
    { "rlwnm",  D_OP(23),           M_RT|M_RA|M_RB|M_MB|M_ME|M_RC,  F_RLWNMx,   FL_RC,       		R_RA,		R_RT|R_RB	},
    { "sc",     D_OP(17)|2,         0,                          F_NONE,         0,           		R_MSR,		0			},		// SRR0, SRR1 not kept track of
    { "slw",    D_OP(31)|D_XO(24),  M_RT|M_RA|M_RB|M_RC,        F_RA_RT_RB,     FL_RC,       		R_RA,		R_RT|R_RB	},
    { "sraw",   D_OP(31)|D_XO(792), M_RT|M_RA|M_RB|M_RC,        F_RA_RT_RB,     FL_RC,       		R_RA,		R_RT|R_RB	},
    { "srawi",  D_OP(31)|D_XO(824), M_RT|M_RA|M_SH|M_RC,        F_SRAWIx,       FL_RC,       		R_RA,		R_RT		},
    { "srw",    D_OP(31)|D_XO(536), M_RT|M_RA|M_RB|M_RC,        F_RA_RT_RB,     FL_RC,       		R_RA,		R_RT|R_RB	},
    { "stb",    D_OP(38),           M_RT|M_RA|M_D,              F_RT_D_RA_0,    0,           		0,   		R_RA_0 		},
    { "stbu",   D_OP(39),           M_RT|M_RA|M_D,              F_RT_D_RA,      FL_CHECK_RA, 		R_RA,  		R_RA		},
    { "stbux",  D_OP(31)|D_XO(247), M_RT|M_RA|M_RB,             F_RT_RA_RB,     FL_CHECK_RA, 		R_RA,    	R_RA|R_RB	},
    { "stbx",   D_OP(31)|D_XO(215), M_RT|M_RA|M_RB,             F_RT_RA_0_RB,   0,           		0,   		R_RA_0|R_RB	},
    { "stfd",   D_OP(54),           M_RT|M_RA|M_D,              F_FRT_D_RA_0,   0,           		0,	    	R_RA_0		},
    { "stfdu",  D_OP(55),           M_RT|M_RA|M_D,              F_FRT_D_RA,     FL_CHECK_RA, 		R_RA,    	R_RA|R_RB	},
    { "stfdux", D_OP(31)|D_XO(759), M_RT|M_RA|M_RB,             F_FRT_RA_RB,    FL_CHECK_RA, 		R_RA,    	R_RA|R_RB	},
    { "stfdx",  D_OP(31)|D_XO(727), M_RT|M_RA|M_RB,             F_FRT_RA_0_RB,  0,           		0,	    	R_RA_0|R_RB	},
    { "stfiwx", D_OP(31)|D_XO(983), M_RT|M_RA|M_RB,             F_FRT_RA_0_RB,  0,           		0,	    	R_RA_0|R_RB	},
    { "stfs",   D_OP(52),           M_RT|M_RA|M_D,              F_FRT_D_RA_0,   0,           		0,	    	R_RA_0		},
    { "stfsu",  D_OP(53),           M_RT|M_RA|M_D,              F_FRT_D_RA,     FL_CHECK_RA, 		R_RA,    	R_RA		},
    { "stfsux", D_OP(31)|D_XO(695), M_RT|M_RA|M_RB,             F_FRT_RA_RB,    FL_CHECK_RA, 		R_RA,    	R_RA|R_RB	},
    { "stfsx",  D_OP(31)|D_XO(663), M_RT|M_RA|M_RB,             F_FRT_RA_0_RB,  0,           		0,	    	R_RA_0|R_RB	},
    { "sth",    D_OP(44),           M_RT|M_RA|M_D,              F_RT_D_RA_0,    0,           		0,	    	R_RA_0		},
    { "sthbrx", D_OP(31)|D_XO(918), M_RT|M_RA|M_RB,             F_RT_RA_0_RB,   0,           		0,	    	R_RA_0|R_RB	},
    { "sthu",   D_OP(45),           M_RT|M_RA|M_D,              F_RT_D_RA,      FL_CHECK_RA, 		R_RA,    	R_RA		},
    { "sthux",  D_OP(31)|D_XO(439), M_RT|M_RA|M_RB,             F_RT_RA_RB,     FL_CHECK_RA, 		R_RA,    	R_RA|R_RB	},
    { "sthx",   D_OP(31)|D_XO(407), M_RT|M_RA|M_RB,             F_RT_RA_0_RB,   0,           		0,	    	R_RA_0|R_RB	},
    { "stmw",   D_OP(47),           M_RT|M_RA|M_D,              F_RT_D_RA_0,    0,           		0,	    	R_RA_0		},		// multiple registers are actually stored
    { "stswi",  D_OP(31)|D_XO(725), M_RT|M_RA|M_NB,             F_RT_RA_0_NB,   0,           		0,	    	R_RA_0		},		// ""
    { "stswx",  D_OP(31)|D_XO(661), M_RT|M_RA|M_RB,             F_RT_RA_0_RB,   0,          		0,	    	R_RA_0|R_RB	},		// ""
    { "stw",    D_OP(36),           M_RT|M_RA|M_D,              F_RT_D_RA_0,    0,           		0,	    	R_RA_0		},
    { "stwbrx", D_OP(31)|D_XO(662), M_RT|M_RA|M_RB,             F_RT_RA_0_RB,   0,           		0,	    	R_RA_0|R_RB	},
    { "stwcx.", D_OP(31)|D_XO(150)|1,   M_RT|M_RA|M_RB,         F_RT_RA_0_RB,   0,           		0,	    	R_RA_0|R_RB	},
    { "stwu",   D_OP(37),           M_RT|M_RA|M_D,              F_RT_D_RA,      FL_CHECK_RA, 		R_RA,    	R_RA		},
    { "stwux",  D_OP(31)|D_XO(183), M_RT|M_RA|M_RB,             F_RT_RA_RB,     FL_CHECK_RA, 		R_RA,    	R_RA|R_RB	},
    { "stwx",   D_OP(31)|D_XO(151), M_RT|M_RA|M_RB,             F_RT_RA_0_RB,   0,           		0,	    	R_RA_0|R_RB	},
    { "subf",   D_OP(31)|D_XO(40),  M_RT|M_RA|M_RB|M_OE|M_RC,   F_RT_RA_RB,     FL_OE|FL_RC, 		R_RT,		R_RA|R_RB	},
    { "subfc",  D_OP(31)|D_XO(8),   M_RT|M_RA|M_RB|M_OE|M_RC,   F_RT_RA_RB,     FL_OE|FL_RC, 		R_RT,		R_RA|R_RB	},
    { "subfe",  D_OP(31)|D_XO(136), M_RT|M_RA|M_RB|M_OE|M_RC,   F_RT_RA_RB,     FL_OE|FL_RC, 		R_RT,		R_RA|R_RB	},
    { "subfic", D_OP(8),            M_RT|M_RA|M_SIMM,           F_RT_RA_SIMM,   0,           		R_RT,		R_RA		},
    { "subfme", D_OP(31)|D_XO(232), M_RT|M_RA|M_OE|M_RC,        F_RT_RA,        FL_OE|FL_RC, 		R_RT,		R_RA		},
    { "subfze", D_OP(31)|D_XO(200), M_RT|M_RA|M_OE|M_RC,        F_RT_RA,        FL_OE|FL_RC, 		R_RT,		R_RA		},
    { "sync",   D_OP(31)|D_XO(598), 0,                          F_NONE,         0,           		0,			0			},
    { "tlbia",  D_OP(31)|D_XO(370), 0,                          F_NONE,         0,           		0,			0			},
    { "tlbie",  D_OP(31)|D_XO(306), M_RB,                       F_RB,           0,           		0,			R_RB		},
    { "tlbsync",D_OP(31)|D_XO(566), 0,                          F_NONE,         0,           		0,			0			},
    { "tw",     D_OP(31)|D_XO(4),   M_TO|M_RA|M_RB,             F_TW,           0,           		0,			R_RA|R_RB	},
    { "twi",    D_OP(3),            M_TO|M_RA|M_SIMM,           F_TWI,          0,           		0,			R_RA		},
    { "xor",    D_OP(31)|D_XO(316), M_RT|M_RA|M_RB|M_RC,        F_RA_RT_RB,     FL_RC,       		R_RA,		R_RT|R_RB	},
    { "xori",   D_OP(26),           M_RT|M_RA|M_UIMM,           F_RA_RT_UIMM,   0,           		R_RA,		R_RT		},
    { "xoris",  D_OP(27),           M_RT|M_RA|M_UIMM,           F_RA_RT_UIMM,   0,           		R_RA,		R_RT		},

    /*
     * PowerPC 603e/EC603e-specific instructions
     */

    { "tlbld",  D_OP(31)|D_XO(978), M_RB,                       F_RB,           0,           		0,			R_RB		},
    { "tlbli",  D_OP(31)|D_XO(1010),M_RB,                       F_RB,           0,          		0,			R_RB		}
};


/******************************************************************/
/* Analysis Routines                                              */
/******************************************************************/

void determine_regusage(PPC_REGUSAGE *ru, FLAGS regs, UINT32 op)
{
	UINT32	spr_field, spr;
	
	if (regs & R_RT)
		++ru->r[G_RT(op)];
	if (regs & R_RA)
		++ru->r[G_RA(op)];
	if ((regs & R_RA_0) && (G_RA(op) != 0))	// if rA field is not 0, refers to a GPR #
		++ru->r[G_RA(op)];
	if (regs & R_RB)
		++ru->r[G_RB(op)];
	if (regs & R_FRT)
		++ru->f[G_RT(op)];
	if (regs & R_FRA)
		++ru->f[G_RA(op)];
	if (regs & R_FRB)
		++ru->f[G_RB(op)];
	if (regs & R_FRC)
		++ru->f[G_REGC(op)];
	if (regs & R_LR)
		++ru->lr;
	if (regs & R_CTR)
		++ru->ctr;
	if (regs & R_CR)
		++ru->cr;
	if (regs & R_MSR)
		++ru->msr;
	if (regs & R_FPSCR)	// this is still pretty inaccurate, only handled for explicit cases
		++ru->fpscr;
	if (regs & R_XER)
		++ru->xer;
		
	/*
	 * Only some SPRs are currently handled. All SPR definitions come from 
	 * ppc.h
	 */
	 
	if (regs & R_SPR) 
	{
		/*
		 * Construct the SPR number -- SPR field is actually 2 5-bit fields
		 * that are reversed
		 */
		 
    	spr_field = G_SPR(op);
    	spr = (spr_field >> 5) & 0x1F;
    	spr |= (spr_field & 0x1F) << 5;
    	
		switch (spr)
		{
		case 1:		++ru->xer;	break;	// XER
		case 8:		++ru->lr;	break;	// LR
		case 9:		++ru->ctr;	break;	// CTR
		case 22:	++ru->dec;	break;	// DEC
		case 284:	++ru->tbl;	break;	// TBL
		case 285:	++ru->tbu;	break;	// TBU
		default:				break;	// anything else is currently ignored
		}
	}
}

/*
 * void ppc_analyze_regusage(PPC_REGUSAGE *written, PPC_REGUSAGE *read, UINT32 op);
 *
 * Analyzes the passed op and determines which registers were read and written.
 * The appropriate fields of "written" and "read" are incremented as needed.
 *
 * Parameters:
 *		written = Records which registers were written (the appropriate fields
 *				  are incremented.) This is assumed to have been initialized.
 *		read    = Records which registers are read.
 */
 
void ppc_analyze_regusage(PPC_REGUSAGE *written, PPC_REGUSAGE *read, UINT32 op)
{
	INT	i;
	
	/*
     * Search for the instruction in the list and then analyze it
     */

    for (i = 0; i < sizeof(itab) / sizeof(IDESCR); i++)
    {        
        if ((op & ~itab[i].mask) == itab[i].match)  // check for match
        {
        	/*
        	 * Test for implicit LR write
        	 */
        	 
       		if ((itab[i].flags & FL_LK) && (op & M_LK))
       			++written->lr;
       		
       		/*
       		 * Test for condition branch to check BO field
       		 */
       		 
       		if (itab[i].flags & FL_COND_BRANCH)
       		{
       			switch (G_BO(op))
       			{			// BO field:
       			case 0:		// 0000	      			
       			case 1:		// 0001	
       			case 4:		// 0100       			
       			case 5:		// 0101
       			case 8:		// 1z00
       			case 0xC:       			
       			case 9:		// 1z01
       			case 0xD:
       				++read->ctr;
       				break;
       			default:
       				break;       				
				}       			
       		}
       	
       		/*
       		 * Handle specified read/write data
       		 */
       		 
			determine_regusage(written, itab[i].written, op);
			determine_regusage(read, itab[i].read, op);			
        }
    }
}

/*
 * UINT ppc_get_num_regs_used(PPC_REGUSAGE *ru);
 *
 * Counts how many different registers were used (accessed at least once.)
 *
 * Parameters:
 *		ru = Register usage data.
 *
 * Returns:
 *		The number of different registers that were accessed.
 */
 
UINT ppc_get_num_regs_used(PPC_REGUSAGE *ru)
{
	UINT	num = 0;
	INT		i;
	
	for (i = 0; i < 32; i++)
	{
		if (ru->r[i] != 0)	++num;
		if (ru->f[i] != 0)	++num;
	}
	
	if (ru->lr != 0)	++num;
	if (ru->ctr != 0)	++num;
	if (ru->cr != 0)	++num;
	if (ru->xer != 0)	++num;
	if (ru->msr != 0)	++num;
	if (ru->fpscr != 0)	++num;
	if (ru->tbu != 0)	++num;
	if (ru->tbl != 0)	++num;
	if (ru->dec != 0)	++num;
	
	return num;
}

/*
 * void ppc_add_regusage_data(PPC_REGUSAGE *dest, PPC_REGUSAGE *src1, PPC_REGUSAGE *src2);
 *
 * Adds two PPC_REGUSAGE structures together (adds each member.)
 *
 * Parameters:
 *		dest = Pointer to where results are stored. Must not be the same as
 *			   either of the sources!
 *		src1 = First source for addition.
 *		src2 = Second source for addition.
 */
 
void ppc_add_regusage_data(PPC_REGUSAGE *dest, PPC_REGUSAGE *src1, PPC_REGUSAGE *src2)
{
	INT	i;
	
	for (i = 0; i < 32; i++)
	{
		dest->r[i] = src1->r[i] + src2->r[i];
		dest->f[i] = src1->f[i] + src2->f[i];
	}
	
	dest->lr = src1->lr + src2->lr;
	dest->ctr = src1->ctr + src2->ctr;	
	dest->cr = src1->cr + src2->cr;
	dest->xer = src1->xer + src2->xer;
	dest->msr = src1->msr + src2->msr;
	dest->fpscr = src1->fpscr + src2->fpscr;
	dest->tbu = src1->tbu + src2->tbu;
	dest->tbl = src1->tbl + src2->tbl;
	dest->dec = src1->dec + src2->dec;
}

/******************************************************************/
/* Output                                                         */
/******************************************************************/

/*
 * void ppc_print_regusage(const CHAR *log_file, PPC_REGUSAGE *ru);
 *
 * Prints the register usage counts out indented with one tab stop. 
 *
 * Parameters:
 *		log_file = Name of file to log to (using LOG() macro defined in model3.h.)
 *		ru       = Pointer to register usage structure.
 */
 
void ppc_print_regusage(const CHAR *log_file, PPC_REGUSAGE *ru)
{
	INT	i;
	
	for (i = 0; i < 32; i++)
	{
		if (ru->r[i] != 0)
			LOG(log_file, "\tR%d=%d\n", i, ru->r[i]);
	}
	
	for (i = 0; i < 32; i++)
	{
		if (ru->f[i] != 0)
			LOG(log_file, "\tF%d=%d\n", i, ru->f[i]);
	}
	
	if (ru->lr != 0)	LOG(log_file, "\tLR=%d\n", ru->lr);
	if (ru->ctr != 0)	LOG(log_file, "\tCTR=%d\n", ru->ctr);	
	if (ru->cr != 0)	LOG(log_file, "\tCR=%d\n", ru->cr);
	if (ru->xer != 0)	LOG(log_file, "\tXER=%d\n", ru->xer);
	if (ru->msr != 0)	LOG(log_file, "\tMSR=%d\n", ru->msr);
	if (ru->fpscr != 0)	LOG(log_file, "\tFPSCR=%d\n", ru->fpscr);
	if (ru->tbu != 0)	LOG(log_file, "\tTBU=%d\n", ru->tbu);
	if (ru->tbl != 0)	LOG(log_file, "\tTBL=%d\n", ru->tbl);
	if (ru->dec != 0)	LOG(log_file, "\tDEC=%d\n", ru->dec);
}