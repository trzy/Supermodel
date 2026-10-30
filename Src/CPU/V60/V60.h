// license:BSD-3-Clause
// copyright-holders:Farfetch'd, R. Belmont
// Ported to Supermodel from MAME (src/devices/cpu/v60/v60.h).
//
// NEC V60 (uPD70615) CPU core, standalone (no MAME framework).
//
// Port notes (framework-forced changes; the emulation logic in the .hxx files is unchanged):
//
//  - Memory: the program and I/O spaces are IBus objects (24-bit addresses, masked to
//    0xFFFFFF, little-endian, 16-bit data bus). Only Read8/Read16/Write8/Write16 are ever
//    called, and every 16-bit access is aligned. Wider or unaligned accesses are split the
//    way MAME's memory_read_generic/memory_write_generic split them on a 16-bit
//    little-endian bus (lowest address first):
//        word  @ even A : Read16(A)
//        word  @ odd A  : Read8(A), Read8(A+1)
//        dword @ even A : Read16(A), Read16(A+2)
//        dword @ odd A  : Read8(A), Read16(A+1), Read8(A+3)
//        qword          : same pattern, 4 (even) or 1+3+1 (odd) pieces
//    MAME issues the odd-address edge pieces as 16-bit accesses with a one-byte lane mask
//    (0xFF00 / 0x00FF); here they are Read8/Write8 of that byte, which is what a masked
//    access means for an IBus. Writes use the same order. Opcode fetches (MAME's m_cache16
//    path) use the same functions, or read directly from a buffer registered with
//    SetFetchRegion().
//
//  - Stall(): see the comment on the method below.
//
//  - Interrupt lines: MAME's set_input_line() is synchronised through the scheduler: a
//    change made while the CPU is executing (e.g. from one of its own bus handlers) takes
//    effect after the current instruction, at the end of the (aborted) timeslice. Here,
//    SetIRQLine()/SetNMILine() called from outside Run() take effect immediately (same as
//    MAME's deferred delivery before the CPU runs again); when called from inside Run()
//    they are queued, the timeslice is aborted after the current instruction, and the queued
//    changes are applied just before Run() returns.
//
//  - fatalerror(): MAME aborts the emulation. Here the message goes to ErrorLog(), the
//    CPU enters a "fatal halt" state (IsFatalHalted()) in which Run() just burns cycles,
//    until the next Reset().
//
//  - standard_irq_callback() (debugger/acknowledge hook only, unused by Model 1) is not
//    ported. The vector comes from the IRQ acknowledge callback, which corresponds to
//    MAME's irq_cycle_callback() (devcb_read8, default 0). As in MAME, the CPU adds 0x40.
//
//  - INPUT_LINE_HALT is a scheduler feature in MAME; the host simply doesn't call Run()
//    while the CPU is halted.

#ifndef INCLUDED_V60_H
#define INCLUDED_V60_H

#include "Types.h"
#include "CPU/Bus.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class CBlockFile;

class CV60
{
public:
	// Register indices into m_reg[] (MAME's internal numbering, also the disassembler's)
	enum
	{
		REG_R0 = 0, REG_AP = 29, REG_FP = 30, REG_SP = 31,
		REG_PC = 32, REG_PSW = 33,
		REG_ISP = 36, REG_L0SP = 37, REG_L1SP = 38, REG_L2SP = 39, REG_L3SP = 40,
		REG_SBR = 41, REG_TR = 42, REG_SYCW = 43, REG_TKCW = 44, REG_PIR = 45,
		REG_PSW2 = 51, REG_ATBR0 = 52, REG_TRMODE = 60, REG_ADTMR1 = 64,
		NUM_REGS = 68
	};

	CV60();

	// Program space and I/O space. Either may be nullptr (reads return 0xFF.., writes ignored).
	void AttachBus(IBus *mem, IBus *io);

	// Fast opcode fetch path: opcode/operand fetches whose bytes all lie in
	// [base, base+size) read ptr[addr-base] directly (ptr[i] is the byte at address
	// base+i, little-endian order) instead of going through IBus. Data accesses always use
	// IBus. Calling again with the same base replaces that region (e.g. ROM bank switch);
	// size 0 or ptr nullptr removes it. Up to MAX_FETCH_REGIONS regions.
	enum { MAX_FETCH_REGIONS = 4 };
	void SetFetchRegion(UINT32 base, UINT32 size, const UINT8 *ptr);
	void ClearFetchRegions();

	// MAME device_reset(): PSW=0x10000000, PC=0xFFFFFFF0 (the V60 starts executing at
	// 0xFFFFF0 after the 24-bit mask, no vector fetch), SBR=0, SYCW=0x70, TKCW=0xE000,
	// PSW2=0xF002, flags cleared. Other registers and the IRQ/NMI line states are left as
	// they are (as in MAME). Also clears a fatal halt.
	void Reset();

	// MAME execute_run(): runs until the cycle budget is used up (8 cycles per instruction,
	// MAME's "average"), or the timeslice is aborted. Returns cycles actually executed
	// (can exceed 'cycles' by up to 7 because of the fixed 8-cycle charge, exactly as MAME
	// overshoots m_icount).
	int Run(int cycles);

	// MAME v60_device::stall() + end of timeslice.
	// MAME's stall() only sets m_stall_io. The flag is tested ONLY by the IN instructions
	// (INB/INH/INW, I/O space): after the I/O read, if m_stall_io is set it is cleared and
	// the instruction returns length 0, so PC is not advanced and the IN is re-executed from
	// its first byte next time. Rolled back: PC only. NOT rolled back: the 8 cycles already
	// charged, and any side effects of decoding the first operand (autoincrement/
	// autodecrement register updates, memory-indirect reads); the destination operand is not
	// written. A stall raised from a program-space access (e.g. a MOV from a memory-mapped
	// FIFO) does NOT abandon that instruction in MAME: the instruction completes with the
	// value the handler returned, and m_stall_io stays set until the next IN instruction
	// consumes it. This port reproduces that exactly.
	// In addition, Stall() calls AbortTimeslice(), which stands in for the machine-wide
	// sync (timer at attotime::zero) that MAME's generic_fifo issues alongside stall().
	void Stall();

	// End Run() after the current instruction (MAME cpu_device::abort_timeslice()).
	void AbortTimeslice();

	// Interrupt lines. IRQ is level-triggered (taken while asserted and PSW.IE=1, checked
	// before the first instruction of Run() and after every instruction). NMI is
	// edge-triggered (taken on the clear->assert transition, vector 2).
	void SetIRQLine(bool asserted);
	void SetNMILine(bool asserted);
	// Returns the interrupt number; the CPU uses vector (number + 0x40), as MAME's
	// v60_try_irq() does with m_irq_cycle(). For Model 1 this is model1_state::irq_callback.
	void SetIRQAckCallback(std::function<int(void)> cb);

	// Called with PC before each instruction (MAME's debugger_instruction_hook position).
	void SetTraceCallback(std::function<void(UINT32 pc)> cb);

	// Registers
	UINT32 GetPC() const;
	UINT32 GetPPC() const;                 // PC of the last instruction started
	void SetPC(UINT32 pc);
	UINT32 GetReg(unsigned n) const;       // raw m_reg[n], n < NUM_REGS (PSW: use GetPSW)
	void SetReg(unsigned n, UINT32 v);     // raw m_reg[n] (PSW: use SetPSW)
	UINT32 GetPSW();                       // v60ReadPSW() (folds the flags in)
	void SetPSW(UINT32 v);                 // v60WritePSW() (swaps stacks if needed)
	static const char *GetRegName(unsigned n);

	bool IsFatalHalted() const { return m_fatal; }

	// Disassembler (ported v60d). Reads bytes with the callback set by
	// SetDisasmReadCallback(); without one, it uses the fetch regions and falls back to
	// program-bus Read8 (which may have side effects on I/O addresses).
	void SetDisasmReadCallback(std::function<UINT8(UINT32)> cb);
	std::string Disassemble(UINT32 pc, unsigned *length);

	// Save state: m_reg[68], IRQ/NMI line states, PPC, the 4 flags (MAME's save_item list),
	// plus m_stall_io.
	void SaveState(CBlockFile *StateFile, const char *name);
	void LoadState(CBlockFile *StateFile, const char *name);

private:
	// MAME address_space stand-in: only what the core uses.
	class Space
	{
	public:
		IBus *bus = nullptr;
		uint8_t  read_byte(uint32_t a);
		uint16_t read_word_unaligned(uint32_t a);
		uint32_t read_dword(uint32_t a);
		uint32_t read_dword_unaligned(uint32_t a);
		uint64_t read_qword_unaligned(uint32_t a);
		void write_byte(uint32_t a, uint8_t d);
		void write_word_unaligned(uint32_t a, uint16_t d);
		void write_dword_unaligned(uint32_t a, uint32_t d);
		void write_qword_unaligned(uint32_t a, uint64_t d);
		uint64_t read_n(uint32_t a, unsigned nbytes);
		void write_n(uint32_t a, unsigned nbytes, uint64_t d);
	};

	struct FetchRegion
	{
		UINT32 base;
		UINT32 size;
		const UINT8 *ptr;
	};

	typedef uint32_t (CV60::*am_func)();
	typedef uint32_t (CV60::*op6_func)(int reg);

	// opcode fetch (MAME's m_pr8/m_pr16/m_pr32 through m_cache16)
	inline const UINT8 *FetchPtr(uint32_t a, unsigned n) const;
	uint8_t  OpRead8(uint32_t a);
	uint16_t OpRead16(uint32_t a);
	uint32_t OpRead32(uint32_t a);

	[[noreturn]] void FatalError(const char *fmt, ...);
	void execute_run();
	void execute_set_input(int irqline, int state);
	void ApplyPendingInputs();

	static const int INPUT_LINE_IRQ0 = 0;
	static const int INPUT_LINE_NMI = 1;
	static const uint8_t CLEAR_LINE = 0;
	static const uint8_t ASSERT_LINE = 1;

	Space m_programSpace, m_ioSpace;
	Space *m_program;
	Space *m_io;
	FetchRegion m_fetch[MAX_FETCH_REGIONS];
	int m_numFetch;

	std::function<int(void)> m_irq_cycle;
	std::function<void(UINT32)> m_trace;
	std::function<UINT8(UINT32)> m_disasmRead;

	bool m_inRun;
	bool m_fatal;
	int m_cyclesStolen;
	std::vector<std::pair<int, int>> m_pendingInputs;

	// ---- MAME v60_device state ----
	uint32_t            m_start_pc;
	uint32_t            m_reg[68];
	struct {
		uint8_t CY;
		uint8_t OV;
		uint8_t S;
		uint8_t Z;
	}                   m_flags;
	uint8_t             m_irq_line;
	uint8_t             m_nmi_line;
	uint32_t            m_PPC;
	int                 m_icount;
	int                 m_stall_io;

	uint32_t            m_op1, m_op2;
	uint8_t             m_flag1, m_flag2;
	uint8_t             m_instflags;
	uint32_t            m_lenop1, m_lenop2;
	uint8_t             m_subop;
	uint32_t            m_bamoffset1, m_bamoffset2;

	// Output variables for ReadAMAddress(cpustate)
	uint8_t             m_amflag;
	uint32_t            m_amout;
	uint32_t            m_bamoffset;

	// Appo temp var
	uint32_t            m_amlength1, m_amlength2;

	// Global vars used by AM functions
	uint32_t            m_modadd;
	uint8_t             m_modm;
	uint8_t             m_modval;
	uint8_t             m_modval2;
	uint8_t             m_modwritevalb;
	uint16_t            m_modwritevalh;
	uint32_t            m_modwritevalw;
	uint8_t             m_moddim;

	// ---- MAME tables and handlers ----
	// (declarations unchanged, except that MAME's [[noreturn]] on the uint32_t-returning
	// error handlers is dropped: MSVC warns C4646 on noreturn functions with a non-void
	// return type. They still never return: they all end in FatalError().)
	static const am_func s_AMTable1_G7a[16];
	static const am_func s_BAMTable1_G7a[16];
	static const am_func s_AMTable1_G7[32];
	static const am_func s_BAMTable1_G7[32];
	static const am_func s_AMTable1_G6[8];
	static const am_func s_BAMTable1_G6[8];
	static const am_func s_AMTable1[2][8];
	static const am_func s_BAMTable1[2][8];
	static const am_func s_AMTable2_G7a[16];
	static const am_func s_BAMTable2_G7a[16];
	static const am_func s_AMTable2_G7[32];
	static const am_func s_BAMTable2_G7[32];
	static const am_func s_AMTable2_G6[8];
	static const am_func s_BAMTable2_G6[8];
	static const am_func s_AMTable2[2][8];
	static const am_func s_BAMTable2[2][8];
	static const am_func s_AMTable3_G7a[16];
	static const am_func s_AMTable3_G7[32];
	static const am_func s_AMTable3_G6[8];
	static const am_func s_AMTable3[2][8];
	static const am_func s_Op5FTable[32];
	static const am_func s_Op5CTable[32];
	static const op6_func s_OpC6Table[8];
	static const op6_func s_OpC7Table[8];
	static const am_func s_Op59Table[32];
	static const am_func s_Op5BTable[32];
	static const am_func s_Op5DTable[32];
	static const am_func s_Op58Table[32];
	static const am_func s_Op5ATable[32];
	static const am_func s_OpCodeTable[256];

	inline void v60SaveStack();
	inline void v60ReloadStack();
	inline uint32_t v60ReadPSW();
	inline void v60WritePSW(uint32_t newval);
	inline uint32_t v60_update_psw_for_exception(int is_interrupt, int target_level);

	uint32_t am1Register();
	uint32_t am1RegisterIndirect();
	uint32_t bam1RegisterIndirect();
	uint32_t am1RegisterIndirectIndexed();
	uint32_t bam1RegisterIndirectIndexed();
	uint32_t am1Autoincrement();
	uint32_t bam1Autoincrement();
	uint32_t am1Autodecrement();
	uint32_t bam1Autodecrement();
	uint32_t am1Displacement8();
	uint32_t bam1Displacement8();
	uint32_t am1Displacement16();
	uint32_t bam1Displacement16();
	uint32_t am1Displacement32();
	uint32_t bam1Displacement32();
	uint32_t am1DisplacementIndexed8();
	uint32_t bam1DisplacementIndexed8();
	uint32_t am1DisplacementIndexed16();
	uint32_t bam1DisplacementIndexed16();
	uint32_t am1DisplacementIndexed32();
	uint32_t bam1DisplacementIndexed32();
	uint32_t am1PCDisplacement8();
	uint32_t bam1PCDisplacement8();
	uint32_t am1PCDisplacement16();
	uint32_t bam1PCDisplacement16();
	uint32_t am1PCDisplacement32();
	uint32_t bam1PCDisplacement32();
	uint32_t am1PCDisplacementIndexed8();
	uint32_t bam1PCDisplacementIndexed8();
	uint32_t am1PCDisplacementIndexed16();
	uint32_t bam1PCDisplacementIndexed16();
	uint32_t am1PCDisplacementIndexed32();
	uint32_t bam1PCDisplacementIndexed32();
	uint32_t am1DisplacementIndirect8();
	uint32_t bam1DisplacementIndirect8();
	uint32_t am1DisplacementIndirect16();
	uint32_t bam1DisplacementIndirect16();
	uint32_t am1DisplacementIndirect32();
	uint32_t bam1DisplacementIndirect32();
	uint32_t am1DisplacementIndirectIndexed8();
	uint32_t bam1DisplacementIndirectIndexed8();
	uint32_t am1DisplacementIndirectIndexed16();
	uint32_t bam1DisplacementIndirectIndexed16();
	uint32_t am1DisplacementIndirectIndexed32();
	uint32_t bam1DisplacementIndirectIndexed32();
	uint32_t am1PCDisplacementIndirect8();
	uint32_t bam1PCDisplacementIndirect8();
	uint32_t am1PCDisplacementIndirect16();
	uint32_t bam1PCDisplacementIndirect16();
	uint32_t am1PCDisplacementIndirect32();
	uint32_t bam1PCDisplacementIndirect32();
	uint32_t am1PCDisplacementIndirectIndexed8();
	uint32_t bam1PCDisplacementIndirectIndexed8();
	uint32_t am1PCDisplacementIndirectIndexed16();
	uint32_t bam1PCDisplacementIndirectIndexed16();
	uint32_t am1PCDisplacementIndirectIndexed32();
	uint32_t bam1PCDisplacementIndirectIndexed32();
	uint32_t am1DoubleDisplacement8();
	uint32_t bam1DoubleDisplacement8();
	uint32_t am1DoubleDisplacement16();
	uint32_t bam1DoubleDisplacement16();
	uint32_t am1DoubleDisplacement32();
	uint32_t bam1DoubleDisplacement32();
	uint32_t am1PCDoubleDisplacement8();
	uint32_t bam1PCDoubleDisplacement8();
	uint32_t am1PCDoubleDisplacement16();
	uint32_t bam1PCDoubleDisplacement16();
	uint32_t am1PCDoubleDisplacement32();
	uint32_t bam1PCDoubleDisplacement32();
	uint32_t am1DirectAddress();
	uint32_t bam1DirectAddress();
	uint32_t am1DirectAddressIndexed();
	uint32_t bam1DirectAddressIndexed();
	uint32_t am1DirectAddressDeferred();
	uint32_t bam1DirectAddressDeferred();
	uint32_t am1DirectAddressDeferredIndexed();
	uint32_t bam1DirectAddressDeferredIndexed();
	uint32_t am1Immediate();
	uint32_t am1ImmediateQuick();
	uint32_t am1Error1();
	uint32_t bam1Error1();
	uint32_t am1Error2();
	uint32_t bam1Error2();
	[[maybe_unused]] uint32_t am1Error3();
	[[maybe_unused]] uint32_t bam1Error3();
	uint32_t am1Error4();
	uint32_t bam1Error4();
	uint32_t am1Error5();
	uint32_t bam1Error5();
	uint32_t bam1Error6();
	uint32_t am1Group7a();
	uint32_t bam1Group7a();
	uint32_t am1Group6();
	uint32_t bam1Group6();
	uint32_t am1Group7();
	uint32_t bam1Group7();
	uint32_t am2Register();
	uint32_t am2RegisterIndirect();
	uint32_t bam2RegisterIndirect();
	uint32_t am2RegisterIndirectIndexed();
	uint32_t bam2RegisterIndirectIndexed();
	uint32_t am2Autoincrement();
	uint32_t bam2Autoincrement();
	uint32_t am2Autodecrement();
	uint32_t bam2Autodecrement();
	uint32_t am2Displacement8();
	uint32_t bam2Displacement8();
	uint32_t am2Displacement16();
	uint32_t bam2Displacement16();
	uint32_t am2Displacement32();
	uint32_t bam2Displacement32();
	uint32_t am2DisplacementIndexed8();
	uint32_t bam2DisplacementIndexed8();
	uint32_t am2DisplacementIndexed16();
	uint32_t bam2DisplacementIndexed16();
	uint32_t am2DisplacementIndexed32();
	uint32_t bam2DisplacementIndexed32();
	uint32_t am2PCDisplacement8();
	uint32_t bam2PCDisplacement8();
	uint32_t am2PCDisplacement16();
	uint32_t bam2PCDisplacement16();
	uint32_t am2PCDisplacement32();
	uint32_t bam2PCDisplacement32();
	uint32_t am2PCDisplacementIndexed8();
	uint32_t bam2PCDisplacementIndexed8();
	uint32_t am2PCDisplacementIndexed16();
	uint32_t bam2PCDisplacementIndexed16();
	uint32_t am2PCDisplacementIndexed32();
	uint32_t bam2PCDisplacementIndexed32();
	uint32_t am2DisplacementIndirect8();
	uint32_t bam2DisplacementIndirect8();
	uint32_t am2DisplacementIndirect16();
	uint32_t bam2DisplacementIndirect16();
	uint32_t am2DisplacementIndirect32();
	uint32_t bam2DisplacementIndirect32();
	uint32_t am2DisplacementIndirectIndexed8();
	uint32_t bam2DisplacementIndirectIndexed8();
	uint32_t am2DisplacementIndirectIndexed16();
	uint32_t bam2DisplacementIndirectIndexed16();
	uint32_t am2DisplacementIndirectIndexed32();
	uint32_t bam2DisplacementIndirectIndexed32();
	uint32_t am2PCDisplacementIndirect8();
	uint32_t bam2PCDisplacementIndirect8();
	uint32_t am2PCDisplacementIndirect16();
	uint32_t bam2PCDisplacementIndirect16();
	uint32_t am2PCDisplacementIndirect32();
	uint32_t bam2PCDisplacementIndirect32();
	uint32_t am2PCDisplacementIndirectIndexed8();
	uint32_t bam2PCDisplacementIndirectIndexed8();
	uint32_t am2PCDisplacementIndirectIndexed16();
	uint32_t bam2PCDisplacementIndirectIndexed16();
	uint32_t am2PCDisplacementIndirectIndexed32();
	uint32_t bam2PCDisplacementIndirectIndexed32();
	uint32_t am2DoubleDisplacement8();
	uint32_t bam2DoubleDisplacement8();
	uint32_t am2DoubleDisplacement16();
	uint32_t bam2DoubleDisplacement16();
	uint32_t am2DoubleDisplacement32();
	uint32_t bam2DoubleDisplacement32();
	uint32_t am2PCDoubleDisplacement8();
	uint32_t bam2PCDoubleDisplacement8();
	uint32_t am2PCDoubleDisplacement16();
	uint32_t bam2PCDoubleDisplacement16();
	uint32_t am2PCDoubleDisplacement32();
	uint32_t bam2PCDoubleDisplacement32();
	uint32_t am2DirectAddress();
	uint32_t bam2DirectAddress();
	uint32_t am2DirectAddressIndexed();
	uint32_t bam2DirectAddressIndexed();
	uint32_t am2DirectAddressDeferred();
	uint32_t bam2DirectAddressDeferred();
	uint32_t am2DirectAddressDeferredIndexed();
	uint32_t bam2DirectAddressDeferredIndexed();
	uint32_t am2Immediate();
	uint32_t am2ImmediateQuick();
	uint32_t am2Error1();
	uint32_t am2Error2();
	[[maybe_unused]] uint32_t am2Error3();
	uint32_t am2Error4();
	uint32_t am2Error5();
	uint32_t bam2Error1();
	uint32_t bam2Error2();
	[[maybe_unused]] uint32_t bam2Error3();
	uint32_t bam2Error4();
	uint32_t bam2Error5();
	uint32_t bam2Error6();
	uint32_t am2Group7a();
	uint32_t bam2Group7a();
	uint32_t am2Group6();
	uint32_t bam2Group6();
	uint32_t am2Group7();
	uint32_t bam2Group7();
	uint32_t am3Register();
	uint32_t am3RegisterIndirect();
	uint32_t am3RegisterIndirectIndexed();
	uint32_t am3Autoincrement();
	uint32_t am3Autodecrement();
	uint32_t am3Displacement8();
	uint32_t am3Displacement16();
	uint32_t am3Displacement32();
	uint32_t am3DisplacementIndexed8();
	uint32_t am3DisplacementIndexed16();
	uint32_t am3DisplacementIndexed32();
	uint32_t am3PCDisplacement8();
	uint32_t am3PCDisplacement16();
	uint32_t am3PCDisplacement32();
	uint32_t am3PCDisplacementIndexed8();
	uint32_t am3PCDisplacementIndexed16();
	uint32_t am3PCDisplacementIndexed32();
	uint32_t am3DisplacementIndirect8();
	uint32_t am3DisplacementIndirect16();
	uint32_t am3DisplacementIndirect32();
	uint32_t am3DisplacementIndirectIndexed8();
	uint32_t am3DisplacementIndirectIndexed16();
	uint32_t am3DisplacementIndirectIndexed32();
	uint32_t am3PCDisplacementIndirect8();
	uint32_t am3PCDisplacementIndirect16();
	uint32_t am3PCDisplacementIndirect32();
	uint32_t am3PCDisplacementIndirectIndexed8();
	uint32_t am3PCDisplacementIndirectIndexed16();
	uint32_t am3PCDisplacementIndirectIndexed32();
	uint32_t am3DoubleDisplacement8();
	uint32_t am3DoubleDisplacement16();
	uint32_t am3DoubleDisplacement32();
	uint32_t am3PCDoubleDisplacement8();
	uint32_t am3PCDoubleDisplacement16();
	uint32_t am3PCDoubleDisplacement32();
	uint32_t am3DirectAddress();
	uint32_t am3DirectAddressIndexed();
	uint32_t am3DirectAddressDeferred();
	uint32_t am3DirectAddressDeferredIndexed();
	uint32_t am3Immediate();
	uint32_t am3ImmediateQuick();
	uint32_t am3Error1();
	uint32_t am3Error2();
	[[maybe_unused]] uint32_t am3Error3();
	uint32_t am3Error4();
	uint32_t am3Error5();
	uint32_t am3Group7a();
	uint32_t am3Group6();
	uint32_t am3Group7();
	uint32_t ReadAM();
	uint32_t BitReadAM();
	uint32_t ReadAMAddress();
	uint32_t BitReadAMAddress();
	uint32_t WriteAM();
	void F12DecodeFirstOperand(am_func DecodeOp1, uint8_t dim1);
	void F12WriteSecondOperand(uint8_t dim2);
	void F12DecodeOperands(am_func DecodeOp1, uint8_t dim1, am_func DecodeOp2, uint8_t dim2);
	uint32_t opADDB();
	uint32_t opADDH();
	uint32_t opADDW();
	uint32_t opADDCB();
	uint32_t opADDCH();
	uint32_t opADDCW();
	uint32_t opANDB();
	uint32_t opANDH();
	uint32_t opANDW();
	uint32_t opCALL();
	uint32_t opCHKAR();
	uint32_t opCHKAW();
	uint32_t opCHKAE();
	uint32_t opCHLVL();
	uint32_t opCLR1();
	uint32_t opCMPB();
	uint32_t opCMPH();
	uint32_t opCMPW();
	uint32_t opDIVB();
	uint32_t opDIVH();
	uint32_t opDIVW();
	uint32_t opDIVX();
	uint32_t opDIVUX();
	uint32_t opDIVUB();
	uint32_t opDIVUH();
	uint32_t opDIVUW();
	uint32_t opINB();
	uint32_t opINH();
	uint32_t opINW();
	uint32_t opLDPR();
	uint32_t opLDTASK();
	uint32_t opMOVD();
	uint32_t opMOVB();
	uint32_t opMOVH();
	uint32_t opMOVW();
	uint32_t opMOVEAB();
	uint32_t opMOVEAH();
	uint32_t opMOVEAW();
	uint32_t opMOVSBH();
	uint32_t opMOVSBW();
	uint32_t opMOVSHW();
	uint32_t opMOVTHB();
	uint32_t opMOVTWB();
	uint32_t opMOVTWH();
	uint32_t opMOVZBH();
	uint32_t opMOVZBW();
	uint32_t opMOVZHW();
	uint32_t opMULB();
	uint32_t opMULH();
	uint32_t opMULW();
	uint32_t opMULUB();
	uint32_t opMULUH();
	uint32_t opMULUW();
	uint32_t opNEGB();
	uint32_t opNEGH();
	uint32_t opNEGW();
	uint32_t opNOTB();
	uint32_t opNOTH();
	uint32_t opNOTW();
	uint32_t opNOT1();
	uint32_t opORB();
	uint32_t opORH();
	uint32_t opORW();
	uint32_t opOUTB();
	uint32_t opOUTH();
	uint32_t opOUTW();
	uint32_t opREMB();
	uint32_t opREMH();
	uint32_t opREMW();
	uint32_t opREMUB();
	uint32_t opREMUH();
	uint32_t opREMUW();
	uint32_t opROTB();
	uint32_t opROTH();
	uint32_t opROTW();
	uint32_t opROTCB();
	uint32_t opROTCH();
	uint32_t opROTCW();
	uint32_t opRVBIT();
	uint32_t opRVBYT();
	uint32_t opSET1();
	uint32_t opSETF();
	uint32_t opSHAB();
	uint32_t opSHAH();
	uint32_t opSHAW();
	uint32_t opSHLB();
	uint32_t opSHLH();
	uint32_t opSHLW();
	uint32_t opSTPR();
	uint32_t opSUBB();
	uint32_t opSUBH();
	uint32_t opSUBW();
	uint32_t opSUBCB();
	uint32_t opSUBCH();
	uint32_t opSUBCW();
	uint32_t opTEST1();
	uint32_t opUPDPSWW();
	uint32_t opUPDPSWH();
	uint32_t opXCHB();
	uint32_t opXCHH();
	uint32_t opXCHW();
	uint32_t opXORB();
	uint32_t opXORH();
	uint32_t opXORW();
	uint32_t opMULX();
	uint32_t opMULUX();
	void F2DecodeFirstOperand(am_func DecodeOp1, uint8_t dim1);
	void F2DecodeSecondOperand(am_func DecodeOp2, uint8_t dim2);
	void F2WriteSecondOperand(uint8_t dim2);
	uint32_t opCVTWS();
	uint32_t opCVTSW();
	uint32_t opMOVFS();
	uint32_t opNEGFS();
	uint32_t opABSFS();
	uint32_t opADDFS();
	uint32_t opSUBFS();
	uint32_t opMULFS();
	uint32_t opDIVFS();
	uint32_t opSCLFS();
	uint32_t opCMPF();
	uint32_t op5FUNHANDLED();
	uint32_t op5CUNHANDLED();
	uint32_t op5F();
	uint32_t op5C();
	uint32_t opINCB();
	uint32_t opINCH();
	uint32_t opINCW();
	uint32_t opDECB();
	uint32_t opDECH();
	uint32_t opDECW();
	uint32_t opJMP();
	uint32_t opJSR();
	uint32_t opPREPARE();
	uint32_t opRET();
	uint32_t opTRAP();
	uint32_t opRETIU();
	uint32_t opRETIS();
	uint32_t opSTTASK();
	uint32_t opGETPSW();
	uint32_t opTASI();
	uint32_t opCLRTLB();
	uint32_t opPOPM();
	uint32_t opPUSHM();
	uint32_t opTESTB();
	uint32_t opTESTH();
	uint32_t opTESTW();
	uint32_t opPUSH();
	uint32_t opPOP();
	uint32_t opINCB_0();
	uint32_t opINCB_1();
	uint32_t opINCH_0();
	uint32_t opINCH_1();
	uint32_t opINCW_0();
	uint32_t opINCW_1();
	uint32_t opDECB_0();
	uint32_t opDECB_1();
	uint32_t opDECH_0();
	uint32_t opDECH_1();
	uint32_t opDECW_0();
	uint32_t opDECW_1();
	uint32_t opJMP_0();
	uint32_t opJMP_1();
	uint32_t opJSR_0();
	uint32_t opJSR_1();
	uint32_t opPREPARE_0();
	uint32_t opPREPARE_1();
	uint32_t opRET_0();
	uint32_t opRET_1();
	uint32_t opTRAP_0();
	uint32_t opTRAP_1();
	uint32_t opRETIU_0();
	uint32_t opRETIU_1();
	uint32_t opRETIS_0();
	uint32_t opRETIS_1();
	uint32_t opGETPSW_0();
	uint32_t opGETPSW_1();
	uint32_t opTASI_0();
	uint32_t opTASI_1();
	uint32_t opCLRTLB_0();
	uint32_t opCLRTLB_1();
	uint32_t opPOPM_0();
	uint32_t opPOPM_1();
	uint32_t opPUSHM_0();
	uint32_t opPUSHM_1();
	uint32_t opTESTB_0();
	uint32_t opTESTB_1();
	uint32_t opTESTH_0();
	uint32_t opTESTH_1();
	uint32_t opTESTW_0();
	uint32_t opTESTW_1();
	uint32_t opPUSH_0();
	uint32_t opPUSH_1();
	uint32_t opPOP_0();
	uint32_t opPOP_1();
	uint32_t opSTTASK_0();
	uint32_t opSTTASK_1();
	uint32_t opBGT8();
	uint32_t opBGT16();
	uint32_t opBGE8();
	uint32_t opBGE16();
	uint32_t opBLT8();
	uint32_t opBLT16();
	uint32_t opBLE8();
	uint32_t opBLE16();
	uint32_t opBH8();
	uint32_t opBH16();
	uint32_t opBNH8();
	uint32_t opBNH16();
	uint32_t opBNL8();
	uint32_t opBNL16();
	uint32_t opBL8();
	uint32_t opBL16();
	uint32_t opBNE8();
	uint32_t opBNE16();
	uint32_t opBE8();
	uint32_t opBE16();
	uint32_t opBNV8();
	uint32_t opBNV16();
	uint32_t opBV8();
	uint32_t opBV16();
	uint32_t opBP8();
	uint32_t opBP16();
	uint32_t opBN8();
	uint32_t opBN16();
	uint32_t opBR8();
	uint32_t opBR16();
	uint32_t opBSR();
	uint32_t opBRK();
	uint32_t opBRKV();
	uint32_t opCLRTLBA();
	uint32_t opDISPOSE();
	uint32_t opHALT();
	uint32_t opNOP();
	uint32_t opRSR();
	uint32_t opTRAPFL();
	uint32_t opTB(int reg);
	uint32_t opDBGT(int reg);
	uint32_t opDBLE(int reg);
	uint32_t opDBGE(int reg);
	uint32_t opDBLT(int reg);
	uint32_t opDBH(int reg);
	uint32_t opDBNH(int reg);
	uint32_t opDBL(int reg);
	uint32_t opDBNL(int reg);
	uint32_t opDBE(int reg);
	uint32_t opDBNE(int reg);
	uint32_t opDBV(int reg);
	uint32_t opDBNV(int reg);
	uint32_t opDBN(int reg);
	uint32_t opDBP(int reg);
	uint32_t opDBR(int reg);
	uint32_t opC6();
	uint32_t opC7();
	void F7aDecodeOperands(am_func DecodeOp1, uint8_t dim1, am_func DecodeOp2, uint8_t dim2);
	void F7bDecodeFirstOperand(am_func DecodeOp1, uint8_t dim1);
	void F7bWriteSecondOperand(uint8_t dim2);
	void F7bDecodeOperands(am_func DecodeOp1, uint8_t dim1, am_func DecodeOp2, uint8_t dim2);
	void F7cDecodeOperands(am_func DecodeOp1, uint8_t dim1, am_func DecodeOp2, uint8_t dim2);
	uint32_t opCMPSTRB(uint8_t bFill, uint8_t bStop);
	uint32_t opCMPSTRH(uint8_t bFill, uint8_t bStop);
	uint32_t opMOVSTRUB(uint8_t bFill, uint8_t bStop);
	uint32_t opMOVSTRDB(uint8_t bFill, uint8_t bStop);
	uint32_t opMOVSTRUH(uint8_t bFill, uint8_t bStop);
	uint32_t opMOVSTRDH(uint8_t bFill, uint8_t bStop);
	uint32_t opSEARCHUB(uint8_t bSearch);
	uint32_t opSEARCHUH(uint8_t bSearch);
	uint32_t opSEARCHDB(uint8_t bSearch);
	uint32_t opSEARCHDH(uint8_t bSearch);
	uint32_t opSCHCUB();
	uint32_t opSCHCUH();
	uint32_t opSCHCDB();
	uint32_t opSCHCDH();
	uint32_t opSKPCUB();
	uint32_t opSKPCUH();
	uint32_t opSKPCDB();
	uint32_t opSKPCDH();
	uint32_t opCMPCB();
	uint32_t opCMPCH();
	uint32_t opCMPCFB();
	uint32_t opCMPCFH();
	uint32_t opCMPCSB();
	uint32_t opCMPCSH();
	uint32_t opMOVCUB();
	uint32_t opMOVCUH();
	uint32_t opMOVCFUB();
	uint32_t opMOVCFUH();
	uint32_t opMOVCSUB();
	uint32_t opMOVCSUH();
	uint32_t opMOVCDB();
	uint32_t opMOVCDH();
	uint32_t opMOVCFDB();
	uint32_t opMOVCFDH();
	uint32_t opEXTBFZ();
	uint32_t opEXTBFS();
	uint32_t opEXTBFL();
	uint32_t opSCHBS(uint32_t bSearch1);
	uint32_t opSCH0BSU();
	uint32_t opSCH1BSU();
	uint32_t opINSBFR();
	uint32_t opINSBFL();
	uint32_t opMOVBSD();
	uint32_t opMOVBSU();
	uint32_t opADDDC();
	uint32_t opSUBDC();
	uint32_t opSUBRDC();
	uint32_t opCVTDPZ();
	uint32_t opCVTDZP();
	uint32_t op58UNHANDLED();
	uint32_t op5AUNHANDLED();
	uint32_t op5BUNHANDLED();
	uint32_t op5DUNHANDLED();
	uint32_t op59UNHANDLED();
	uint32_t op58();
	uint32_t op5A();
	uint32_t op5B();
	uint32_t op5D();
	uint32_t op59();
	uint32_t opUNHANDLED();
	void v60_do_irq(int vector);
	void v60_try_irq();
};

#endif	// INCLUDED_V60_H
