/*
 * front/powerpc/powerpc.c
 *
 * PowerPC front-end. It comprises an interpreter and a translator (althought
 * only a portion of it is hosted here, namely the PowerPC-to-IR decoder.)
 */

/*
 * NOTES:
 *
 *	- Whenever the context is updated, at initialization or on SetContext and
 *	  LoadState, MMap_Setup (&ctx->mmap) must be called to aknowledge the memory
 *	  map system of the (possible) memory map change.
 *
 *	- There are many other things to write, but none comes to mind. :)
 */

#include <string.h>		// memcpy in GetContext, SetContext

#include "drppc.h"
#include "toplevel.h"
#include "mempool.h"
#include "mmap.h"
#include "bblookup.h"
#include "middle/ir.h"
#include TARGET_CPU_HEADER

#include "powerpc.h"
#include "internal.h"
#include "interp/interp.h"		// I_* instruction handlers
#include "decode/decode.h"		// D_* instruction handlers
#include "ppcdasm.h"


static INT Decode (UINT32, DRPPC_BB *);

/*******************************************************************************
 Configuration
*******************************************************************************/

#define PRINT_POWERPC_DISASM	1


/*******************************************************************************
 Variables

 There are both global and local variables here. The layout in memory is
 important for cache issues.
*******************************************************************************/

/*
 * Instruction description table
 */

typedef struct IDESC
{
	UINT	mask, match;
	INT		(* Interp)(UINT32);
	INT		(* Decode)(UINT32);

} IDESC;

#define INST_HANDLER(name)		I_##name, D_##name

static IDESC	idesc[] =
{
	#include "idesc.h"	// Uses INST_HANDLER!
};

/*
 * Interpreter and Decoder jump tables
 */

static INT		(* interp[65536])(UINT32);
static INT		(* decode[65536])(UINT32);

/*
 * Precomputed mask table for RLW* instructions
 */

UINT32			mask_table[1024];

/*
 * The Context
 */

PPC_CONTEXT		ppc;

/*
 * The Context Descriptor
 */

static REG_DESC	context_desc[NUM_DFLOW_BITS];

/*
 * IR Code Cache
 */

MEMPOOL			ir_cache;

/*
 * Temporary variables. (To be removed)
 */

static UINT32	(* Fetch)(void);

static const UINT64	timebase_max = ((UINT64)1) << 56;

static INT decode_cycles = 0; /* Cycles taken by the currently decoded BB. */

/*******************************************************************************
 Invalid Opcode Handlers

 These handlers are called whenever an opcode can't be interpreted or decoded.
*******************************************************************************/

static void InvalidInst (CHAR * prefix, UINT32 op)
{
    char mnem[16] = "";
	char oprs[48] = "";

//	DisassemblePowerPC(op, ppc.pc, (CHAR *)mnem, (CHAR *)oprs, FALSE);

	Print("%s unhandled opcode %08X (%u,%u). Disasm: %08X: %s\t%s",
		  prefix, op, _OP, _XO, PC, mnem, oprs);
}

static INT I_Invalid(UINT32 op)
{
	InvalidInst("interpeter", op);
	return 0;
}

static INT D_Invalid(UINT32 op)
{
	InvalidInst("decoder", op);
	return DRPPC_ERROR;
}

/*******************************************************************************
 Initialization Routines

 These routines perform all sorts of computations needed to build up the opcode
 dispatcher jump tables (for both the interpreter and the decoder), and other
 pre-computed tables.
*******************************************************************************/

#ifdef DRPPC_DEBUG

static INT VerifyInstTable(void)
{
	UINT8	temp[65536];
	UINT	op, i;
	UINT32	instr;
	INT		ret = DRPPC_OKAY;

	for (op = 0; op < 65536; op++)
	{
		instr =  (op << 16) & 0xFC000000;
		instr |= ((op & 0x3FF) << 1);

		temp[op] = 0;

		for (i = 0; i < (sizeof(idesc) / sizeof(idesc[0])); i++)
			if ((instr & idesc[i].mask) == idesc[i].match)
				temp[op] ++;

		if (temp[op] > 1)
		{
			Print("%08X [index %04X] has overlapping maps", instr, op);
			ret = DRPPC_ERROR;
		}
	}

	return ret;
}

#endif

static void SetupJumpTables (void)
{
	UINT32	instr;
	UINT	op, i;

	ASSERT(VerifyInstTable() == DRPPC_OKAY);

	for (op = 0; op < 65536; op++)
	{
		instr =  (op << 16) & 0xFC000000;
		instr |= ((op & 0x3FF) << 1);

		interp[op] = I_Invalid;
		decode[op] = D_Invalid;

		for (i = 0; i < (sizeof(idesc) / sizeof(idesc[0])); i++)
			if ((instr & idesc[i].mask) == idesc[i].match)
			{
				interp[op] = idesc[i].Interp;
				decode[op] = idesc[i].Decode;
				break;
			}
	}
}

static UINT32 CalcMask (UINT32 mb, UINT32 me)
{
	UINT32	m;

	m = ((UINT32)0xFFFFFFFF >> mb) ^ ((me >= 31) ? 0 : ((UINT32)0xFFFFFFFF >> (me + 1)));

	if (mb > me)
		return ~m;
	return m;
}

static void SetupMaskTable (void)
{
	UINT me, mb;

	for (mb = 0; mb < 32; mb++)
		for (me = 0; me < 32; me++)
			mask_table[(mb << 5) | me] = CalcMask(mb, me);
}

static INT SetupMMapRegions (DRPPC_CFG * cfg)
{
	/*
	 * Copy the configuration to the current context.
	 */

	ppc.mmap.fetch		= cfg->mmap_cfg.fetch;
	ppc.mmap.read8		= cfg->mmap_cfg.read8;
	ppc.mmap.read16		= cfg->mmap_cfg.read16;
	ppc.mmap.read32		= cfg->mmap_cfg.read32;
	ppc.mmap.write8		= cfg->mmap_cfg.write8;
	ppc.mmap.write16	= cfg->mmap_cfg.write16;
	ppc.mmap.write32	= cfg->mmap_cfg.write32;

	/*
	 * Now initialize the memory map module.
	 */

	return MMap_Setup(&cfg->mmap_cfg);
}

static INT SetupBBLookupHandlers (DRPPC_CFG * cfg)
{
	if (cfg->SetupBBLookup == NULL)
	{
		ppc.SetupBBLookup		= BBLookup_Setup;
		ppc.CleanBBLookup		= BBLookup_Clean;
		ppc.LookupBB			= BBLookup_Lookup;
		ppc.InvalidateBBLookup	= BBLookup_Invalidate;
		ppc.SetBBLookupInfo		= BBLookup_SetLookupInfo;
	}
	else
	{
		ppc.SetupBBLookup		= cfg->SetupBBLookup;
		ppc.CleanBBLookup		= cfg->CleanBBLookup;
		ppc.LookupBB			= cfg->LookupBB;
		ppc.InvalidateBBLookup	= cfg->InvalidateBBLookup;
		ppc.SetBBLookupInfo		= cfg->SetBBLookupInfo;
	}

	if (!ppc.SetupBBLookup || !ppc.CleanBBLookup ||
		!ppc.LookupBB || !ppc.InvalidateBBLookup ||
		!ppc.SetBBLookupInfo)
		return DRPPC_INVALID_CONFIG;

	return DRPPC_OKAY;
}

static void SetupContextDesc (void)
{
	UINT i;

	for (i = 0; i < NUM_DFLOW_BITS; i++)
		context_desc[i].ptr = NULL;

	for (i = 0; i < 32; i++)
		context_desc[DFLOW_BIT_R0+i].ptr	= (void *)&ppc.r[i];

	context_desc[DFLOW_BIT_CTR].ptr	= (void *)&CTR;
	context_desc[DFLOW_BIT_LR].ptr	= (void *)&LR;

	for (i = 0; i < 32; i++)
		context_desc[DFLOW_BIT_F0+i].ptr	= (void *)&ppc.f[i];

	for (i = 0; i < 8; i++)
	{
		context_desc[DFLOW_BIT_CR0_LT+i*4].ptr = (void *)&ppc.cr[i].lt;
		context_desc[DFLOW_BIT_CR0_GT+i*4].ptr = (void *)&ppc.cr[i].gt;
		context_desc[DFLOW_BIT_CR0_EQ+i*4].ptr = (void *)&ppc.cr[i].eq;
		context_desc[DFLOW_BIT_CR0_SO+i*4].ptr = (void *)&ppc.cr[i].so;
	}

	context_desc[DFLOW_BIT_XER_SO].ptr		= (void *)&ppc.xer.so;
	context_desc[DFLOW_BIT_XER_OV].ptr		= (void *)&ppc.xer.ov;
	context_desc[DFLOW_BIT_XER_CA].ptr		= (void *)&ppc.xer.ca;
	context_desc[DFLOW_BIT_XER_COUNT].ptr	= (void *)&ppc.xer.count;

	for (i = 0; i < 4; i++)
		context_desc[DFLOW_BIT_TEMP0+i].ptr	= (void *)&ppc.temp[i];
}

/*******************************************************************************
 PowerPC Support Emulation

 A bunch of routines that perform all sorts of things.
*******************************************************************************/

/*
 * Fetch*():
 *
 * These are the possible instruction fetch handlers. The correct handler is
 * installed in Fetch whenever the cur_fetch_region is changed (see
 * UpdateFetchPtr below.) They're the unified fetch handlers used by both Interp
 * and Decode.
 */

static UINT32 FetchHandler (void)
{
	UINT32 (* Handler)(UINT32);

	Handler = (UINT32 (*)(UINT32))ppc.cur_fetch_region->handler;

	return Handler(ppc.pc);
}

static UINT32 FetchDirect (void)
{
	return *ppc.pc_ptr++;
}

static UINT32 FetchDirectReverse (void)
{
	return Byteswap32(*ppc.pc_ptr++);
}

/*
 * UpdateFetchPtr():
 *
 * Update the current fetch pointer. First, check if the new PC still lies in
 * the old PC's fetch region. If it doesn't, retrieve the new fetch region.
 * Then, update the PC pointer within the current fetch region.
 */

INT UpdateFetchPtr (UINT32 pc)
{
	if (ppc.cur_fetch_region == NULL || // Remove me!
		ppc.cur_fetch_region->start > pc || ppc.cur_fetch_region->end <= pc)
	{
		if ((ppc.cur_fetch_region = MMap_FindFetchRegion(pc)) == NULL)
			return DRPPC_BAD_PC;

		/*
		 * Now install the correct fetch handler.
		 */

		if (ppc.cur_fetch_region->ptr == NULL)	// It's a handler-driven region
		{
			if (ppc.cur_fetch_region->handler == NULL)
				return DRPPC_ERROR;

			Fetch = FetchHandler;
		}
		else
		{
			if (ppc.cur_fetch_region->big_endian == FALSE)
			{
#if TARGET_ENDIANESS == LITTLE_ENDIAN
				Fetch = FetchDirect;			// LE region on LE target
#else // BIG_ENDIAN
				Fetch = FetchDirectReverse;		// LE region on BE target
#endif
			}
			else
			{
#if TARGET_ENDIANESS == LITTLE_ENDIAN
				Fetch = FetchDirectReverse;		// BE region on LE target
#else // BIG_ENDIAN
				Fetch = FetchDirect;			// BE region on BE target
#endif
			}
		}
	}

	/*
	 * Update pc_ptr even if the fetch region is handler-driven. Who cares?
	 */

	ppc.pc_ptr = (UINT32 *)((UINT32)ppc.cur_fetch_region->ptr + (pc - (UINT32)ppc.cur_fetch_region->start));

	return DRPPC_OKAY;
}

/*
 * INT UpdatePC(void);
 *
 * This routine must be called whenever a branch is executed, so that the core
 * can perform the operations needed to switch between the interpreter and the
 * recompiler.
 */

INT UpdatePC (void)
{
	BOOL		hit_untraslated_bb;
	DRPPC_BB	* bb;

	/*
	 * Update the current fetch pointer.
	 */

	DRPPC_TRY( UpdateFetchPtr(ppc.pc) );

	if (ppc.interpret_only)
		return DRPPC_OKAY;

	/*
	 * Run the translator till we hit a BB that cannot be translated.
	 */

	hit_untraslated_bb = FALSE;

	do
	{
		/*
		 * Retrieve BB info.
		 */

		UINT32 pc = ppc.pc;
		INT	ret;

		if ((bb = ppc.LookupBB(ppc.pc, &ret)) == NULL)
			return ret;

		if (bb->count < ppc.hot_threshold)
		{
			/*
			 * If the BB's execution count hasn't reached the threshold yet,
			 * bump it; then, if it has finally reached the threshold, translate
			 * the BB at once.
			 */

			if (++bb->count == ppc.hot_threshold)
			{
				DRPPC_TRY( Decode(ppc.pc, bb) );
			}
			else
				hit_untraslated_bb = TRUE;
		}

		if (bb->ptr != NULL)
		{
			/*
			 * The BB has already reached the threshold, so it's already
			 * translated. In this case, just execute it.
			 */

			DRPPC_TRY( Target_RunBB(bb) );

			CheckIRQs();
		}
		else
		{
			// Temporary check.
			ASSERT(bb->count < ppc.hot_threshold);
		}

	} while (!hit_untraslated_bb && ppc.remaining >= 0);

	/*
	 * Keep the fetch pointer in sync.
	 */

	DRPPC_TRY( UpdateFetchPtr(ppc.pc) );

	return DRPPC_OKAY;
}

/*
 * Timebase Register Access
 *
 * UINT32 ReadTimebaseHi, ReadTimebaseLo (void);
 * void WriteTimebaseHi, WriteTimebaseLo (UINT32);
 *
 * These four routines are used to access the timebase register. Since it's
 * handled differently from the other registers, these abstract access routines
 * are used instead of direct access. Their meaning is elementary.
 *
 * NOTE: the cause of all the shift and masking here is that the timebase is
 * stored in 56.2 fixed point format (see UpdateTimers for explanations.)
 */

UINT32 ReadTimebaseLo (void)
{
	return (UINT32)(ppc.timebase >> 2);
}

UINT32 ReadTimebaseHi (void)
{
	return (UINT32)(ppc.timebase >> 34) & 0x00FFFFFF; // sign extend?
}

void WriteTimebaseLo (UINT32 data)
{
	ppc.timebase &= ~((UINT64)0xFFFFFFFFU << 2);
	ppc.timebase |= ((UINT64)data) << 2;
}

void WriteTimebaseHi (UINT32 data)
{
	ppc.timebase &= ~((UINT64)0xFFFFFFFFU << 34);
	ppc.timebase |= ((UINT64)data) << 34;
}

/*
 * void UpdateTimers (INT cycles);
 *
 * Function to step the timebase register by the specified amount of cycles.
 * It handles timebase overflow, too, althought overflow is very unlikely to
 * happen (every 2^58 cycles!), but the emulated code may write to the timebase
 * a value near to 2^58 to cause an overflow on purpose.
 *
 * It also updates the decrementer register on 603 and Gekko models, and checks
 * for underflow. This computation relies on the fractionary part of the
 * timebase.
 *
 * SOME EXPLANATIONS: the timebase register is a 56-bit hardware register that
 * is incremented by one every four cycles. To obtain full precision, we use the
 * two lower bits of ppc.timebase as a fractional part, and simply add the raw
 * cycle number to ppc.timebase whenever an instruction has been executed. On
 * overflow, we clear everything except the fractionary bits.
 */

void UpdateTimers (INT cycles)
{
	cycles *= 0x100000;

	ppc.remaining -= cycles;

	ppc.timebase += cycles;

	if (ppc.timebase >= ((UINT64)1 << (56 + 2)))	// Overflow
		ppc.timebase = (((ppc.timebase >> 2) - timebase_max) << 2) +	// Integer part
					   (ppc.timebase & 3);								// Fraction

#if PPC_MODEL == PPC_MODEL_6XX || PPC_MODEL == PPC_MODEL_GEKKO
	{
		UINT32 old_dec = DEC;

		DEC -= (cycles / 4) + ((ppc.timebase & 3) != 0);

		if (old_dec > DEC)					// Underflow
			ppc.dec_expired = TRUE;
	}
#endif
}

/*******************************************************************************
 Interpreter

 Right, here's the interpreter.
*******************************************************************************/

/*
 * INT Interpret (INT count, INT * ret);
 *
 * Run the interpreter for count cycles. It returns the remaining cycles at
 * end of execution. Actually, it will try to run in native mode (i.e. through
 * the recompiler) whenever it can. So, the name 'Interpret' isn't correct.
 *
 * If an error happens, err is written with the corresponding error number.
 */

static INT Interpret (INT count, INT * ret)
{
	UINT32	op, cycles, iaddr;

	ppc.requested = count;
	ppc.remaining = count;

	DRPPC_TRY( UpdatePC() );

	while (ppc.remaining > 0)
	{
		iaddr = ppc.pc;

		op = Fetch();

		ppc.pc += 4;

		cycles = interp[(_OP << 10) | _XO](op);

		UpdateTimers(cycles);

		CheckIRQs();

		if (iaddr == ppc.breakpoint)
			break;
	}

	*ret = ppc.remaining - ppc.requested;

	return DRPPC_OKAY;
}

/*******************************************************************************
 Decoder

 Here's the decoder's main routine.
*******************************************************************************/

extern UINT32 __temp;

static void expand_string (char * string, int max)
{
	int i;

	for (i = strlen(string); i < (max-1); i++)
		string[i] = ' ';
	string[max-1] = 0;
}

static void PrintDisasm (UINT32 op, UINT32 pc)
{
	char	mnem[12], oprs[32];

	DisassemblePowerPC(op, pc, mnem, oprs, FALSE);

	expand_string(mnem, 12);

	Print("%08X: %s%s", pc, mnem, oprs);
}

INT GetDecodeCycles (void)
{
	return decode_cycles;
}

void ResetDecodeCycles (void)
{
	decode_cycles = 0;
}

void AddDecodeCycles (INT cycles)
{
	decode_cycles += cycles;
}

/*
 * INT Decode (UINT32 pc, DRPPC_BB * bb):
 *
 * The purpose of this function is to, well, decode PowerPC instructions into a
 * more malleable intermediate representation (IR.) The translation is then
 * passed on the the middle and and finally to the generator. Everything starts
 * in this little routine. The code is fetched starting at pc, tilla a branch
 * (or any other terminator) is hit; the result is placed in bb.
 */

static INT Decode (UINT32 pc, DRPPC_BB * bb)
{
	UINT32	op;
	INT		ret = 0;
	IR		* ir;
	UINT32	pc_temp = ppc.pc;

	ResetDecodeCycles();

	/*
	 * Let the middle-end know that a new BB is being translated.
	 */

	DRPPC_TRY( IR_BeginBB() );

	/*
	 * Now, fetch instructions sequentially and decode, till a branch is found
	 * or an  error is generated.
	 */

#if PRINT_POWERPC_DISASM
	Print("\n ========== PowerPC BB ========== ");
#endif

	do
	{
		ASSERT(Fetch != NULL);

		op = Fetch();

#if PRINT_POWERPC_DISASM
		PrintDisasm(op, ppc.pc);
#endif

		ppc.pc += 4;	// FIXME! It shouldn't use ppc.pc!

		ret = decode[(_OP << 10) | _XO](op);

		AddDecodeCycles(1);

	} while (ret == DRPPC_OKAY);

#if PRINT_POWERPC_DISASM
	Print(" ================================ ");
#endif

	/*
	 * Check if we hit a branch (terminator) or caught an error.
	 */

	if (ret != DRPPC_TERMINATOR)
	{
		ASSERT(D_Invalid(op)); // Just to print out the disassembly.
		return ret;
	}

	// __temp = ppc.pc-4 (latest decoded instruction)
	IR_EncodeLoadi(ID_TEMP(0), ppc.pc-4);
	IR_EncodeStorePtr32(ID_TEMP(0), (UINT32)&__temp);

	ppc.pc = pc_temp;

	/*
	 * Add a timer synchronization instruction.
	 */

	IR_EncodeSync(GetDecodeCycles());

	/*
	 * Let know the middle-end that the BB translation has ended and retrieve
	 * various informations about it (such as the address of the *final*
	 * intermediate BB representation).
	 */

	DRPPC_TRY( IR_EndBB(&ir) );

	/*
	 * Finally generate the native BB translation. Don't execute it just yet!
	 */

	DRPPC_TRY( Target_GenerateBB(bb, ir->next) );

	return DRPPC_OKAY;
}

/*******************************************************************************
 Utility Functions

 Used by the I_* and D_* routines. They're placed here instead of being
 statically declared in internal.h, because VC fries the other way. And i
 don't want to object Mr. Gates decisions.
*******************************************************************************/

UINT GET_CR_LT (UINT n){ return ppc.cr[n].lt; }
UINT GET_CR_GT (UINT n){ return ppc.cr[n].gt; }
UINT GET_CR_EQ (UINT n){ return ppc.cr[n].eq; }
UINT GET_CR_SO (UINT n){ return ppc.cr[n].so; }

void SET_CR_LT (UINT n){ ppc.cr[n].lt = 1; }
void SET_CR_GT (UINT n){ ppc.cr[n].gt = 1; }
void SET_CR_EQ (UINT n){ ppc.cr[n].eq = 1; }
void SET_CR_SO (UINT n){ ppc.cr[n].so = 1; }

void CLEAR_CR_LT (UINT n){ ppc.cr[n].lt = 0; }
void CLEAR_CR_GT (UINT n){ ppc.cr[n].gt = 0; }
void CLEAR_CR_EQ (UINT n){ ppc.cr[n].eq = 0; }
void CLEAR_CR_SO (UINT n){ ppc.cr[n].so = 0; }

UINT GET_CR (UINT n)
{
	return	(((ppc.cr[n].lt & 1) << 3) |
			 ((ppc.cr[n].gt & 1) << 2) |
			 ((ppc.cr[n].eq & 1) << 1) |
			 ((ppc.cr[n].so & 1) << 0));
}

void SET_CR (UINT n, UINT32 v)
{
	ppc.cr[n].lt = (v >> 3) & 1;
	ppc.cr[n].gt = (v >> 2) & 1;
	ppc.cr[n].eq = (v >> 1) & 1;
	ppc.cr[n].so = (v >> 0) & 1;
}

void RESET_CR (UINT n)
{
	CLEAR_CR_LT(n);
	CLEAR_CR_GT(n);
	CLEAR_CR_EQ(n);
	CLEAR_CR_SO(n);
}

UINT GET_XER_SO (void){ return ppc.xer.so; }
UINT GET_XER_OV (void){ return ppc.xer.ov; }
UINT GET_XER_CA (void){ return ppc.xer.ca; }

void SET_XER_SO (void){ ppc.xer.so = 1; }
void SET_XER_OV (void){ ppc.xer.ov = 1; }
void SET_XER_CA (void){ ppc.xer.ca = 1; }

void CLEAR_XER_SO (void){ ppc.xer.so = 0; }
void CLEAR_XER_OV (void){ ppc.xer.ov = 0; }
void CLEAR_XER_CA (void){ ppc.xer.ca = 0; }

UINT GET_XER (void)
{
	return	((ppc.xer.so & 1) << 31) |
			((ppc.xer.ov & 1) << 30) |
			((ppc.xer.ca & 1) << 29) |
			 (ppc.xer.count & 0x7F);
}

void SET_XER (UINT32 v)
{
	ppc.xer.so = (v >> 31) & 1;
	ppc.xer.ov = (v >> 30) & 1;
	ppc.xer.ca = (v >> 29) & 1;
	ppc.xer.count = v & 0x7F;
}

void RESET_XER (void)
{
	CLEAR_XER_SO();
	CLEAR_XER_OV();
	CLEAR_XER_CA();
}

void CMPS (UINT t, INT32 a, INT32 b)
{
	if(a < b)
	{
		SET_CR_LT(t);
		CLEAR_CR_GT(t);
		CLEAR_CR_EQ(t);
	}
	else if(a > b)
	{
		CLEAR_CR_LT(t);
		SET_CR_GT(t);
		CLEAR_CR_EQ(t);
	}
	else
	{
		CLEAR_CR_LT(t);
		CLEAR_CR_GT(t);
		SET_CR_EQ(t);
	}

	if (GET_XER_SO())
		SET_CR_SO(t);
	else
		CLEAR_CR_SO(t);
}

void CMPU (UINT t, UINT32 a, UINT32 b)
{
	if(a < b)
	{
		SET_CR_LT(t);
		CLEAR_CR_GT(t);
		CLEAR_CR_EQ(t);
	}
	else if(a > b)
	{
		CLEAR_CR_LT(t);
		SET_CR_GT(t);
		CLEAR_CR_EQ(t);
	}
	else
	{
		CLEAR_CR_LT(t);
		CLEAR_CR_GT(t);
		SET_CR_EQ(t);
	}

	if (GET_XER_SO())
		SET_CR_SO(t);
	else
		CLEAR_CR_SO(t);
}

BOOL is_nan_f32(FLOAT32 x)
{
	return	((*(UINT32 *)&x & SINGLE_EXP) == SINGLE_EXP) && 
			((*(UINT32 *)&x & SINGLE_FRAC) != 0);
}

BOOL is_qnan_f32(FLOAT32 x)
{
	return	is_nan_f32(x) && ((*(UINT32 *)&x & SINGLE_QUIET) != 0);
}

BOOL is_snan_f32(FLOAT32 x)
{
	return	is_nan_f32(x) && ((*(UINT32 *)&x & SINGLE_QUIET) == 0);
}

BOOL is_infinity_f32(FLOAT32 x)
{
	return	((*(UINT32 *)&x & SINGLE_EXP) == SINGLE_EXP) &&
			((*(UINT32 *)&x & SINGLE_FRAC) == 0);
}

BOOL is_denormal_f32(FLOAT32 x)
{
	return	((*(UINT32 *)&x & SINGLE_EXP) == 0) &&
			((*(UINT32 *)&x & SINGLE_FRAC) != 0);
}

BOOL is_nan_f64(FLOAT64 x)
{
	return	((*(UINT64 *)&x & DOUBLE_EXP) == DOUBLE_EXP) &&
			((*(UINT64 *)&x & DOUBLE_FRAC) != 0);
}

BOOL is_qnan_f64(FLOAT64 x)
{
	return	is_nan_f64(x) && ((*(UINT64 *)&x & DOUBLE_QUIET) != 0);
}

BOOL is_snan_f64(FLOAT64 x)
{
	return	is_nan_f64(x) && ((*(UINT64 *)&x & DOUBLE_QUIET) == 0);
}

BOOL is_infinity_f64(FLOAT64 x)
{
    return	((*(UINT64 *)&(x) & DOUBLE_EXP) == DOUBLE_EXP) &&
			((*(UINT64 *)&(x) & DOUBLE_FRAC) == 0);
}

BOOL is_normalized_f64(FLOAT64 x)
{
    UINT64 exp;

    exp = ((*(UINT64 *) &(x)) & DOUBLE_EXP) >> 52;

    return (exp >= 1) && (exp <= 2046);
}

BOOL is_denormalized_f64(FLOAT64 x)
{
	return	((*(UINT64 *)&x & DOUBLE_EXP) == 0) &&
			((*(UINT64 *)&x & DOUBLE_FRAC) != 0);
}

BOOL sign_f64(FLOAT64 x)
{
    return ((*(UINT64 *)&(x) & DOUBLE_SIGN) != 0);
}

INT32 round_f64_to_s32(FLOAT64 v)
{
	if(v >= 0)
		return((INT32)(v + 0.5));
	else
		return(-(INT32)(-v + 0.5));
}

INT32 trunc_f64_to_s32(FLOAT64 v)
{
	if(v >= 0)
		return((INT32)v);
	else
		return(-((INT32)(-v)));
}

INT32 ceil_f64_to_s32(FLOAT64 v)
{
	return((INT32)ceil(v));
}

INT32 floor_f64_to_s32(FLOAT64 v)
{
	return((INT32)floor(v));
}

void set_fprf(FLOAT64 f)
{
    UINT32 fprf;

    // see page 3-30, 3-31

    if (is_qnan_f64(f))
        fprf = 0x11;
    else if (is_infinity_f64(f))
    {
        if (sign_f64(f))    // -INF
            fprf = 0x09;
        else                // +INF
            fprf = 0x05;
    }
    else if (is_normalized_f64(f))
    {
        if (sign_f64(f))    // -Normalized
            fprf = 0x08;
        else                // +Normalized
            fprf = 0x04;
    }
    else if (is_denormalized_f64(f))
    {
        if (sign_f64(f))    // -Denormalized
            fprf = 0x18;
        else                // +Denormalized
            fprf = 0x14;
    }
    else    // Zero
    {
        if (sign_f64(f))    // -Zero
            fprf = 0x12;
        else                // +Zero
            fprf = 0x02;
    }

    FPSCR &= ~0x0001F000;
    FPSCR |= (fprf << 12);
}

/*******************************************************************************
 Interface

 Interface to the rest of drppc and to the external emulator.
*******************************************************************************/

/*
 * INT PowerPC_Init (DRPPC_CFG * cfg, UINT32 pvr, UINT (* IRQCallback)(void));
 *
 * This routine initializes all of the structures needed to run drppc. It's
 * placed here, in the front-end, since the main application (the emulator) only
 * communicates with this portion of drtest.
 *
 * VERY IMPORTANT NOTE: this routine must be called before any other drtest
 * service is used! If the main application forgets to call it, behaviour will
 * be undefined.
 *
 * Parameters:
 *		cfg         = The configuration structure. The user must fill in the 
 *                    various fields before sending it in to this function.
 *		pvr         = The value you wish to be used as Processor Version 
 *					  Register. It's a PowerPC-model specific value. It does
 *					  vary from submodel to submodel, too.
 *		IRQCallback = This callback function is called whenever an external IRQ
 *                    is accepted. It must return the new IRQ line state
 *                    (usually you'll want to return 0).
 *
 * Returns:
 *		DRPPC_OKAY if no error occurred.
 *		DRPPC_INVALID_CONFIG if some cfg field holds an invalid value.
 *		DRPPC_OUT_OF_MEMORY if memory allocation failed.
 */

INT PowerPC_Init (DRPPC_CFG * cfg)
{
	DRPPC_TRY( SetupBasicServices(cfg) );

	SetupJumpTables();

	SetupMaskTable();

	SetupContextDesc();

	return DRPPC_OKAY;
}

/*
 * INT PowerPC_SetupContext ():
 *
 * Sets up the current context with the given informations.
 */

INT PowerPC_SetupContext
(
	DRPPC_CFG	* cfg,
	UINT32		pvr,
	UINT		(* IRQCallback)(void)
)
{
	ppc.interpret_only = cfg->interpret_only;

	DRPPC_TRY( SetupMMapRegions(cfg) );

	DRPPC_TRY( SetupBBLookupHandlers(cfg) );

	DRPPC_TRY( ppc.SetupBBLookup(cfg, &ppc.bblookup_info) );

	if (cfg->native_cache_size > 0)
	{
		DRPPC_TRY( MemPool_Alloc(	&ppc.code_cache,
									cfg->native_cache_size,
									cfg->native_cache_guard_size) );
	}

	if (cfg->intermediate_cache_size > 0)
	{
		DRPPC_TRY( MemPool_Alloc(	&ir_cache,
									cfg->intermediate_cache_size,
									cfg->intermediate_cache_guard_size) );
	}

	ppc.hot_threshold = cfg->hot_threshold;

	if (NULL == (ppc.IRQCallback = IRQCallback))
		return DRPPC_INVALID_CONFIG;

	SPR(SPR_PVR) = pvr;

	DRPPC_TRY( InitModel(cfg, pvr) );

	/*
	 * Initialize the middle end.
	 */

	DRPPC_TRY( IR_Init(&ir_cache) );

	/*
	 * Initialize the back end.
	 */

	DRPPC_TRY( Target_Init(	&ppc.code_cache,	// Native Code Cache
							&ppc.pc,			// Program Counter
							&ppc,				// Context Base
							context_desc,		// Context Descriptor
							&UpdateTimers,		// Timer Update Routine
							&UpdateFetchPtr) );	// Fetch Pointer Update Routine

	return DRPPC_OKAY;
}

/*
 * INT PowerPC_Reset (void);
 *
 * It resets the current context and all the other emulation components. It 
 * does the same (somewhat) as if the emulated CPU was reset.
 * 
 * Returns:
 *  	DRPPC_OKAY if no error occurred.
 *		DRPPC_ERROR if MMap_Setup failed (because of invalid fetch/read/write 
 *		ptrs).
 */

INT PowerPC_Reset (void)
{
	UINT	i, pvr;

	for (i = 0; i < 32; i++)
		R(i) = 0;

	pvr = SPR(SPR_PVR);
	for (i = 0; i < 1024; i++)
		SPR(i) = 0;
	SPR(SPR_PVR) = pvr;

	for (i = 0; i < 8; i++)
		RESET_CR(i);

	ppc.irq_state = 0;
	ppc.timebase = 0;

	DRPPC_TRY( MMap_Setup(&ppc.mmap) );

	DRPPC_TRY( ResetModel() );

	UpdateFetchPtr(ppc.pc);

	ppc.InvalidateBBLookup();

	return DRPPC_OKAY;
}

/*
 * void PowerPC_Shutdown (void);
 *
 * Call this routine when you're done with drppc. It will simply free all of the
 * allocated structures. You can feed this one to atexit, if you wish. 
 */

void PowerPC_Shutdown (void)
{
	MemPool_Free(&ppc.code_cache);
	MemPool_Free(&ir_cache);

	ShutdownModel();

	ppc.SetBBLookupInfo(ppc.bblookup_info);
	ppc.CleanBBLookup();

	Target_Shutdown();
}

/*
 * INT PowerPC_Run (INT cycles, INT * err);
 *
 * This routine runs the emulated context for the specified amount of cycles.
 *
 * Parameters:
 *		cycles = The amount of cycles to run for.
 *		err    = A non-NULL pointer to an integer to hold the return error 
 *               code.
 *
 * Returns:
 *		The number of effective cycles the CPU has been emulated for, minus the
 *		specified amoung of cycles. The emulator isn't cycle-perfect, so it 
 *		will probably run a few cycles *more* than specified; so, the returned
 *		value will very likely be positive.
 *
 * Notes:
 *		err will eventually assume one of the following error codes:
 *
 *		DRPPC_OKAY when no error happened -- or we didn't notice it ;)
 *		DRPPC_BAD_PC when the code branches to an invalid location.
 *		DRPPC_ERROR when a generic error happens (self-generated, miraculous
 *		errors.	You won't see many of these. ;) In case, take a look at 
 *		UpdateFetchPtr.)
 */

INT PowerPC_Run (INT cycles, INT * err)
{
	INT ret;

	ppc.SetBBLookupInfo(ppc.bblookup_info);

	*err = Interpret(cycles, &ret);

	return ret;
}

/*
 * void PowerPC_AddCycles (INT cycles);
 *
 * This routine adds the specified number of cycles to the cycles remaining
 * to run.
 *
 * Parameters:
 *		cycles = The amount of cycles to add. 
 */

void PowerPC_AddCycles (INT cycles)
{
	//ppc.requested += cycles;
	ppc.remaining += cycles;
}

/*
 * void PowerPC_ResetCycles (void);
 *
 * It resets the number of remaining cycles: as an effect, PowerPC_Run will
 * (almost) immediately return. 
 */

void PowerPC_ResetCycles (void)
{
	ppc.remaining = 0;
}

/*
 * INT PowerPC_GetCyclesLeft (void);
 *
 * It returns the number of cycles still left. 
 *
 * Returns:
 *		The number of cycles left.
 */

INT PowerPC_GetCyclesLeft (void)
{
	return ppc.remaining;
}

/*
 * void PowerPC_SetIRQLine (UINT state);
 *
 * It sets or clears the external interrupt request line (vector 0x500). It is
 * mainly used to trigger hardware interrupts; rarely to clear the line.
 *
 * Parameters:
 *		state = The new IRQ line state. 0 means off, 1 means on. 
 */

void PowerPC_SetIRQLine (UINT state)
{
	ppc.irq_state = state;
}

/*
 * INT PowerPC_Get/Set_Context (PPC_CONTEXT * ctx);
 *
 * These routines map in and out the current CPU context. They are provided for
 * emulation of multiprocessor systems. (The limitation of the current source is
 * that only processors of the *same* PowerPC model can be emulated together.)
 *
 * Parameters:
 *		ctx = The context to write from (on Set) or to read to (on Get). It
 *			  shall be a non-NULL pointer to a valid PPC_CONTEXT struct.
 *
 * Notes:
 *
 *	All of the structure fields are considered valid, on Get. That is, if you
 *	implement some kind of save state mechanism externally to drppc, you should
 *	provide valid function handlers and region descriptors in ctx, not
 *	merely the ones you saved in the file, otherwise you'll end up crashing the
 *  emulator (as the old pointers won't point to valid memory anymore.)
 */

void PowerPC_GetContext (PPC_CONTEXT * ctx)
{
	if (ctx != NULL)		
		memcpy((void *)ctx, (void *)&ppc, sizeof(PPC_CONTEXT));
}

void PowerPC_SetContext (PPC_CONTEXT * ctx)
{
	if (ctx != NULL)
	{		
		memcpy((void *)&ppc, (void *)ctx, sizeof(PPC_CONTEXT));
		MMap_Setup (&ctx->mmap);
	}	
}

void PowerPC_SetBreakpoint (UINT32 bp)
{
	ppc.breakpoint = bp;
}
