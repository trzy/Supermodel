/**
 ** Supermodel
 ** A Sega Model Series Arcade Emulator.
 **
 ** This file is part of Supermodel.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **
 ** Supermodel is distributed in the hope that it will be useful, but WITHOUT
 ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 ** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 ** more details.
 **
 ** You should have received a copy of the GNU General Public License along
 ** with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
 **/

/*
 * MB86235.h
 *
 * Fujitsu MB86235 "TGPx4", ported from MAME's interpreter (Angelo Salese,
 * ElSemi, Matthew Daniels; src/devices/cpu/mb86235, MAME 0.289). The
 * geometry coprocessor of Sega Model 2C. The instruction code is MAME's, with
 * Supermodel glue around it: a bus interface instead of MAME address spaces
 * and FIFO devices, Run() instead of the scheduler, save states. MAME's
 * recompiler (mb86235drc/mb86235fe) is not ported.
 *
 * Based on original work by Angelo Salese, ElSemi, Ville Linde and Matthew
 * Daniels in MAME. Portions Copyright (c) Angelo Salese, ElSemi, Ville Linde
 * and Matthew Daniels, used under the BSD 3-Clause License: see
 * Docs/README.txt.
 *
 * Memory: the program space (4K x 64-bit words, MAME's AS_PROGRAM; on Model
 * 2C a RAM the i960 fills through the copro FIFO port while COPROCTL bit 31
 * is set) and the internal A and B bus RAMs (1K x 32-bit each) live in the
 * core and are saved with it. The external data space (MAME's AS_DATA, 24-bit
 * word addresses: on Model 2C buffer RAM at 0x400000 and the copro data ROM at
 * 0x800000) goes to the bus.
 *
 * Timing: MAME's cycle unit is one instruction (the clock isn't divided;
 * Model 2C clocks the TGPx4 at 20 MHz). Most instructions cost 1 cycle,
 * branches and calls 1-2 more. Run(n) may overrun by a couple of cycles and
 * returns the actual amount used.
 *
 * FIFO stalls (MAME's stall() / check_previous_op_stall()): reading the input
 * FIFO (FI) when it is empty, the bus calls Stall() from inside FIFOInPop()
 * (MAME's on_fifo_empty_pre_sync). As in MAME the instruction completes in a
 * degraded way (the ALU/multiplier and the address register updates are
 * skipped, the value read is garbage) and is executed again, without
 * advancing the PC, as the next instruction. Its cycle is charged, like MAME
 * where the FIFO's sync timer then aborts the timeslice; here Run() returns
 * after the stalled instruction. The next Run() retries it. Whether the core
 * is then halted until data arrives (MAME's on_fifo_empty_post_sync /
 * on_fifo_unempty: HALT line) is up to the board (SetHalt()).
 *
 * FIFO out: FIFOOutPush() must accept the word even when the FIFO is full
 * (MAME's generic FIFO keeps the extra values and halts the writer after a
 * sync: on_fifo_full_post_sync). The board can call EndSlice() from inside
 * the push (MAME's sync aborts the timeslice) and SetHalt(true).
 */

#ifndef INCLUDED_MB86235_H
#define INCLUDED_MB86235_H

#include "Types.h"
#include <cstdint>
#include <functional>
#include <string>
#include <utility>

class CBlockFile;

/*
 * External bus and FIFOs. All addresses are word addresses.
 */
class IMB86235Bus
{
public:
  virtual ~IMB86235Bus() {}

  // External data space (MAME's AS_DATA, 24-bit addresses, 32-bit words):
  // A bus addresses 0x400-0x3fff (+ EB), and the E(EB+EO) transfers
  virtual UINT32 ReadData(UINT32 addr) = 0;
  virtual void   WriteData(UINT32 addr, UINT32 data) = 0;

  // Input FIFO (MAME's fifoin, generic_fifo_u32_device). FIFOInPop() on an
  // empty FIFO must call CMB86235::Stall() and return anything (MAME's
  // generic FIFO returns 0).
  virtual UINT32 FIFOInPop() = 0;
  virtual bool   FIFOInEmpty() = 0;               // IFE condition
  virtual bool   FIFOInFull() = 0;                // IFF condition
  virtual void   FIFOInClear() = 0;               // CLRFI

  // Output FIFO (MAME's fifoout). FO0 and FO1 both push here (FO1 would set
  // a 33rd bit that Model 2 doesn't use).
  virtual void   FIFOOutPush(UINT32 data) = 0;
  virtual bool   FIFOOutEmpty() = 0;              // OFE condition
  virtual bool   FIFOOutFull() = 0;               // OFF condition
  virtual void   FIFOOutClear() = 0;              // CLRFO
};

class CMB86235
{
public:
  // Registers for GetReg()/SetReg() (MAME's state entries, plus the PC stack
  // and the PR registers)
  enum
  {
    REG_PC = 0,
    REG_AA0 = 1,
    REG_AB0 = REG_AA0 + 8,
    REG_MA0 = REG_AB0 + 8,
    REG_MB0 = REG_MA0 + 8,
    REG_AR0 = REG_MB0 + 8,
    REG_MOD = REG_AR0 + 8, REG_EB, REG_EO, REG_SP, REG_PDR, REG_DDR, REG_RPC, REG_LPC,
    REG_PRP, REG_PWP, REG_ST, REG_PPC, REG_PCP,
    REG_PCS0,
    REG_PR0 = REG_PCS0 + 4,
    NUM_REGS = REG_PR0 + 24
  };

  static constexpr unsigned PROGRAM_WORDS = 0x1000;   // 12-bit program space
  static constexpr unsigned RAM_WORDS = 0x400;        // each internal bus RAM

  CMB86235();
  CMB86235(const CMB86235 &) = delete;
  CMB86235 &operator=(const CMB86235 &) = delete;

  void AttachBus(IMB86235Bus *bus);

  // MAME device_reset: PC = 0 (registers and memories are kept)
  void Reset();

  int  Run(int cycles);             // returns the cycles used
  int  CyclesRun() const;           // so far in the current Run()

  // Called by the bus from inside FIFOInPop() (or ReadData()) when the read
  // can't complete: MAME's stall(). See the top of this file. Ignored outside
  // instruction execution.
  void Stall();
  bool IsStalled() const;           // the last instruction stalled and will be retried

  // Ends the current Run() after the current instruction (MAME's
  // abort_timeslice(), e.g. a push into a full FIFO). Ignored outside Run().
  void EndSlice();

  // INPUT_LINE_HALT: while halted, Run() executes nothing and returns all the
  // cycles (like a halted MAME CPU). Halting from inside an instruction (a bus
  // callback) ends the current Run() after it.
  void SetHalt(bool halted);
  bool IsHalted() const { return m_halted; }
  bool IsFatalHalted() const { return m_fatal; }   // stopped by an illegal instruction (MAME's fatalerror)

  // Program RAM (MAME's AS_PROGRAM, 64-bit words). Model 2C writes it 32 bits
  // at a time (model2c_state::copro_fifo_w).
  uint64_t *GetProgramRAM() { return m_program; }
  void    WriteProgram(UINT32 addr, UINT64 data) { m_program[addr & (PROGRAM_WORDS - 1)] = data; }
  UINT64  ReadProgram(UINT32 addr) const         { return m_program[addr & (PROGRAM_WORDS - 1)]; }

  // Internal A/B bus RAM (debugger)
  UINT32 ReadRAMA(UINT32 addr) const { return m_dataa.mem[addr & (RAM_WORDS - 1)]; }
  UINT32 ReadRAMB(UINT32 addr) const { return m_datab.mem[addr & (RAM_WORDS - 1)]; }

  void SetTraceCallback(std::function<void(UINT32 pc)> cb) { m_trace = std::move(cb); }

  UINT32 GetPC() const;
  UINT32 GetReg(unsigned n) const;
  void   SetReg(unsigned n, UINT32 v);
  static const char *GetRegName(unsigned n);

  std::string Disassemble(UINT32 pc) const;           // reads the program RAM
  static std::string Disassemble(UINT32 pc, UINT64 opcode);

  void SaveState(CBlockFile *StateFile, const char *name);
  void LoadState(CBlockFile *StateFile, const char *name);

private:
  struct fifo_state
  {
    uint32_t pc;
    bool has_stalled;
  };

  // MAME's mb86235_internal_state without the recompiler's fields
  struct mb86235_internal_state
  {
    uint32_t pc;
    uint32_t delay_pc;
    uint32_t ppc;
    uint32_t aa[8];
    uint32_t ab[8];
    uint32_t ma[8];
    uint32_t mb[8];
    uint32_t ar[8];

    uint32_t sp;
    uint32_t eb;
    uint32_t eo;
    uint32_t rpc;
    uint32_t lpc;

    uint32_t prp;
    uint32_t pwp;
    uint32_t pr[24];

    uint32_t mod;
    uint32_t st;

    int icount;

    uint32_t pcp;       /**< PC stack pointer */
    uint32_t pcs[4];    /**< PC stack contents */

    uint32_t pdr;
    uint32_t ddr;

    bool delay_slot;

    fifo_state cur_fifo_state;
  };

  // Stand-ins for MAME's address spaces and FIFO devices used by the core
  struct InternalRAM
  {
    uint32_t mem[RAM_WORDS];
    uint32_t read_dword(uint32_t a) const   { return mem[a & (RAM_WORDS - 1)]; }
    void write_dword(uint32_t a, uint32_t d) { mem[a & (RAM_WORDS - 1)] = d; }
  };

  struct External
  {
    CMB86235 *cpu;
    uint32_t read_dword(uint32_t a);
    void write_dword(uint32_t a, uint32_t d);
  };

  struct FIFOIn
  {
    CMB86235 *cpu;
    FIFOIn *operator->() { return this; }
    explicit operator bool() const { return true; }
    uint32_t pop();
    bool is_empty();
    bool is_full();
    void clear();
  };

  struct FIFOOut
  {
    CMB86235 *cpu;
    FIFOOut *operator->() { return this; }
    explicit operator bool() const { return true; }
    void push(uint32_t data);
    bool is_empty();
    bool is_full();
    void clear();
  };

  mb86235_internal_state m_core_state;
  mb86235_internal_state *const m_core;

  uint64_t m_program[PROGRAM_WORDS];
  InternalRAM m_dataa;
  InternalRAM m_datab;
  External m_external;
  FIFOIn m_fifoin;
  FIFOOut m_fifoout;

  // Supermodel
  IMB86235Bus *m_bus;
  std::function<void(UINT32)> m_trace;
  bool m_halted;
  bool m_fatal;
  bool m_inRun;
  bool m_executing;                 // inside execute_op()
  bool m_endSlice;
  int  m_cyclesRequested;

  void execute_run();

  // MAME's interpreter (mb86235ops.cpp)
  bool check_previous_op_stall();
  void execute_op(uint64_t op);
  void do_alu1(uint64_t op);
  void do_alu2(uint64_t op);
  void do_alu2_trans2_1(uint64_t op);
  void do_alu2_trans1_1(uint64_t op);
  void do_alu1_trans2_2(uint64_t op);
  void do_alu1_trans1_2(uint64_t op);
  void do_trans1_3(uint64_t op);
  void do_alu_control(uint64_t op);
  uint32_t get_prx(uint8_t which);
  uint32_t get_constfloat(uint8_t which);
  uint32_t get_constint(uint8_t which);
  uint32_t get_alureg(uint8_t which, bool isfloatop);
  uint32_t get_mulreg(uint8_t which, bool isfloatop);
  void set_alureg(uint8_t which, uint32_t value);
  void decode_aluop(uint8_t opcode, uint32_t src1, uint32_t src2, uint8_t imm, uint8_t dst_which);
  void decode_mulop(bool isfmul, uint32_t src1, uint32_t src2, uint8_t dst_which);
  bool decode_branch_jump(uint8_t which);
  uint32_t do_control_dst(uint64_t op);
  void push_pc(uint32_t pcval);
  uint32_t pop_pc();
  void set_mod(uint16_t mod1, uint16_t mod2);
  uint32_t get_transfer_reg(uint8_t which);
  void set_transfer_reg(uint8_t which, uint32_t value);
  uint32_t decode_ea(uint8_t mode, uint8_t rx, uint8_t ry, uint16_t disp, bool isbbus);
  uint32_t read_abus(uint32_t addr);
  uint32_t read_bbus(uint32_t addr);
  void write_abus(uint32_t addr, uint32_t data);
  void write_bbus(uint32_t addr, uint32_t data);
  void increment_pwp();
  void increment_prp();
  void decrement_prp();
  void zero_prp();
  void set_alu_flagsd(uint32_t val);
  void set_alu_flagsf(float val);
  void set_alu_flagsi(int val);
  bool get_alu_second_src(uint8_t which);
  void handle_single_step_execution();
};

#endif  // INCLUDED_MB86235_H
