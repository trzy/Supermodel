// TODO: check for endianess issues in load/store covers once more.
// TODO: a good idea would be to precompute (on Target_Init) the right ModR/M
//       byte for each emulated register, not to have to call IsEBPCached
//       anymore at recompile time.
// TODO: write GenPrintflike (printfun, format, ...) that parses the optional
//       arguments, computes how much stack space is needed, makes space for the
//       format string on the stack, moves it there, then pushes all of the args
//       based on their format (ptr for integers, etc.) and calls printfun when
//		 it's done. It'll be used alot for debugging.
/*
 * back/x86/x86.c
 *
 * Intel X86 Backend.
 */

#include <string.h>	// memset in Target_GenerateBB

#include "drppc.h"
#include "toplevel.h"
#include "mempool.h"
#include "mmap.h"
#include "middle/ir.h"
#include "middle/irdasm.h"
#include "back/ccache.h"
#include "disasm.h"
#include "emit.h"
#include "native.h"
#include SOURCE_CPU_HEADER
#include TARGET_CPU_HEADER

/*******************************************************************************
 Configuration
*******************************************************************************/

#define PRINT_X86_DISASM		1
#define PRINT_X86_BB_EXECUTION	0

/*******************************************************************************
 Local Variables
*******************************************************************************/

static INT		(* GenerateOp[NUM_IR_OPS])(IR *);
static INT		reg_location[NUM_DFLOW_BITS];

static UINT32	* context_pc;
static void		* context_base;
static REG_DESC	* context_desc;
static void		(* UpdateTimers)(INT);
static INT		(* UpdateFetch)(UINT32);

static DRPPC_REGION * (* read_region_table[4])(UINT32) =
{
	MMap_FindRead8Region,
	MMap_FindRead16Region,
	MMap_FindRead32Region,
	MMap_FindRead32Region,
};

static DRPPC_REGION * (* write_region_table[4])(UINT32) =
{
	MMap_FindWrite8Region,
	MMap_FindWrite16Region,
	MMap_FindWrite32Region,
	MMap_FindWrite32Region,
};

static UINT32 (* generic_read_table[4])(UINT32) =
{
	MMap_GenericRead8,
	MMap_GenericRead16,
	MMap_GenericRead32,
	MMap_GenericRead32,
};

static void (* generic_write_table[4])(UINT32, UINT32) =
{
	MMap_GenericWrite8,
	MMap_GenericWrite16,
	MMap_GenericWrite32,
	MMap_GenericWrite32,
};

/*******************************************************************************
 Profiling
*******************************************************************************/

/*
 * UINT64 ReadTSC (void);
 *
 * Returns the current contents of the timestamp counter X86 CPU register.
 */

static UINT64 ReadTSC (void)
{
	UINT32 hi, lo;

#ifdef __GNUC__
	__asm__ __volatile__ ("rdtsc" : "=&a" (lo), "=&d" (hi));
#else
	__asm
	{
		push eax
		push edx
		rdtsc
		mov hi, edx
		mov lo, eax
		pop edx
		pop eax
	}
#endif

	return ((UINT64)hi << 32) | (UINT64)lo;
}

/*******************************************************************************
 Register Location Tracking
*******************************************************************************/

/*
 * The idea is: reg_location[] keeps track of the current location for each of
 * the dflow bits. The meaning of a reg_location[] entry is:
 *
 *  lower than zero	= the register is located in memory (see GetRegContextPtr.)
 *	zero or greater	= the entry value specifies the native register holding the
 *					  given source CPU / temporary register.
 */

static BOOL IsCached (UINT reg)
{
	ASSERT(reg < NUM_DFLOW_BITS);

	return reg_location[reg] >= 0;
}

static UINT CombineIsCached2 (UINT reg1, UINT reg2)
{
	UINT res1 = IsCached(reg1) ? 2 : 0;
	UINT res2 = IsCached(reg2) ? 1 : 0;

	return res1 | res2;
}

static INT GetNativeReg (UINT reg)
{
	ASSERT(reg < NUM_DFLOW_BITS);

	return reg_location[reg];
}

static void SetNativeReg (UINT reg, INT location)
{
	ASSERT(reg < NUM_DFLOW_BITS);

	reg_location[reg] = location;
}

/*
 * Location of the memory image of a register.
 */

static void * GetRegContextPtr (UINT dflow_bit)
{
	ASSERT(dflow_bit < NUM_DFLOW_BITS);
	ASSERT(context_desc[dflow_bit].ptr != NULL);

	return context_desc[dflow_bit].ptr;
}

static BOOL IsEBPCached (void * ptr)
{
	return	(UINT32)ptr >= (UINT32)context_base &&
			(UINT32)ptr < ((UINT32)context_base + 256);
}

static UINT32 EBPOffs (void * ptr)
{
	return	(UINT32)ptr - ((UINT32)context_base + 128);
}

/*
 * Stack operations. (Pay attention to unordered stack moves!)
 */

static void SaveReg_32 (UINT reg)
{
	// It shouldn't push a reg that isn't used

	_PushReg_32(reg);
}

static void RestoreReg_32 (UINT reg)
{
	// It shouldn't pop a reg that wasn't saved (and eventually report an
	// error.)

	_PopReg_32(reg);
}

/*******************************************************************************
 Cover Helpers
*******************************************************************************/

/*
 * void PushMem_32, PushReg_32
 */

static void PushMem_32 (void * ptr)
{
	if (IsEBPCached(ptr))
		_PushMemRegRel_32(EBP, EBPOffs(ptr));
	else
		_PushMemAbs_32((UINT32)ptr);
}

static void PushReg_32 (UINT reg)
{
	if (IsCached(reg))
		_PushReg_32(GetNativeReg(reg));
	else
		PushMem_32(GetRegContextPtr(reg));
}

/*
 * void PopMem_32, PopReg_32
 */

static void PopMem_32 (void * ptr)
{
	if (IsEBPCached(ptr))
		_PopMemRegRel_32(EBP, EBPOffs(ptr));
	else
		_PopMemAbs_32((UINT32)ptr);
}

static void PopReg_32 (UINT reg)
{
	if (IsCached(reg))
		_PopReg_32(GetNativeReg(reg));
	else
		PopMem_32(GetRegContextPtr(reg));
}

/*
 * MovImmToMem_32, MovImmToReg_32
 */

static void MovImmToMem_32 (void * ptr, UINT32 imm)
{
	if (IsEBPCached(ptr))
		_MovImmToMemRegRel_32(EBP, EBPOffs(ptr), imm);
	else
		_MovImmToMemAbs_32((UINT32)ptr, imm);
}

static void MovImmToReg_32 (UINT reg, UINT32 imm)
{
	if (IsCached(reg))
		_MovImmToReg_32(GetNativeReg(reg), imm);
	else
		MovImmToMem_32(GetRegContextPtr(reg), imm);
}

/*
 * MovRegToMem_32, MovMemToReg_32
 */

static void MovMemToReg_32 (UINT reg, UINT32 ptr)
{
	if (IsCached(reg))
		_MovMemAbsToReg_32(GetNativeReg(reg), ptr);
	else
	{
		_PushMemAbs_32(ptr);
		PopReg_32(reg);
	}
}

static void MovRegToMem_32 (UINT reg, UINT32 ptr)
{
	if (IsCached(reg))
		_MovRegToMemAbs_32(GetNativeReg(reg), ptr);
	else
	{
		PushReg_32(reg);
		_PopMemAbs_32(ptr);
	}
}

/*
 * void MovRegToNReg (UINT nreg, UINT reg);
 *
 * Generates code to move the contents of emulated register reg to native
 * register nreg.
 */

static void MovRegToNReg (UINT nreg, UINT reg)
{
	if (IsCached(reg))
	{
		reg = GetNativeReg(reg);
		if (reg != nreg)
			_MovRegToReg_32(nreg, reg);
	}
	else
	{
		void * ptr = GetRegContextPtr(reg);

		if (IsEBPCached(ptr))
			_MovMemRegRelToReg_32(nreg, EBP, EBPOffs(ptr));
		else
			_MovMemAbsToReg_32(nreg, (UINT32)ptr);
	}
}

/*
 * void MovNRegToReg (UINT reg, UINT nreg);
 *
 * Generates code to move the contents of native register nreg to emulated
 * register reg.
 */

static void MovNRegToReg (UINT reg, UINT nreg)
{
	if (IsCached(reg))
	{
		reg = GetNativeReg(reg);
		if (reg != nreg)
			_MovRegToReg_32(reg, nreg);
	}
	else
	{
		void * ptr = GetRegContextPtr(reg);

		if (IsEBPCached(ptr))
			_MovRegToMemRegRel_32(nreg, EBP, EBPOffs(ptr));
		else
			_MovRegToMemAbs_32(nreg, (UINT32)ptr);
	}
}

/*
 * void MovRegToReg (UINT dest, UINT src);
 *
 * Generates code to move the contents of emulated register src to emulated
 * register dest.
 */

static void MovRegToReg (UINT dest, UINT src)
{
	UINT	cd = IsCached(dest) ? 2 : 0;
	UINT	cs = IsCached(src)  ? 1 : 0;

	if (dest == src)
		return;

	switch (cd | cs)
	{
	case 0:		// mem <- mem
		PushReg_32(src);
		PopReg_32(dest);
		break;
	case 1:		// mem <- reg
		src  = GetNativeReg(src);
		MovNRegToReg(dest, src);
		break;
	case 2:		// reg <- mem
		dest = GetNativeReg(dest);
		MovRegToNReg(dest, src);
		break;
	default:	// reg <- reg
		dest = GetNativeReg(dest);
		src  = GetNativeReg(src);
		_MovRegToReg_32(dest, src);
		break;
	}
}

#define OP_REG_TO_REG_32(Name, dest, src)								\
																		\
	switch (CombineIsCached2(dest, src))								\
	{																	\
	case 0:		/* mem -> mem */										\
		{																\
			void * destp = GetRegContextPtr(dest);						\
			SaveReg_32(EAX);											\
			MovRegToNReg(EAX, src);										\
			if (IsEBPCached(destp))										\
				_##Name##RegToMemRegRel_32(EAX, EBP, EBPOffs(destp));	\
			else														\
				_##Name##RegToMemAbs_32(EAX, (UINT32)destp);			\
			RestoreReg_32(EAX);											\
		}																\
		break;															\
	case 1:		/* reg -> mem */										\
		ASSERTEX(0, { Print("reg->mem"); });							\
		break;															\
	case 2:		/* mem -> reg */										\
		ASSERTEX(0, { Print("mem->reg"); });							\
		break;															\
	default:	/* reg -> reg */										\
		ASSERTEX(0, { Print("reg->reg"); });							\
		break;															\
	}


static void AddRegToReg_32 (UINT dest, UINT src)
{
	OP_REG_TO_REG_32( Add, dest, src );
}

static void AndRegToReg_32 (UINT dest, UINT src)
{
	OP_REG_TO_REG_32( And, dest, src );
}

static void OrRegToReg_32 (UINT dest, UINT src)
{
	OP_REG_TO_REG_32( Or, dest, src );
}

static void XorRegToReg_32 (UINT dest, UINT src)
{
	OP_REG_TO_REG_32( Xor, dest, src );
}

#define OP_IMM_TO_REG_32(Name, _reg, _imm)						\
																\
	if (IsCached(_reg))											\
	{															\
		ASSERT(0);												\
	}															\
	else														\
	{															\
		void * ptr = GetRegContextPtr(reg);						\
		if (IsEBPCached(ptr))									\
			_##Name##ImmToMemRegRel_32(EBP, EBPOffs(ptr), imm);	\
		else													\
			_##Name##ImmToMemAbs_32((UINT32)ptr, imm);			\
	}

static void AddImmToReg_32 (UINT reg, UINT imm)
{
	OP_IMM_TO_REG_32( Add, reg, imm );
}

static void AndImmToReg_32 (UINT reg, UINT imm)
{
	OP_IMM_TO_REG_32( And, reg, imm );
}

static void OrImmToReg_32 (UINT reg, UINT imm)
{
	OP_IMM_TO_REG_32( Or, reg, imm);
}

static void XorImmToReg_32 (UINT reg, UINT imm)
{
	OP_IMM_TO_REG_32( Xor, reg, imm );
}

#define SHIFT_ROT_REG_BY_SA( Op, reg, sa )								\
		{																\
			ASSERT(sa < 256);											\
																		\
			if (IsCached(reg))											\
			{															\
				ASSERT(0);												\
			}															\
			else														\
			{															\
				void * ptr = GetRegContextPtr(reg);						\
																		\
				if (IsEBPCached(ptr))									\
					_##Op##MemRegRelByImm_32(EBP, EBPOffs(ptr), sa);	\
				else													\
					_##Op##MemAbsByImm_32((UINT32)ptr, sa);				\
			}															\
		}

static void ShlRegBySa (UINT _reg, UINT32 _sa)
{
	SHIFT_ROT_REG_BY_SA( Shl, _reg, _sa);
}

static void ShrRegBySa (UINT _reg, UINT32 _sa)
{
	SHIFT_ROT_REG_BY_SA( Shr, _reg, _sa);
}

static void RolRegBySa (UINT _reg, UINT32 _sa)
{
	SHIFT_ROT_REG_BY_SA( Rol, _reg, _sa );
}

static void RorRegBySa (UINT _reg, UINT32 _sa)
{
	SHIFT_ROT_REG_BY_SA( Ror, _reg, _sa );
}

#define OP_REG_TO_REG_32_SINGLE(Name, dest, src)		\
														\
	switch (CombineIsCached2(dest, src))				\
	{													\
	case 0:		/* mem -> mem */						\
		{												\
		void * dstp = GetRegContextPtr(dest);			\
		void * srcp = GetRegContextPtr(src);			\
														\
		MovRegToReg(dest, src);							\
														\
		if (IsEBPCached(dstp))							\
			_##Name##MemRegRel_32(EBP, EBPOffs(dstp));	\
		else											\
			_##Name##MemAbs_32((UINT32)dstp);			\
		}												\
		break;											\
	case 1:		/* reg -> mem */						\
		ASSERTEX(0, { Print("reg -> mem"); });			\
		break;											\
	case 2:		/* mem -> reg */						\
		ASSERTEX(0, { Print("mem -> reg"); });			\
		break;											\
	default:	/* reg -> reg */						\
		ASSERTEX(0, { Print("reg -> reg"); });			\
		break;											\
	}

static void NotRegToReg_32 (UINT dest, UINT src)
{
	OP_REG_TO_REG_32_SINGLE ( Not, dest, src );
}

static void NegRegToReg_32 (UINT dest, UINT src)
{
	OP_REG_TO_REG_32_SINGLE ( Neg, dest, src );
}

static void SetccReg_8 (X86_CC cc, UINT reg)
{
	if (IsCached(reg))
	{
		ASSERT(0);
	}
	else
	{
		void * ptr = GetRegContextPtr(reg);

		if (IsEBPCached(ptr))
		{
			_MovImmToMemRegRel_32(EBP, EBPOffs(ptr), 0);
			_SetccMemRegRel_8(cc, EBP, EBPOffs(ptr));
		}
		else
		{
			_MovImmToMemAbs_32((UINT32)ptr, 0);
			_SetccMemAbs_8(cc, (UINT32)ptr);
		}
	}
}

static void SubRegToReg_32 (UINT dest, UINT src)
{
	switch (CombineIsCached2(dest, src))
	{
	case 0:
		{
			void * ptr = GetRegContextPtr(dest);

			_PushReg_32(EAX);
			MovRegToNReg(EAX, src);
			if (IsEBPCached(ptr))
				_SubRegToMemRegRel_32(EAX, EBP, EBPOffs(ptr));
			else
				_SubRegToMemAbs_32(EAX, (UINT32)ptr);
			_PopReg_32(EAX);
		}
		break;
	case 1:
	case 2:
	case 3:
		ASSERT(0);
		break;
	}
}

static void MuluRegWithImm_32 (UINT reg, UINT32 imm)
{
	if (IsCached(reg))
	{
		ASSERT(0);
	}
	else
	{
		void * ptr = GetRegContextPtr(reg);

		_PushReg_32(EDX);
		_PushReg_32(EAX);

		_MovImmToReg_32(EAX, imm);

		if(IsEBPCached(ptr))
			_MulMemRegRel_32(EBP, EBPOffs(ptr));
		else
			_MulMemAbs_32((UINT32)ptr);

		MovNRegToReg(reg, EAX);

		_PopReg_32(EAX);
		_PopReg_32(EDX);
	}
}

static void CmpRegToImm_32 (UINT reg, UINT32 imm)
{
	if (IsCached(reg))
	{
		ASSERT(0);
	}
	else
	{
		void * ptr = GetRegContextPtr(reg);

		if (IsEBPCached(ptr))
			_CmpMemRegRelToImm_32(EBP, EBPOffs(ptr), imm);
		else
			_CmpMemAbsToImm_32((UINT32)ptr, imm);
	}
}

static void CmpImmToReg_32 (UINT32 imm, UINT reg)
{
	if (IsCached(reg))
	{
		ASSERT(0);
	}
	else
	{
		void * ptr = GetRegContextPtr(reg);

		SaveReg_32(EAX);

		_MovImmToReg_32(EAX, imm);
		_CmpRegToMemAbs_32(EAX, (UINT32)ptr);

		RestoreReg_32(EAX);
	}
}

static void CmpRegToReg_32 (UINT reg1, UINT reg2)
{
	switch (CombineIsCached2(reg1, reg2))
	{
	case 0:		// cmp mem, mem
		_PushReg_32(EAX);
		MovRegToNReg(EAX, reg1);
		if (IsCached(reg2))
		{
			ASSERT(0);
		}
		else
		{
			void * ptr = GetRegContextPtr(reg2);
			if (IsEBPCached(ptr))
				_CmpRegToMemRegRel_32(EAX, EBP, EBPOffs(ptr));
			else
				_CmpRegToMemAbs_32(EAX, (UINT32)ptr);
		}
		_PopReg_32(EAX);
		break;
	case 1:		// cmp mem, reg
		ASSERT(0);
		break;
	case 2:		// cmp reg, mem
		ASSERT(0);
		break;
	default:	// cmp reg, reg
		ASSERT(0);
		break;
	}
}

/*
 * void GenerateLoad_KnownHandler ( ... );
 */

static void GenerateLoad_KnownHandler
(
	IR				* ir,
	DRPPC_REGION	* region,
	UINT32			addr
)
{
	UINT32	dest = IR_OPERAND_DEST(ir);

	if (IR_SIZE(ir) != IR_INT64)
	{
		/*
		 * push	eax
		 * push	#addr
		 * call	#region->handler
		 * mov	dest, eax
		 * add	esp, 4
		 * pop	eax
		 */

		// TODO: what if EAX holds dest?

		SaveReg_32(EAX);

		_PushImm_32(addr);
		_CallDirect((UINT32)region->handler);

		MovNRegToReg(dest, EAX);

		_AddImmToReg_32(ESP, 4);

		RestoreReg_32(EAX);
	}
	else
	{
		/*
		 * push	eax
		 * push	#addr+0
		 * call	#region->handler
		 * mov	dest.hi, eax
		 * add	esp, 4
		 * push	#addr+4
		 * call	#region->handler
		 * mov	dest.lo, eax
		 * add	esp, 4
		 * pop	eax
		 */

		UINT32 dstp = (UINT32)GetRegContextPtr(dest);

		ASSERT(!IsCached(dest));	// no support for 64-bit reg allocation

		SaveReg_32(EAX);

		_PushImm_32(addr+0);
		_CallDirect((UINT32)region->handler);
		MovRegToMem_32(EAX, dstp+4);
		_AddImmToReg_32(ESP, 4);

		_PushImm_32(addr+4);
		_CallDirect((UINT32)region->handler);
		MovRegToMem_32(EAX, dstp+0);
		_AddImmToReg_32(ESP, 4);

		RestoreReg_32(EAX);
	}
}

/*
 * void GenerateStore_KnownHandler ( ... );
 */

static void GenerateStore_KnownHandler
(
	IR				* ir,
	DRPPC_REGION	* region,
	UINT32			addr
)
{
	UINT32	data = IR_OPERAND_SRC2(ir);
	UINT32	Handler;

	Handler = (UINT32)region->handler;

	if (IR_SIZE(ir) < IR_INT64)
	{
		/*
		 * push		data
		 * push		addr
		 * call		Handler
		 * add		esp, 8
		 */

		if (IR_DATA_CONST(ir))
			_PushImm_32(data);
		else
			PushReg_32(data);

		_PushImm_32(addr);
		_CallDirect(Handler);
		_AddImmToReg_32(ESP, 8);
	}
	else
	{
		/*
		 * push		data.hi
		 * push		addr+0
		 * call		Handler
		 * push		data.lo
		 * push		addr+4
		 * call		Handler
		 * add		esp, 16
		 */

		void * ptr;

		ASSERT(!IR_DATA_CONST(ir));	// no support for 64-bit constants
		ASSERT(!IsCached(data));	// no support for 64-bit reg allocation

		ptr = GetRegContextPtr(data);

		PushMem_32((void *)((UINT32)ptr+4));
		_PushImm_32(addr+0);
		_CallDirect(Handler);

		PushMem_32((void *)((UINT32)ptr+0));
		_PushImm_32(addr+4);
		_CallDirect(Handler);

		_AddImmToReg_32(ESP, 8);
	}
}

/*
 * void GenerateLoad_KnownDirect ( ... );
 */

static void GenerateLoad_KnownDirect
(
	IR				* ir,
	DRPPC_REGION	* region,
	UINT32			addr
)

{
	UINT32	srcp = (UINT32)(&region->ptr[addr - region->start]);
	UINT32	dest = IR_OPERAND_DEST(ir);

	ASSERT(!region->volatile_ptr);	// no support for volatile regions yet

	switch (IR_SIZE(ir))
	{
	case IR_INT8:

		/*
		 * push		eax
		 * xor		eax, eax
		 * mov		al, [srcp]
		 * mov		dest, eax
		 * pop		eax
		 */

		SaveReg_32(EAX);
		_XorRegToReg_32(EAX, EAX);
		_MovMemAbsToReg_8(AL, srcp);
		MovNRegToReg(dest, EAX);
		RestoreReg_32(EAX);
		break;
	case IR_INT16:
		if (region->big_endian)
		{
			/*
			 * push		eax
			 * xor		eax, eax
			 * mov		ah, [srcp]
			 * xchg		ah, al
			 * mov		dest, eax
			 * pop		eax
			 */

			SaveReg_32(EAX);
			_XorRegToReg_32(EAX, EAX);
			_MovMemAbsToReg_16(AX, srcp);
			_XchgRegToReg_8(AH, AL);
			MovNRegToReg(dest, EAX);
			RestoreReg_32(EAX);
		}
		else
		{
			/*
			 * push		#0
			 * push		[srcp]
			 * pop		dest
			 */

			_PushImm_16(0);
			_PushMemAbs_16(srcp);
			PopReg_32(dest);
		}
		break;
	case IR_INT32:
		if (region->big_endian)
		{
			/*
			 * push		eax
			 * mov		eax, srcp
			 * bswap	eax
			 * mov		dest, eax
			 * pop		eax
			 */

			SaveReg_32(EAX);
			_MovMemAbsToReg_32(EAX, srcp);
			_BswapReg_32(EAX);
			MovNRegToReg(dest, EAX);
			RestoreReg_32(EAX);
		}
		else
		{
			/*
			 * push		#0
			 * push		[srcp]
			 * pop		dest
			 */

			_PushMemAbs_32(srcp);
			PopReg_32(dest);
		}
		break;
	case IR_INT64:
		ASSERT(0);
		break;
	}
}

/*
 * void GenerateStore_KnownDirect ( ... );
 */

static void GenerateStore_KnownDirect
(
	IR				* ir,
	DRPPC_REGION	* region,
	UINT32			addr
)
{
	UINT32	dest = (UINT32)(&region->ptr[addr - region->start]);
	UINT32	data = IR_OPERAND_SRC2(ir);

	ASSERT(!region->volatile_ptr);	// no support for volatile regions yet

	switch (IR_SIZE(ir))
	{
	case IR_INT8:

		/*
		 * push		eax
		 * mov		eax, data
		 * mov		[dest], al
		 * pop		eax
		 */

		SaveReg_32(EAX);
		MovRegToNReg(EAX, data);
		_MovRegToMemAbs_8(AL, dest);
		RestoreReg_32(EAX);
		break;
	case IR_INT16:
		if (region->big_endian)
		{
			/*
			 * push		eax
			 * mov		eax, data
			 * xchg		ah, al
			 * mov		[dest], ax
			 * pop		eax
			 */

			ASSERT(0);

			// SaveReg_32(EAX);
			// MovRegToNReg(EAX, data);
			// _XchgRegToReg_16(AH, AL);
			// _MovRegToMemAbs_16(AX, dest);
			// RestoreReg_32(EAX);
		}
		else
		{
			/*
			 * push		data
			 * pop		[dest]
			 */

			ASSERT(0);

			// PushReg_16(data);
			// _PopMemAbs_16(dest);
		}
		break;
	case IR_INT32:
		if (region->big_endian)
		{
			/*
			 * push		eax
			 * mov		eax, data
			 * bswap	eax
			 * mov		[dest], eax
			 * pop		eax
			 */

			SaveReg_32(EAX);
			MovRegToNReg(EAX, data);
			_BswapReg_32(EAX);
			_MovRegToMemAbs_32(EAX, dest);
			RestoreReg_32(EAX);
		}
		else
		{
			/*
			 * push		data
			 * pop		[dest]
			 */

			PushReg_32(data);
			_PopMemAbs_32(dest);
		}
		break;
	case IR_INT64:
		{

		// FIXME: endianess!

		void * srcp;

		ASSERT(!IR_DATA_CONST(ir));
		ASSERT(!IsCached(data));

		srcp = GetRegContextPtr(data);

		if (region->big_endian)
		{
			/*
			 * push		eax
			 * push		edx
			 * mov		eax, src+4
			 * mov		edx, src+0
			 * bswap	eax
			 * bswap	edx
			 * mov		[dest+0], eax
			 * mov		[dest+4], edx
			 * pop		edx
			 * pop		eax
			 */

			ASSERT(0);
		}
		else
		{
			/*
			 * push		src+4
			 * push		src+0
			 * pop		[ptr+4]
			 * pop		[ptr+0]
			 */

			_PushMemAbs_32((UINT32)srcp+4);
			_PushMemAbs_32((UINT32)srcp+0);
			_PopMemAbs_32((UINT32)dest+4);
			_PopMemAbs_32((UINT32)dest+0);
		}

		}
		break;
	}
}

/*
 * void GenerateLoad_Unknown ( ... );
 */

static void GenerateLoad_Unknown
(
	IR		* ir,
	UINT32	(* Handler)(UINT32)
)
{
	UINT32	dest = IR_OPERAND_DEST(ir);
	UINT32	addr = IR_OPERAND_SRC1(ir);
	UINT32	offs = IR_OPERAND_SRC3(ir);

	if (IR_SIZE(ir) < IR_INT64)
	{
		if (offs == 0)
		{
			/*
			 * push		eax				## if eax is used
			 * push		addr
			 * call		Handler
			 * add		esp, 4
			 * mov		dest, eax
			 * pop		eax				## if eax was used
			 */

			if (!IsCached(dest) || GetNativeReg(dest))
				SaveReg_32(EAX);

			PushReg_32(addr);
			_CallDirect((UINT32)Handler);
			_AddImmToReg_32(ESP, 4);
			MovNRegToReg(dest, EAX);

			if (!IsCached(dest) || GetNativeReg(dest))
				RestoreReg_32(EAX);
		}
		else
		{
			/*
			 * push		eax
			 * push		addr
			 * mov		eax, esp
			 * add		[eax], offs
			 * call		Handler
			 * add		esp, 4
			 * mov		dest, eax
			 * ]add		esp, 4
			 * ]pop		eax
			 */

			PushReg_32(EAX);
			PushReg_32(addr);
			_MovRegToReg_32(EAX, ESP);
			_AddImmToMemReg_32(EAX, offs);
			_CallDirect((UINT32)Handler);
			_AddImmToReg_32(ESP, 4);
			MovNRegToReg(dest, EAX);

			if (IsCached(dest) && GetNativeReg(dest) == EAX)
				_AddImmToReg_32(ESP, 4);
			else
				PopReg_32(EAX);
		}
	}
	else	// IR_SIZE(ir) == IR_INT64
	{
		UINT32 destp = (UINT32)GetRegContextPtr(dest);

		/*
		 * push		eax
		 *
		 * push		addr
		 * mov		eax, esp
		 * add		[eax], offs+4
		 * call		Handler
		 * mov		dest.lo, eax
		 * add		esp, 4
		 *
		 * push		addr
		 * mov		eax, esp
		 * add		[eax], offs+0
		 * call		Handler
		 * mov		dest.hi, eax
		 * add		esp, 4
		 *
		 * pop		eax
		 */

		ASSERT(!IsCached(dest));

		_PushReg_32(EAX);

		PushReg_32(addr);
		_MovRegToReg_32(EAX, ESP);
		_AddImmToMemReg_32(EAX, offs+4);
		_CallDirect((UINT32)Handler);
		_MovRegToMemAbs_32(EAX, destp+0);
		_AddImmToReg_32(ESP, 4);

		PushReg_32(addr);
		_MovRegToReg_32(EAX, ESP);
		_AddImmToMemReg_32(EAX, offs+0);
		_CallDirect((UINT32)Handler);
		_MovRegToMemAbs_32(EAX, destp+4);
		_AddImmToReg_32(ESP, 4);

		_PopReg_32(EAX);
	}
}

/*
 * void GenerateStore_Unknown
 */

static void GenerateStore_Unknown
(
	IR		* ir,
	void	(* Handler)(UINT32, UINT32)
)
{
	UINT32	addr = IR_OPERAND_SRC1(ir);
	UINT32	data = IR_OPERAND_SRC2(ir);
	UINT32	offs = IR_OPERAND_SRC3(ir);

	if (IR_SIZE(ir) < IR_INT64)
	{
		/*
		 * push		eax
		 * push		data
		 * push		addr
		 * mov		eax, esp
		 * add		[eax], offs
		 * call		GenericWrite
		 * add		esp, 8
		 * pop		eax
		 */

		if (offs == 0)
		{
			SaveReg_32(EAX);
			if (IR_DATA_CONST(ir))
				_PushImm_32(data);
			else
				PushReg_32(data);
			PushReg_32(addr);
			_CallDirect((UINT32)Handler);
			_AddImmToReg_32(ESP, 8);
			RestoreReg_32(EAX);
		}
		else
		{
			PushReg_32(EAX);
			if (IR_DATA_CONST(ir))
				_PushImm_32(data);
			else
				PushReg_32(data);
			PushReg_32(addr);
			_MovRegToReg_32(EAX, ESP);
			_AddImmToMemReg_32(EAX, offs);
			_CallDirect((UINT32)Handler);
			_AddImmToReg_32(ESP, 8);
			PopReg_32(EAX);
		}
	}
	else	// IR_SIZE(ir) == IR_INT64
	{
		UINT32 datap = (UINT32)GetRegContextPtr(data);

		ASSERT(!IR_DATA_CONST(ir));

		/*
		 * push		eax
		 * push		data.hi
		 * push		addr
		 * mov		eax, esp
		 * add		[eax], offs+0
		 * call		Handler
		 * add		esp, 8
		 * push		data.lo
		 * push		addr
		 * mov		eax, esp
		 * add		[eax], offs+4
		 * call		Handler
		 * add		esp, 8
		 * pop		eax
		 */

		_PushReg_32(EAX);
		PushMem_32(datap+4);
		PushReg_32(addr);
		_MovRegToReg_32(EAX, ESP);
		_AddImmToMemReg_32(EAX, offs+0);
		_CallDirect((UINT32)Handler);
		_AddImmToReg_32(ESP, 8);
		PushMem_32(datap+0);
		PushReg_32(addr);
		_MovRegToReg_32(EAX, ESP);
		_AddImmToMemReg_32(EAX, offs+4);
		_CallDirect((UINT32)Handler);
		_AddImmToReg_32(ESP, 8);
		_PopReg_32(EAX);
	}
}

/*******************************************************************************
 Cover Generator Templates
*******************************************************************************/

/* For commutative operations only! */
#define GENERATE_ALU_3OP( op )						\
		{											\
			UINT	dest = IR_OPERAND_DEST(ir);		\
			UINT	src1 = IR_OPERAND_SRC1(ir);		\
			UINT	src2 = IR_OPERAND_SRC2(ir);		\
			UINT32	imm = IR_OPERAND_CONST(ir);		\
													\
			if (IR_CONSTANT(ir))					\
			{										\
				if (dest != src1)					\
					MovRegToReg(dest, src1);		\
				op##ImmToReg_32(dest, imm);			\
			}										\
			else									\
			{										\
				if (dest == src1 && dest == src2)	\
				{									\
					op##RegToReg_32(dest, dest);	\
				}									\
				else if (dest == src1)				\
				{									\
					op##RegToReg_32(dest, src2);	\
				}									\
				else if (dest == src2)				\
				{									\
					op##RegToReg_32(dest, src1);	\
				}									\
				else								\
				{									\
					MovRegToReg(dest, src1);		\
					op##RegToReg_32(dest, src2);	\
				}									\
			}										\
		}

#define GENERATE_SHIFT_ROT( Op )					\
		{											\
			if (IR_CONSTANT(ir))					\
			{										\
				UINT	dest = IR_OPERAND_DEST(ir);	\
				UINT	src  = IR_OPERAND_SRC1(ir);	\
				UINT32	sa   = IR_OPERAND_CONST(ir);\
													\
				ASSERT(sa < 32);					\
													\
				if (dest != src)					\
					MovRegToReg(dest, src);			\
													\
				Op##RegBySa(dest, sa);				\
			}										\
			else									\
			{										\
				ASSERT(0);							\
			}										\
		}

/*******************************************************************************
 Cover Generators
*******************************************************************************/

/*
 * INT GenerateUnhandled (IR * ir);
 */

static INT GenerateUnhandled (IR * ir)
{
	printf("Unhandled IR op (%u of %u)\n", IR_OPERATION(ir), NUM_IR_OPS);
	return DRPPC_ERROR;
}

/*
 * INT GenerateLoadi (IR * ir);
 */

static INT GenerateLoadi (IR * ir)
{
	UINT	dest = IR_OPERAND_DEST(ir);
	UINT32	imm	 = IR_OPERAND_CONST(ir);

	MovImmToReg_32(dest, imm);

	return DRPPC_OKAY;
}

/*
 * INT GenerateMove (IR * ir);
 */

static INT GenerateMove (IR * ir)
{
	UINT	dest = IR_OPERAND_DEST(ir);
	UINT	src  = IR_OPERAND_SRC1(ir);

	MovRegToReg(dest, src);

	return DRPPC_OKAY;
}

/*
 * INT GenerateAdd (IR * ir);
 */

static INT GenerateAdd (IR * ir)
{
	GENERATE_ALU_3OP( Add );

	return DRPPC_OKAY;
}

/*
 * INT GenerateSub (IR * ir);
 */

static INT GenerateSub (IR * ir)
{
	UINT	dest = IR_OPERAND_DEST(ir);
	UINT32	src1 = IR_OPERAND_SRC1(ir);
	UINT32	src2 = IR_OPERAND_SRC2(ir);

	ASSERT(IR_CONSTANT(ir) < 3);

	if (IR_CONSTANT(ir) == 0)
	{
		if (src1 == src2)		// dest = 0
			MovImmToReg_32(dest, 0);
		else
		{
			if (dest != src1)
				MovRegToReg(dest, src1);
			SubRegToReg_32(dest, src2);
		}
	}
	else if (IR_CONSTANT(ir) == 1)
	{
		ASSERT(0);	// dest = imm - src
	}
	else
	{
		ASSERT(0);	// dest = src - imm
	}

	return DRPPC_OKAY;
}

/*
 * INT GenerateNeg (IR * ir);
 */

static INT GenerateNeg (IR * ir)
{
	UINT	dest = IR_OPERAND_DEST(ir);
	UINT	src  = IR_OPERAND_SRC1(ir);

	ASSERT(!IR_CONSTANT(ir));

	NegRegToReg_32(dest, src);

	return DRPPC_OKAY;
}

/*
 * INT GenerateNot (IR * ir);
 */

static INT GenerateNot (IR * ir)
{
	UINT	dest = IR_OPERAND_DEST(ir);
	UINT	src  = IR_OPERAND_SRC1(ir);

	ASSERT(!IR_CONSTANT(ir));

	NotRegToReg_32(dest, src);

	return DRPPC_OKAY;
}

/*
 * INT GenerateMul (IR * ir);
 */

static INT GenerateMulu (IR * ir)
{
	// TODO: EBP caching!

	if (IR_CONSTANT(ir))
	{
		UINT	dest = IR_OPERAND_DEST(ir);
		UINT	src  = IR_OPERAND_SRC1(ir);
		UINT32	imm  = IR_OPERAND_CONST(ir);

		if (dest != src)
			MovRegToReg(dest, src);

		MuluRegWithImm_32(dest, imm);
	}
	else
	{
		UINT	dest = IR_OPERAND_DEST(ir);
		UINT	src1 = IR_OPERAND_SRC1(ir);
		UINT	src2 = IR_OPERAND_SRC2(ir);

		ASSERT(!IsCached(dest));
		ASSERT(!IsCached(src1));
		ASSERT(!IsCached(src2));

		if (dest == src1 && dest == src2)
		{
			ASSERT(0);
		}
		else if (dest == src1)
		{
			// mov eax, src1
			// mul src1
			// mov dest, eax

			MovRegToNReg(EAX, src1);
			_MulMemAbs_32((UINT32)GetRegContextPtr(src2));
			MovNRegToReg(dest, EAX);
		}
		else if (dest == src2)
		{
			ASSERT(0);
		}
		else if (src1 == src2)
		{
			ASSERT(0);
		}
		else
		{
			ASSERT(0);
		}
	}

	return DRPPC_OKAY;
}

/*
 * INT GenerateBrev (IR * ir);
 */

static INT GenerateBrev (IR * ir)
{
	UINT dest	= IR_OPERAND_DEST(ir);
	UINT src	= IR_OPERAND_SRC1(ir);

	if (dest == src)
	{
		ASSERT(0);
	}
	else
	{
		if (IsCached(dest) && IsCached(src))
		{
			ASSERT(0);
		}
		else if (IsCached(dest))
		{
			ASSERT(0);
		}
		else if (IsCached(src))
		{
			ASSERT(0);
		}
		else
		{
			if (IR_SIZE(ir) == IR_REV_8_IN_16)
			{
				switch ((IsCached(dest) ? 2 : 0) |
						(IsCached(src)  ? 1 : 0))
				{
				case 0:
					_PushReg_32(EAX);
					MovRegToNReg(EAX, src);
					_XchgRegToReg_8(AH, AL);
					MovNRegToReg(dest, EAX);
					_PopReg_32(EAX);					
					break;
				case 1:
				case 2:
				case 3:
					ASSERT(0);
					break;
				}
			}
			else // IR_REV_8_IN_32
			{
				switch ((IsCached(dest) ? 2 : 0) |
						(IsCached(src)  ? 1 : 0))
				{
				case 0:
					_PushReg_32(EAX);
					MovRegToNReg(EAX, src);
					_BswapReg_32(EAX);
					MovNRegToReg(dest, EAX);
					_PopReg_32(EAX);
					break;
				case 1:
				case 2:
				case 3:
					ASSERT(0);
					break;
				}
			}
		}
	}

	return DRPPC_OKAY;
}

/*
 * INT GenerateExts (IR * ir);
 */

static INT GenerateExts (IR * ir)
{
	UINT	dest = IR_OPERAND_DEST(ir);
	UINT	src  = IR_OPERAND_SRC1(ir);

	if (dest == src)
	{
		if (IsCached(src))
		{
			ASSERT(0);
		}
		else
		{
			switch (IR_SIZE(ir))
			{
			case IR_EXT_8_TO_16:
				ASSERT(0);
				break;
			case IR_EXT_8_TO_32:
				ASSERT(0);
				break;
			case IR_EXT_16_TO_32:
				{
				/*
				 * push		eax
				 * movsx	eax, [src]
				 * mov		[dest], eax
				 * pop		eax
				 */

				void * srcp = GetRegContextPtr(src);
				void * destp = GetRegContextPtr(dest);

				// TODO: use EBP-relative addressing!

				SaveReg_32(EAX);
				_MovsxMemAbsToReg_16To32(EAX, (UINT32)srcp);
				_MovRegToMemAbs_32(EAX, (UINT32)destp);
				RestoreReg_32(EAX);
				}
				break;
			}
		}
	}
	else
	{
		ASSERT(0);
	}

	return DRPPC_OKAY;
}

/*
 * INT GenerateCmp (IR * ir);
 */

static const X86_CC setcc_cc[NUM_IR_CONDITIONS] =
{
	CC_E,	CC_NE,
	CC_B,	CC_A,
	CC_L,	CC_G,
	CC_BE,	CC_AE,
	CC_LE,	CC_GE,
};

static INT GenerateCmp (IR * ir)
{
	UINT	dest = IR_OPERAND_DEST(ir);
	UINT32	src1 = IR_OPERAND_SRC1(ir);
	UINT32	src2 = IR_OPERAND_SRC2(ir);
	X86_CC	cc;

	ASSERT(IR_CONSTANT(ir) < 3);

	// Do the comparison
	if (IR_CONSTANT(ir) == 0)
	{
		// No constant
		CmpRegToReg_32(src1, src2);
	}
	else if (IR_CONSTANT(ir) == 1)
	{
		// Operand 1 is constant
		CmpImmToReg_32(src1, src2);
	}
	else
	{
		// Operand 2 is constant
		CmpRegToImm_32(src1, src2);
	}

	// Pick the right condition code
	cc = setcc_cc[IR_CONDITION(ir)];

	// Do the final condition set op.
	SetccReg_8(cc, dest);

	return DRPPC_OKAY;
}

/*
 * INT GenerateAnd (IR * ir);
 */

static INT GenerateAnd (IR * ir)
{
	GENERATE_ALU_3OP( And );

	return DRPPC_OKAY;
}

/*
 * INT GenerateOr (IR * ir);
 */

static INT GenerateOr (IR * ir)
{
	GENERATE_ALU_3OP( Or );

	return DRPPC_OKAY;
}

/*
 * INT GenerateXor (IR * ir);
 */

static INT GenerateXor (IR * ir)
{
	GENERATE_ALU_3OP( Xor );

	return DRPPC_OKAY;
}

/*
 * INT GenerateShl (IR * ir);
 */

static INT GenerateShl (IR * ir)
{
	GENERATE_SHIFT_ROT( Shl );

	return DRPPC_OKAY;
}

/*
 * INT GenerateShr (IR * ir);
 */

static INT GenerateShr (IR * ir)
{
	GENERATE_SHIFT_ROT( Shr );

	return DRPPC_OKAY;
}

/*
 * INT GenerateRol (IR * ir);
 */

static INT GenerateRol (IR * ir)
{
	GENERATE_SHIFT_ROT( Rol );

	return DRPPC_OKAY;
}

/*
 * INT GenerateRor (IR * ir);
 */

static INT GenerateRor (IR * ir)
{
	GENERATE_SHIFT_ROT( Ror );

	return DRPPC_OKAY;
}

/*
 * INT GenerateLoad (IR * ir);
 */

static INT GenerateLoad (IR * ir)
{
	if (IR_ADDR_CONST(ir))
	{
		/*
		 * Known address
		 */

		UINT32			addr = IR_OPERAND_SRC1(ir) + IR_OPERAND_CONST(ir);
		UINT			dest = IR_OPERAND_DEST(ir);
		DRPPC_REGION	* region;

		/*
		 * Find the source region
		 */

		region = read_region_table[IR_SIZE(ir)](addr);

		ASSERTEX(region, { Print("Load%u from 0x%08X\n", IR_SIZE(ir), addr); });

		if (region->ptr == NULL)
			GenerateLoad_KnownHandler(ir, region, addr);
		else
			GenerateLoad_KnownDirect(ir, region, addr);
	}
	else
	{
		UINT32	(* GenericRead)(UINT32);

		GenericRead = generic_read_table[IR_SIZE(ir)];

		GenerateLoad_Unknown(ir, GenericRead);
	}

	return DRPPC_OKAY;
}

/*
 * INT GenerateStore (IR * ir);
 */

static INT GenerateStore (IR * ir)
{
	if (IR_ADDR_CONST(ir))
	{
		/*
		 * Known address
		 */

		DRPPC_REGION	* region;
		UINT32			addr;

		// Compute the address

		addr = IR_OPERAND_SRC1(ir) + IR_OPERAND_CONST(ir);

		// Recover the corresponding region

		region = write_region_table[IR_SIZE(ir)](addr);

		ASSERTEX(region, { Print("Store%u to 0x%08X\n", IR_SIZE(ir), addr); });

		// Generate the cover

		if (region->ptr == NULL)
			GenerateStore_KnownHandler(ir, region, addr);
		else
			GenerateStore_KnownDirect(ir, region, addr);
	}
	else
	{
		/*
		 * Unknown address
		 */

		void	(* Handler)(UINT32, UINT32);

		// Pick a handler

		Handler = generic_write_table[IR_SIZE(ir)];

		// Generate the cover

		GenerateStore_Unknown(ir, Handler);
	}

	return DRPPC_OKAY;
}

/*
 * INT GenerateLoadPtr (IR * ir);
 */

static INT GenerateLoadPtr (IR * ir)
{
	MovMemToReg_32(IR_OPERAND_DEST(ir), IR_OPERAND_CONST(ir));

	return DRPPC_OKAY;
}

/*
 * INT GenerateStorePtr (IR * ir);
 */

static INT GenerateStorePtr (IR * ir)
{
	if (IR_DATA_CONST(ir))
	{
		_MovImmToMemAbs_32(IR_OPERAND_CONST(ir), IR_OPERAND_SRC1(ir));
	}
	else
		MovRegToMem_32(IR_OPERAND_SRC1(ir), IR_OPERAND_CONST(ir));

	return DRPPC_OKAY;
}

/*
 * INT GenerateBranch (IR * ir);
 */

static INT GenerateBranch (IR * ir)
{
	UINT32	target = IR_OPERAND_SRC2(ir);

	if (IR_TARGET_CONST(ir))
		_MovImmToMemAbs_32((UINT32)context_pc, target);
	else
	{
		PushReg_32(target);
		_PopMemAbs_32((UINT32)context_pc);
	}

	return DRPPC_OKAY;
}

/*
 * INT GenerateBCond (IR * ir);
 */

static INT GenerateBCond (IR * ir)
{
	UINT32	target	= IR_OPERAND_SRC2(ir);
	UINT32	next	= IR_OPERAND_SRC3(ir);
	UINT	flag	= IR_OPERAND_SRC1(ir);
	UINT8	* ptr1, * ptr2;

	// Perform the flag comparison against zero
	CmpRegToImm_32(flag, 0);

	// Move next addr to PC
	if (IR_NEXT_CONST(ir))
		_MovImmToMemAbs_32((UINT32)context_pc, next);
	else
	{
		PushReg_32(next);
		_PopMemAbs_32((UINT32)context_pc);
	}

	// Record the current position to know where to put the jcc later
	ptr1 = GetEmitPtr();
	SetEmitPtr(ptr1 + 2);

	// Move target addr to PC
	if (IR_TARGET_CONST(ir))
	{
		_MovImmToMemAbs_32((UINT32)context_pc, target);
		_PushMemAbs_32((UINT32)context_pc);
		_CallDirect((UINT32)UpdateFetch);
		_AddImmToReg_32(ESP, 4);
	}
	else
	{
		PushReg_32(target);
		_PopMemAbs_32((UINT32)context_pc);
	}

	// Emit the jump at ptr1, then move back the emit pointer to ptr2
	ptr2 = GetEmitPtr();
	SetEmitPtr(ptr1);
	_JccRel8(CC_E, (UINT32)ptr2);
	SetEmitPtr(ptr2);

	return DRPPC_OKAY;
}

/*
 * INT GenerateCallRead (IR * ir);
 */

static INT GenerateCallRead (IR * ir)
{
	UINT32	handler = IR_OPERAND_SRC3(ir);
	UINT	dest = IR_OPERAND_DEST(ir);

	SaveReg_32(EAX);
	_CallDirect(handler);
	MovNRegToReg(dest, EAX);
	RestoreReg_32(EAX);

	return DRPPC_OKAY;
}

/*
 * INT GenerateSync (IR * ir);
 */

static INT GenerateSync (IR * ir)
{
	ASSERT(IR_CONSTANT(ir));

	if (UpdateTimers)	// Not all frontends will need this
	{
		if (IR_OPERAND_CONST(ir) != 0)
		{
			_PushImm_32(IR_OPERAND_CONST(ir));
			_CallDirect((UINT32)UpdateTimers);
			_AddImmToReg_32(ESP, 4);
		}
	}

	// TODO: update context_cycles, too!

	return DRPPC_OKAY;
}

/*
 * INT GenerateConvert (IR * ir);
 */

static INT GenerateConvert (IR * ir)
{
	UINT	dest = IR_OPERAND_DEST(ir);
	UINT	src  = IR_OPERAND_SRC1(ir);
	UINT	size = IR_SIZE(ir);

	switch (CombineIsCached2(dest, src) | (size << 2))
	{
	case (IR_S_TO_D << 2) | 0:
		_FldMemAbs_32((UINT32)GetRegContextPtr(src));	// fld	dword [src]
		_FstpMemAbs_64((UINT32)GetRegContextPtr(dest));	// fstp	qword [dest]
		break;
	case (IR_D_TO_S << 2) | 0:
		_FldMemAbs_64((UINT32)GetRegContextPtr(src));	// fld	qword [src]
		_FstpMemAbs_32((UINT32)GetRegContextPtr(dest));	// fstp	dword [dest]
		break;
	default:
		Print("ERROR: size = %i\n", size);
		ASSERT(0);
	}

	return DRPPC_OKAY;
}

static INT GenerateFSub (IR * ir)
{
	UINT	dest = IR_OPERAND_DEST(ir);
	UINT	src1 = IR_OPERAND_SRC1(ir);
	UINT	src2 = IR_OPERAND_SRC2(ir);

	ASSERT(!IsCached(dest));
	ASSERT(!IsCached(src1));
	ASSERT(!IsCached(src2));
	ASSERT(!IR_CONSTANT(ir));

	_FldMemAbs_64((UINT32)GetRegContextPtr(src2));
	_FldMemAbs_64((UINT32)GetRegContextPtr(src1));
	_Fsubp();
	_FstpMemAbs_64((UINT32)GetRegContextPtr(dest));

	return DRPPC_OKAY;
}

static INT GenerateFMul (IR * ir)
{
	UINT	dest = IR_OPERAND_DEST(ir);
	UINT	src1 = IR_OPERAND_SRC1(ir);
	UINT	src2 = IR_OPERAND_SRC2(ir);

	ASSERT(!IsCached(dest));
	ASSERT(!IsCached(src1));
	ASSERT(!IsCached(src2));
	ASSERT(!IR_CONSTANT(ir));

	_FldMemAbs_64((UINT32)GetRegContextPtr(src2));
	_FldMemAbs_64((UINT32)GetRegContextPtr(src1));
	_Fmulp();
	_FstpMemAbs_64((UINT32)GetRegContextPtr(dest));

	return DRPPC_OKAY;
}

static INT GenerateFDiv (IR * ir)
{
	UINT	dest = IR_OPERAND_DEST(ir);
	UINT	src1 = IR_OPERAND_SRC1(ir);
	UINT	src2 = IR_OPERAND_SRC2(ir);

	ASSERT(!IsCached(dest));
	ASSERT(!IsCached(src1));
	ASSERT(!IsCached(src2));
	ASSERT(!IR_CONSTANT(ir));

	_FldMemAbs_64((UINT32)GetRegContextPtr(src2));
	_FldMemAbs_64((UINT32)GetRegContextPtr(src1));
	_Fdivp();
	_FstpMemAbs_64((UINT32)GetRegContextPtr(dest));

	return DRPPC_OKAY;
}

/*******************************************************************************
 Interface

 This is the only mean of communication between the back end and the rest of
 drppc.
*******************************************************************************/

/*
 * INT Target_Init
 * (
 *		MEMPOOL		* _cache,
 *		UINT32		* _pc,
 *		void		* _context_base,
 *		REG_DESC	* _context_desc,
 *		void		(* _UpdateTimers)(INT),
 *		void		(* _UpdateFetch)(UINT32)
 * );
 *
 * Initializes the whole back end. It must be called once before any of the back
 * end facilities is used.
 *
 * Parameters:
 *
 *		_cache			= pointer to the native code cache. It must be already
 *						  initialized and ready for work.
 *		_pc				= since the source CPU program counter (PC) won't be
 *						  given a DFLOW_BIT id, a pointer must be provided. This
 *						  is the aforementioned pointer.
 *		_context_base	= a pointer to the base address of the source CPU
 *						  context. This may be needed by the back end in case it
 *						  has to cache the context region or something.
 *		_context_desc	= the context descriptor. Each DFLOW_BIT id is an index
 *						  into the context descriptor, where each entry holds
 *						  informations about the corresponding register.
 *		_UpdateTimers	= used by the SYNC intermediate instruction, to update
 *						  the source CPU timers and such.
 *		_UpdateFetch	= called whenever a branch is taken. It has to update
 *						  the current fetch pointer and/or fetch region.
 *
 * Returns:
 *
 *  DRPPC_OKAY if no error happened; otherwise the corresponding error code.
 */

INT Target_Init
(
	MEMPOOL		* _cache,
	UINT32		* _pc,
	void		* _context_base,
	REG_DESC	* _context_desc,
	void		(* _UpdateTimers)(INT),
	INT			(* _UpdateFetch)(UINT32)
)
{
	UINT i;

	/*
	 * Install the code cache handle and all the other stuff.
	 */

	if (!_cache || !_pc || !_context_desc || !_UpdateTimers || !_UpdateFetch)
		return DRPPC_INVALID_CONFIG;

	SetCodeCache(_cache);

	context_pc		= _pc;
	context_base	= _context_base;
	context_desc	= _context_desc;
	UpdateTimers	= _UpdateTimers;
	UpdateFetch		= _UpdateFetch;

	ebp_base_ptr = (UINT32)context_base + 128;

	/*
	 * Setup the jump table
	 */

	for (i = 0; i < NUM_IR_OPS; i++)
		GenerateOp[i] = GenerateUnhandled;

	GenerateOp[IR_LOADI]	= GenerateLoadi;
	GenerateOp[IR_MOVE]		= GenerateMove;
	GenerateOp[IR_ADD]		= GenerateAdd;
	GenerateOp[IR_SUB]		= GenerateSub;
	GenerateOp[IR_NEG]		= GenerateNeg;
	GenerateOp[IR_MULU]		= GenerateMulu;
	GenerateOp[IR_AND]		= GenerateAnd;
	GenerateOp[IR_OR]		= GenerateOr;
	GenerateOp[IR_XOR]		= GenerateXor;
	GenerateOp[IR_NOT]		= GenerateNot;
	GenerateOp[IR_SHL]		= GenerateShl;
	GenerateOp[IR_SHR]		= GenerateShr;
	GenerateOp[IR_ROL]		= GenerateRol;
	GenerateOp[IR_ROR]		= GenerateRor;
	GenerateOp[IR_BREV]		= GenerateBrev;
	GenerateOp[IR_EXTS]		= GenerateExts;
	GenerateOp[IR_CMP]		= GenerateCmp;
	GenerateOp[IR_LOAD]		= GenerateLoad;
	GenerateOp[IR_STORE]	= GenerateStore;
	GenerateOp[IR_CALLREAD]	= GenerateCallRead;
	GenerateOp[IR_LOADPTR]	= GenerateLoadPtr;
	GenerateOp[IR_STOREPTR]	= GenerateStorePtr;
	GenerateOp[IR_BRANCH]	= GenerateBranch;
	GenerateOp[IR_BCOND]	= GenerateBCond;
	GenerateOp[IR_SYNC]		= GenerateSync;
	GenerateOp[IR_CONVERT]	= GenerateConvert;
	GenerateOp[IR_FSUB]		= GenerateFSub;
	GenerateOp[IR_FMUL]		= GenerateFMul;

	return DRPPC_OKAY;
}

/*
 * void Target_Shutdown (void);
 *
 * It shuts down the whole back end.
 */

void Target_Shutdown (void)
{
	/* Do nothing. */
}

/*
 * INT Target_GenerateBB (DRPPC_BB * bb, IR * ir);
 *
 * It generates a native version of the intermediate basic block specified in
 * ir, placing it in the native code cache. bb will be updated with informations
 * about the generated bb once the process is complete.
 *
 * Parameters:
 *
 *		bb	= the basic block info struct.
 *		ir	= the intermediate basic block representation.
 *
 * Returns:
 *
 *	DRPPC_OKAY if no error happened; otherwise, the error's code.
 */

INT Target_GenerateBB (DRPPC_BB * bb, IR * ir)
{
	CHAR	string[10240];
	UINT8	* bb_native_ptr,  * temp;
	UINT32	size;

	/*
	 * Reset the register location tracking array.
	 */

	memset(reg_location, -1, sizeof(reg_location));

	/*
	 * Align the code cache emit pointer to 4 bytes.
	 */

	bb_native_ptr = (UINT8 *)(((UINT32)GetEmitPtr() + 3) & ~3);
	SetEmitPtr(bb_native_ptr);

	/*
	 * Generate the BB body
	 */

	do
	{
#if PRINT_X86_DISASM
		printf("\n// "); IR_PrintOp(ir);
		temp = GetEmitPtr();
#endif

		DRPPC_TRY( GenerateOp[IR_OPERATION(ir)](ir) );

		ir = ir->next;

#if PRINT_X86_DISASM
		size = (UINT32)GetEmitPtr() - (UINT32)temp;
		ASSERT(size != 0);
		Target_Disasm(string, temp, size);
		printf(string);
#endif

	} while (IR_OPERATION(ir) != IR_NOP);

	_Ret();

	/*
	 * Okay, we're done. Update the BB info struct. We cannot do this step
	 * anytime before, unless we want to link a possibly unfinished BB to the
	 * working BB list. And we don't want that to happen, do we?
	 */

	bb->ptr = bb_native_ptr;

#ifdef DRPPC_PROFILE
	bb->native_size = (UINT32)GetEmitPtr() - (UINT32)bb->ptr;
#endif

	return DRPPC_OKAY;
}

/*
 * INT Target_RunBB (DRPPC_BB * bb);
 *
 * Runs the specified native basic block. 
 */

INT Target_RunBB (DRPPC_BB * bb)
{
	void			(* Block)(void);
	static INT		bb_count = 0;
	static UINT64	old_tsc, new_tsc;

	ASSERT(bb);
	ASSERT(bb->ptr);

#if PRINT_X86_BB_EXECUTION
	printf("\n * X86: Executing BB @0x%08X, Count = %d.\n", (UINT32)Block, ++bb_count);
	fflush(stdout);
#endif

	Block = (void (*)(void))bb->ptr;

#ifdef DRPPC_PROFILE
	// Start profiling for BB
	bb->native_exec_time = -1;
	old_tsc = ReadTSC();
#endif

	CallNative(Block);

#ifdef DRPPC_PROFILE
	// End profiling for BB
	new_tsc = ReadTSC();
	ASSERT((new_tsc - old_tsc) < 0x7FFFFFFF);
	bb->native_exec_time = (UINT32)(new_tsc - old_tsc);
#endif

#if PRINT_X86_BB_EXECUTION
	printf(" * X86: Done executing BB @0x%08X.\n\n", (UINT32)Block);
#endif

	return DRPPC_OKAY;
}

/*
 * void Target_Disasm (CHAR * out, UINT8 * ptr, UINT nbytes);
 *
 * Disassembles nbytes bytes of code, starting at ptr, and places the
 * disassembly in out.
 */

void Target_Disasm (CHAR * out, UINT8 * ptr, UINT nbytes)
{
	CHAR	string[128];
	UINT	size;
	UINT8	* temp;

	while ((INT)nbytes > 0)
	{
		string[0] = '\0';

		size = disasm(ptr, string, 32, (long)ptr, 0, 0);

		temp = ptr;
		nbytes -= size;
		ptr += size;

		out += sprintf(out, "0x%08X: %s\n", (UINT32)temp, string);
	}
}
