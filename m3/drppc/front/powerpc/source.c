/*
 * front/powerpc/source.c
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
#include "mmap.h"
#include "bb_lookup.h"
#include "ir.h"
#include "target.h"

#include "source.h"
#include "internal.h"
#include "interp.h"		// I_* instruction handlers
#include "decode.h"		// D_* instruction handlers
#include "disasm.h"

/*******************************************************************************
 Variables

 There are both global and local variables here. The layout in memory is
 important for cache issues.
*******************************************************************************/

/*
 * NOTE: INST_HANDLER is used in idesc.h
 */

static const struct
{
	UINT	mask, match;
	INT		(* Interp)(UINT32);
	INT		(* Decode)(UINT32);
}
	idesc[] =							// Instruction description table
{

#define INST_HANDLER(name)	I_##name, D_##name

#include "idesc.h"

};

static INT		(* interp[65536])(UINT32);	// Interpreter dispatch table
static INT		(* decode[65536])(UINT32);	// Decoder dispatch table

#define DISPATCH_OP(table, op)	table[(_OP << 10) | _XO](op)

UINT32			mask_table[1024];			// Precomputed mask table (see RLW*)

static UINT32	(* Fetch)(void);			// The unified fetch handler

PPC_CONTEXT		ppc;						// The PowerPC context

INT				decoded_bb_timing;			// Used by the decoder

/*******************************************************************************
 Invalid Opcode Handlers

 These handlers are called whenever an opcode can't be interpreted or decoded.
*******************************************************************************/

static void InvalidInst (CHAR * prefix, UINT32 op)
{
    char mnem[16] = "";
	char oprs[48] = "";

    DisassemblePowerPC(op, ppc.pc, (CHAR *)mnem, (CHAR *)oprs, FALSE);

	Print("%s unhandled opcode %08X (%u,%u). Disassembly: %08X: %s\t%s\n",
		  prefix, op, _OP, _XO, PC, mnem, oprs);
}

static INT I_Invalid(UINT32 op)
{
	InvalidInst("interpeter", op);

	exit(1); // FIXME!

	return 0;
}

static INT D_Invalid(UINT32 op)
{
	InvalidInst("decoder", op);
	return DRPPC_ERROR;
}

/*******************************************************************************
 Initialization Routines

 These routines perform all the sort of computations needed to build up the
 opcode dispatcher jump tables (for both the interpreter and the decoder), and
 other pre-computed tables.
*******************************************************************************/

#define IDESC_NUM		(sizeof(idesc) / sizeof(idesc[0]))

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

		for (i = 0; i < IDESC_NUM; i++)
			if ((instr & idesc[i].mask) == idesc[i].match)
				temp[op] ++;

		if (temp[op] > 1)
		{
			printf("%08X [%04X] has overlapping maps\n", instr, op);
			ret = DRPPC_ERROR;
		}
	}

	return ret;
}

static void SetupJumpTables (void)
{
	UINT32	instr;
	UINT	op, i;

	ASSERT(VerifyInstTable() == DRPPC_OKAY); // It's compiled only if DRPPC_DEBUG is defined.

	for (op = 0; op < 65536; op++)
	{
		instr =  (op << 16) & 0xFC000000;
		instr |= ((op & 0x3FF) << 1);

		interp[op] = I_Invalid;
		decode[op] = D_Invalid;

		for (i = 0; i < IDESC_NUM; i++)
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
	}
	else
	{
		ppc.SetupBBLookup		= cfg->SetupBBLookup;
		ppc.CleanBBLookup		= cfg->CleanBBLookup;
		ppc.LookupBB			= cfg->LookupBB;
		ppc.InvalidateBBLookup	= cfg->InvalidateBBLookup;
	}

	if (!ppc.SetupBBLookup || !ppc.CleanBBLookup ||
		!ppc.LookupBB || !ppc.InvalidateBBLookup)
		return DRPPC_INVALID_CONFIG;

	return DRPPC_OKAY;
}

/*******************************************************************************
 PowerPC Support Emulation

 A bunch of routines that perform all sorts of things.
*******************************************************************************/

static INT Decode (UINT32, DRPPC_BB *);

/*
 * UINT32 Fetch* (void);
 *
 * These are the possible instruction fetch handlers. The correct handler is
 * installed in Fetch whenever the cur_fetch_region is changed (see
 * UpdateFetchRegion below.) They're the unified fetch handlers used by both
 * Interp and Decode.
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
	return BSWAP32(*ppc.pc_ptr++); // BSWAP32 is a function!
}

/*
 * INT UpdateFetchPtr (void);
 *
 * Update the current fetch pointer. First, check if the new PC still lies in
 * the old PC's fetch region. If it doesn't, retrieve the new fetch region.
 * Then, update the PC pointer within the current fetch region.
 */

static INT UpdateFetchPtr (void)
{
	UINT32	pc = ppc.pc;

	if (ppc.cur_fetch_region == NULL ||
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
 *
 * NOTE: to achieve direct translation, and bypass the profiling stage, you must
 * set hot_threshold to 1, *not* to 0! It is somewhat counterintuitive, so it'll
 * probably be changed in the future.
 */

INT UpdatePC (void)
{
	DRPPC_BB	* bb;
	BOOL		hit_untraslated_bb = FALSE;
	INT			ret;

	/*
	 * Update the current fetch pointer.
	 */

	DRPPC_TRY( UpdateFetchPtr() );

	/*
	 * Run the translator till we hit a BB that cannot be translated.
	 */

	do
	{
		/*
		 * Retrieve BB info.
		 */

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
				if ((ret = Decode(ppc.pc, bb)) != DRPPC_OKAY)
					return ret;
				if ((ret = Target_RunBB(bb)) != DRPPC_OKAY)
				{
					if (ret == DRPPC_TIMESLICE_ENDED)
						hit_untraslated_bb = TRUE; // Actually we've exausthed the cycle counter
					else
						return ret;
				}
			}
			else
				hit_untraslated_bb = TRUE;
		}
		else
		{
			/*
			 * The BB has already reached the threshold, so it's already
			 * translated. In this case, just execute it.
			 */

			if ((ret = Target_RunBB(bb)) != DRPPC_OKAY)
			{
				if (ret == DRPPC_TIMESLICE_ENDED)
					hit_untraslated_bb = TRUE;	// Actually we've exausthed the cycle counter
				else
					return ret;
			}
		}

	} while (!hit_untraslated_bb);

	/*
	 * Keep the fetch pointer in sync.
	 */

	DRPPC_TRY( UpdateFetchPtr() );

	return DRPPC_OKAY;
}

/*
 * Timebase Register Access
 *
 * UINT32 ReadTimebaseHi, ReadTimebaseLo (void);
 * void WriteTimebaseHi, WriteTimebaseLo (UINT32);
 *
 * These four routines are used to access the timebase register. Since it's
 * handled differenctly from other registers, these abstract access routines
 * are used instead of direct access. Their meaning is elementary.
 *
 * NOTE: the cause of all the shift and masking here is that the timebase is
 * stored in 56.2 fixed point format (see UpdateTimebase for explanations.)
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
	ppc.timebase &= ~(0xFFFFFFFFULL << 2);
	ppc.timebase |= ((UINT64)data) << 2;
}

void WriteTimebaseHi (UINT32 data)
{
	ppc.timebase &= ~(0xFFFFFFFFULL << 34);
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
	ppc.timebase += cycles;

	if (ppc.timebase >= (1ULL << (56 + 2)))	// Overflow
		ppc.timebase = (((ppc.timebase >> 2) - (1ULL << 56)) << 2) +	// Integer part
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
 * INT Interpret (INT count, INT * err);
 *
 * Run the interpreter for count cycles. It returns the remaining cycles at
 * end of execution. Actually, it will try to run in native mode (i.e. through
 * the recompiler) whenever it can. So, the name 'Interpret' isn't correct.
 *
 * If an error happens, err is written with the corresponding error number.
 */

static INT Interpret (INT count, INT * err)
{
	UINT32	op, cycles;
	UINT	ret;

	ppc.requested = count;
	ppc.remaining = count;

	if (DRPPC_OKAY != (ret = UpdatePC()))
	{
		*err = ret;
		return 0;
	}

	while (ppc.remaining > 0)
	{
		ASSERT(Fetch != NULL);

		op = Fetch();

		ppc.pc += 4;

		cycles = DISPATCH_OP(interp, op);

		ppc.remaining -= cycles;

		UpdateTimers(cycles);

		CheckIRQs();
	}

	return ppc.remaining - ppc.requested;
}

/*******************************************************************************
 Decoder

 Here's the decoder's main routines.
*******************************************************************************/

/*
 * INT Decode (UINT32 pc, DRPPC_BB * bb);
 *
 * The purpose of this function is to, well, decode PowerPC instructions into a
 * more malleable intermediate representation (IR.) The translation is then
 * passed on the the middle and and finally to the generator. Everything starts
 * in this little routine. The result is placed in bb.
 */

static INT Decode (UINT32 pc, DRPPC_BB * bb)
{
	UINT32	op;
	INT		ret = 0;
	IR		* ir;

	decoded_bb_timing = 0;

	/*
	 * Let know the middle-end that a new BB is being translated.
	 */

	DRPPC_TRY( IR_BeginBB() );

	/*
	 * Now, fetch instructions sequentially and decode, till a branch is found,
	 * or an  error is generated.
	 */

	do
	{
		ASSERT(Fetch != NULL);

		op = Fetch();

		ppc.pc += 4;

		ret = DISPATCH_OP(decode, op);

	} while (ret == DRPPC_OKAY);

	/*
	 * Check if we hit a branch (terminator) or caught an error.
	 */

	if (ret != DRPPC_TERMINATOR)
	{
		ASSERT(D_Invalid(op)); // Just to print out the disassembly.
		return ret;
	}

	/*
	 * Let know the middle-end that the BB translation has ended and retrieve
	 * various informations about it (such as the address of the *final*
	 * intermediate BB representation).
	 */

	DRPPC_TRY( IR_EndBB(&ir) );

	/*
	 * Finally generate the native BB translation. Don't execute it just yet!
	 */

	DRPPC_TRY( Target_GenerateBB(bb, ir, decoded_bb_timing) );

	return DRPPC_OKAY;
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
 *
 *	- DRPPC_CFG	* cfg
 *
 *	  The configuration structure. The user must fill in the various fields
 *	  before sending it in to this function.
 *
 *	- UINT32 pvr
 *
 *	  The value you wish to be used as Processor Version Register. It's a
 *	  PowerPC-model specific value. It does vary from submodel to submodel, too.
 *
 *	- UINT (* IRQCallback)(void)
 *
 *	  This callback function is called whenever an external IRQ is accepted.
 *	  It must return the new IRQ line state (usually you'll want to return 0).
 *
 * Returns:
 *
 *	DRPPC_OKAY if no error occurred.
 *	DRPPC_INVALID_CONFIG if some cfg field holds an invalid value.
 *	DRPPC_OUT_OF_MEMORY if memory allocation failed.
 */

INT PowerPC_Init (DRPPC_CFG * cfg, UINT32 pvr, UINT (* IRQCallback)(void))
{
	DRPPC_TRY( SetupBasicServices(cfg) );

	DRPPC_TRY( SetupMMapRegions(cfg) );

	DRPPC_TRY( SetupBBLookupHandlers(cfg) );

	DRPPC_TRY( ppc.SetupBBLookup(cfg) )

	DRPPC_TRY( AllocCache(&ppc.code_cache, cfg->native_cache_size, cfg->native_cache_guard_size) );

	ppc.hot_threshold = cfg->hot_threshold;

	SetupJumpTables();

	SetupMaskTable();

	if (NULL == (ppc.IRQCallback = IRQCallback))
		return DRPPC_INVALID_CONFIG;

	SPR(SPR_PVR) = pvr;

	DRPPC_TRY( InitModel(cfg, pvr) );

	DRPPC_TRY( IR_Init(cfg) );

	DRPPC_TRY( Target_Init(&ppc.code_cache) );

	return DRPPC_OKAY;
}

/*
 * INT PowerPC_Reset (void);
 *
 * It resets the current context and all the other emulation components. It does
 * the same (somewhat) as if the emulated CPU was reset.
 *
 * Parameters:
 *
 *	None.
 *
 * Returns:
 *
 *	DRPPC_OKAY if no error occurred.
 *	DRPPC_ERROR if MMap_Setup failed (because of invalid fetch/read/write ptrs).
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
		ppc.cr[i] = 0;

	ppc.cr0.lt = ppc.cr0.gt = ppc.cr0.eq = ppc.cr0.so = 0;
	ppc.cr1.lt = ppc.cr1.gt = ppc.cr1.eq = ppc.cr1.so = 0;

	ppc.irq_state = 0;
	ppc.timebase = 0;

	DRPPC_TRY( MMap_Setup(&ppc.mmap) );

	DRPPC_TRY( ResetModel() );

	UpdateFetchPtr();

	ppc.InvalidateBBLookup();

	return DRPPC_OKAY;
}

/*
 * void PowerPC_Shutdown (void);
 *
 * Call this routine when you're done with drppc. It will simply free all of the
 * allocated structures. You can feed this one to atexit, if you wish.
 *
 * Parameters:
 *
 *	None.
 *
 * Returns:
 *
 *	Nothing.
 */

void PowerPC_Shutdown (void)
{
	ShutdownModel();

	ppc.CleanBBLookup();

	IR_Shutdown();

	Target_Shutdown();
}

/*
 * INT PowerPC_Run (INT cycles, INT * err);
 *
 * This routine runs the emulated context for the specified amount of cycles.
 *
 * Parameters:
 *
 *	- INT cycles
 *
 *	  The amount of cycles to run for.
 *
 *	- INT * err
 *
 *	  A non-NULL pointer to an integer to hold the return error code.
 *
 * Returns:
 *
 *	The number of effective cycles the CPU has been emulated for, minus the
 *	specified amoung of cycles. The emulator isn't cycle-perfect, so it will
 *	probably run a few cycles *more* than specified; so, the returned value
 *	will very likely be positive.
 *
 *	err will eventually assume one of the following error codes:
 *
 *	DRPPC_OKAY when no error happened -- or we didn't notice it ;)
 *	DRPPC_BAD_PC when the code branches to an invalid location.
 *	DRPPC_ERROR when a generic error happens (self-generated, miracolous errors.
 *	You won't see many of these. ;) In case, take a look at UpdateFetchPtr.)
 */

INT PowerPC_Run (INT cycles, INT * err)
{
	return Interpret (cycles, err);
}

/*
 * void PowerPC_AddCycles (INT cycles);
 *
 * This routine adds the specified number of cycles to the cycles remaining
 * to run.
 *
 * Parameters:
 *
 *	- INT cycles
 *
 *	The amount of cycles to add.
 *
 * Returns:
 *
 *	Nothing.
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
 *
 * Parameters:
 *
 *	None.
 *
 * Returns:
 *
 *	Nothing.
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
 * Parameters:
 *
 *	None.
 *
 * Returns:
 *
 *	The number of cycles left.
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
 *
 *	- UINT state
 *
 *	  The new IRQ line state. 0 means off, 1 means on.
 *
 * Returns:
 *
 *	Nothing.
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
 *
 *	- PPC_CONTEXT * ctx
 *
 *	  The context to write from (on Set) or to read to (on Get). It shall be
 *	  a non-NULL pointer to a valid PPC_CONTEXT struct.
 *
 * Returns:
 *
 *	The size of the context structure in bytes, or -1 on error.
 *
 * Notes:
 *
 *	All of the structure fields are considered valid, on Get. That is, if you
 *	implement some kind of save state mechanism externally to drppc, you should
 *	provide valid function handlers and region descriptors in ctx, not
 *	merely the ones you saved in the file, otherwise you'll end up crashing the
 *  emulator (as the old pointers won't point to valid memory anymore.)
 */

INT PowerPC_GetContext (PPC_CONTEXT * ctx)
{
	if (ctx == NULL)
		return -1;

	memcpy((void *)ctx, (void *)&ppc, sizeof(PPC_CONTEXT));

	return sizeof(PPC_CONTEXT);
}

INT PowerPC_SetContext (PPC_CONTEXT * ctx)
{
	if (ctx == NULL)
		return -1;

	memcpy((void *)&ppc, (void *)ctx, sizeof(PPC_CONTEXT));

	MMap_Setup (&ctx->mmap); // Update the memory map system.

	return sizeof(PPC_CONTEXT);
}

/*******************************************************************************
 Additional routines that won't necessarily be included in the final version.
*******************************************************************************/

#if 0

// TODO: how do we handle save states?

INT PowerPC_SaveState (FILE *fp)
{
/*
    fwrite(ppc.gpr, sizeof(UINT32), 32, fp);
    fwrite(ppc.fpr, sizeof(FPR), 32, fp);
    fwrite(&ppc.pc, sizeof(UINT32), 1, fp);
    fwrite(&ppc.msr, sizeof(UINT32), 1, fp);
    fwrite(&ppc.fpscr, sizeof(UINT32), 1, fp);
    fwrite(&ppc.count, sizeof(UINT32), 1, fp);
    fwrite(ppc.cr, sizeof(UINT8), 8, fp);
    fwrite(ppc.sr, sizeof(UINT32), 16, fp);
    fwrite(ppc.tgpr, sizeof(UINT32), 4, fp);
    fwrite(&ppc.dec_expired, sizeof(UINT32), 1, fp);
    fwrite(&ppc.reserved, sizeof(UINT32), 1, fp);
    fwrite(ppc.spr, sizeof(UINT32), 1024, fp);
    fwrite(&ppc.irq_state, sizeof(UINT32), 1, fp);
    fwrite(&ppc.tb, sizeof(UINT64), 1, fp);
*/

	return DRPPC_OKAY;
}

INT PowerPC_LoadState (FILE *fp)
{
/*
    fread(ppc.gpr, sizeof(UINT32), 32, fp);
    fread(ppc.fpr, sizeof(FPR), 32, fp);
    fread(&ppc.pc, sizeof(UINT32), 1, fp);
    fread(&ppc.msr, sizeof(UINT32), 1, fp);
    fread(&ppc.fpscr, sizeof(UINT32), 1, fp);
    fread(&ppc.count, sizeof(UINT32), 1, fp);
    fread(ppc.cr, sizeof(UINT8), 8, fp);
    fread(ppc.sr, sizeof(UINT32), 16, fp);
    fread(ppc.tgpr, sizeof(UINT32), 4, fp);
    fread(&ppc.dec_expired, sizeof(UINT32), 1, fp);
    fread(&ppc.reserved, sizeof(UINT32), 1, fp);
    fread(ppc.spr, sizeof(UINT32), 1024, fp);
    fread(&ppc.irq_state, sizeof(UINT32), 1, fp);
    fread(&ppc.tb, sizeof(UINT64), 1, fp);

	ppc.cur_fetch.start = 0;
	ppc.cur_fetch.end = 0;
	ppc.cur_fetch.ptr = NULL;

	UpdateFetchPtr();

	MMap_Setup ( ??? );
*/

	return DRPPC_OKAY;
}

UINT32 PowerPC_GetReg (UINT reg)
{
	if (reg >= PPC_REG_R0 && reg <= PPC_REG_R31)
		return R(reg - PPC_REG_R0);

	switch (reg)
	{
	case PPC_REG_PC:
		return PC;
	case PPC_REG_CR:
		return	(CR(0) << 28) |
				(CR(1) << 24) |
				(CR(2) << 20) |
				(CR(3) << 16) |
				(CR(4) << 12) |
				(CR(5) <<  8) |
				(CR(6) <<  4) |
				(CR(7) <<  0);
	case PPC_REG_XER:
		return XER;
	case PPC_REG_LR:
		return SPR(SPR_LR);
	case PPC_REG_CTR:
		return SPR(SPR_CTR);
	case PPC_REG_MSR:
		return MSR;
	case PPC_REG_PVR:
		return SPR(SPR_PVR);

	/* TODO: 4XX */

	// 6XX

	case PPC_REG_FPSCR:
		return FPSCR;

	case PPC_REG_HID0:
		return(ppc.spr[SPR_HID0]);
	case PPC_REG_HID1:
		return(ppc.spr[SPR_HID1]);

	case PPC_REG_IBAT0L: return(ppc.spr[SPR_IBAT0L]);
	case PPC_REG_IBAT0H: return(ppc.spr[SPR_IBAT0U]);
	case PPC_REG_IBAT1L: return(ppc.spr[SPR_IBAT1L]);
	case PPC_REG_IBAT1H: return(ppc.spr[SPR_IBAT1U]);
	case PPC_REG_IBAT2L: return(ppc.spr[SPR_IBAT2L]);
	case PPC_REG_IBAT2H: return(ppc.spr[SPR_IBAT2U]);
	case PPC_REG_IBAT3L: return(ppc.spr[SPR_IBAT3L]);
	case PPC_REG_IBAT3H: return(ppc.spr[SPR_IBAT3U]);

	case PPC_REG_DBAT0L: return(ppc.spr[SPR_DBAT0L]);
	case PPC_REG_DBAT0H: return(ppc.spr[SPR_DBAT0U]);
	case PPC_REG_DBAT1L: return(ppc.spr[SPR_DBAT1L]);
	case PPC_REG_DBAT1H: return(ppc.spr[SPR_DBAT1U]);
	case PPC_REG_DBAT2L: return(ppc.spr[SPR_DBAT2L]);
	case PPC_REG_DBAT2H: return(ppc.spr[SPR_DBAT2U]);
	case PPC_REG_DBAT3L: return(ppc.spr[SPR_DBAT3L]);
	case PPC_REG_DBAT3H: return(ppc.spr[SPR_DBAT3U]);

	case PPC_REG_SPRG0: return(ppc.spr[SPR_SPRG0]);
	case PPC_REG_SPRG1: return(ppc.spr[SPR_SPRG1]);
	case PPC_REG_SPRG2: return(ppc.spr[SPR_SPRG2]);
	case PPC_REG_SPRG3: return(ppc.spr[SPR_SPRG3]);

	case PPC_REG_SRR0: return(ppc.spr[SPR_SRR0]);
	case PPC_REG_SRR1: return(ppc.spr[SPR_SRR1]);
	case PPC_REG_SRR2: return(ppc.spr[SPR_SRR2]);
	case PPC_REG_SRR3: return(ppc.spr[SPR_SRR3]);

	case PPC_REG_SR0: return(ppc.sr[0]);
	case PPC_REG_SR1: return(ppc.sr[1]);
	case PPC_REG_SR2: return(ppc.sr[2]);
	case PPC_REG_SR3: return(ppc.sr[3]);
	case PPC_REG_SR4: return(ppc.sr[4]);
	case PPC_REG_SR5: return(ppc.sr[5]);
	case PPC_REG_SR6: return(ppc.sr[6]);
	case PPC_REG_SR7: return(ppc.sr[7]);
	case PPC_REG_SR8: return(ppc.sr[8]);
	case PPC_REG_SR9: return(ppc.sr[9]);
	case PPC_REG_SR10: return(ppc.sr[10]);
	case PPC_REG_SR11: return(ppc.sr[11]);
	case PPC_REG_SR12: return(ppc.sr[12]);
	case PPC_REG_SR13: return(ppc.sr[13]);
	case PPC_REG_SR14: return(ppc.sr[14]);
	case PPC_REG_SR15: return(ppc.sr[15]);

	case PPC_REG_SDR1: return(ppc.spr[SPR_SDR1]);

	case PPC_REG_DSISR: return(0);
	case PPC_REG_DAR: return(0);

	/* TODO: Gekko */

	default:
		return(0xFFFFFFFF);
	}
}

void PowerPC_SetReg (UINT reg, UINT32 val)
{
	switch(reg)
	{
	case PPC_REG_PC:
		PC = val;
		UpdateFetchPtr();
		break;
	case PPC_REG_XER:
		SPR(SPR_XER) = val;
		break;
	case PPC_REG_LR:
		SPR(SPR_LR) = val;
		break;
	case PPC_REG_CR:
		CR(0) = (val >> 28) & 15;
		CR(1) = (val >> 24) & 15;
		CR(2) = (val >> 20) & 15;
		CR(3) = (val >> 16) & 15;
		CR(4) = (val >> 12) & 15;
		CR(5) = (val >>  8) & 15;
		CR(6) = (val >>  4) & 15;
		CR(7) = (val >>  0) & 15;
		break;
	default:
		Error("PowerPC_SetReg(%d, %08X)\n", reg, val);
	}
}

#endif
