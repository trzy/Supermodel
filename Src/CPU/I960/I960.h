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
 * I960.h
 *
 * Intel 80960KA/KB, ported from MAME's i960 core (Farfetch'd, R. Belmont;
 * src/devices/cpu/i960). The main CPU of Sega Model 2. The instruction code
 * is MAME's, with Supermodel glue around it: a bus interface instead of MAME
 * address spaces, Run() instead of the scheduler, save states.
 *
 * Based on original work by Farfetch'd and R. Belmont in MAME. Portions
 * Copyright (c) Farfetch'd and R. Belmont, used under the BSD 3-Clause
 * License: see Docs/README.txt.
 */

#ifndef INCLUDED_I960_H
#define INCLUDED_I960_H

#include "Types.h"
#include "CPU/Bus.h"
#include <cstdint>
#include <functional>
#include <string>
#include <utility>

class CBlockFile;

/*
 * The i960's bus. Little endian; Read16/Write16 get even addresses and
 * Read32/Write32 multiples of 4 (the core splits unaligned accesses into
 * bytes, like MAME).
 *
 * Burst(): the multi-word loads and stores (ldl/ldt/ldq, stl/stt/stq) step
 * through memory only where this returns true. Elsewhere (MAME's regions
 * without the BURST flag, e.g. Model 2's FIFO ports) every word goes to the
 * same address.
 */
class II960Bus : public IBus
{
public:
  virtual bool Burst(UINT32 addr) { return true; }
};

class CI960
{
public:
  enum
  {
    REG_PFP = 0, REG_SP = 1, REG_RIP = 2, REG_FP = 31,   // r0-r15 = 0-15, g0-g15 = 16-31
    REG_SAT = 32, REG_PRCB, REG_PC, REG_AC, REG_IP, REG_PIP,
    NUM_REGS
  };

  CI960();

  void AttachBus(II960Bus *bus);

  // Opcode fetches from [base, base + size) read ptr directly (must stay
  // valid, little endian); the rest go through the bus
  enum { MAX_FETCH_REGIONS = 4 };
  void SetFetchRegion(UINT32 base, UINT32 size, const UINT8 *ptr);
  void ClearFetchRegions();

  void Reset();                     // MAME device_reset (reads the boot record from the bus)
  int  Run(int cycles);             // returns the cycles used
  int  CyclesRun() const { return m_inRun ? m_cyclesRequested - m_icount : 0; }   // so far in the current Run()

  // Called by the bus during a read or write that can't complete (e.g. an
  // empty FIFO): the instruction is retried on the next Run(), the rest of
  // this slice is given up
  void Stall();

  // Called by the bus during a data read or write: the access takes this many
  // more cycles (the board's wait states). Opcode fetches aren't charged.
  void AddWaitCycles(int cycles) { if (m_inRun && !m_fetching) m_icount -= cycles; }

  // The four external interrupt lines (MAME's execute_set_input, "normal"
  // interrupt mode: the vectors come from the interrupt control register)
  void SetIRQLine(int line, bool asserted);

  void SetTraceCallback(std::function<void(UINT32 ip)> cb) { m_trace = std::move(cb); }

  // Calls fn once this many cycles have run after the current instruction (a
  // device that reacts a little after a write: Model 2's sound interrupt check).
  // FlushDelayed() calls a pending one now (before a save state).
  void CallAfterCycles(int cycles, std::function<void()> fn) { m_delayFn = std::move(fn); m_delayRemaining = cycles; m_delayArmed = true; }
  void FlushDelayed() { if (m_delayFn) { auto fn = std::move(m_delayFn); m_delayFn = nullptr; fn(); } }

  UINT32 GetIP() const  { return m_IP; }
  UINT32 GetPIP() const { return m_PIP; }
  UINT32 GetReg(unsigned n) const;
  void   SetReg(unsigned n, UINT32 v);
  static const char *GetRegName(unsigned n);
  bool   IsFatalHalted() const { return m_fatal; }

  std::string Disassemble(UINT32 pc, unsigned *length);  // reads through the bus

  void SaveState(CBlockFile *StateFile, const char *name);
  void LoadState(CBlockFile *StateFile, const char *name);

private:
  static constexpr uint16_t BURST = 0x0001;
  enum { I960_RCACHE_SIZE = 4 };
  enum { I960_PFP = 0, I960_SP = 1, I960_RIP = 2, I960_FP = 31 };
  enum { CLEAR_LINE = 0, ASSERT_LINE = 1 };

  // Stand-in for MAME's address space accessors used by the core
  class Program
  {
  public:
    II960Bus *bus = nullptr;
    uint8_t  read_byte(uint32_t a)  { return bus->Read8(a); }
    uint16_t read_word(uint32_t a)  { return bus->Read16(a); }
    uint32_t read_dword(uint32_t a) { return bus->Read32(a); }
    std::pair<uint8_t, uint16_t>  read_byte_flags(uint32_t a)  { uint16_t f = flags(a); return { bus->Read8(a), f }; }
    std::pair<uint32_t, uint16_t> read_dword_flags(uint32_t a) { uint16_t f = flags(a); return { bus->Read32(a), f }; }
    void write_byte(uint32_t a, uint8_t d)   { bus->Write8(a, d); }
    void write_word(uint32_t a, uint16_t d)  { bus->Write16(a, d); }
    void write_dword(uint32_t a, uint32_t d) { bus->Write32(a, d); }
    uint16_t write_byte_flags(uint32_t a, uint8_t d)   { uint16_t f = flags(a); bus->Write8(a, d); return f; }
    uint16_t write_dword_flags(uint32_t a, uint32_t d) { uint16_t f = flags(a); bus->Write32(a, d); return f; }
  private:
    uint16_t flags(uint32_t a) { return bus->Burst(a) ? BURST : 0; }
  };

  // Stand-in for MAME's opcode cache: fetch regions, else the bus
  class Cache
  {
  public:
    CI960 *cpu = nullptr;
    uint32_t read_dword(uint32_t a);
  };

  struct FetchRegion
  {
    UINT32 base;
    UINT32 size;
    const UINT8 *ptr;
  };

  [[noreturn]] void FatalError(const char *fmt, ...);
  void i960_stall();
  void execute_run();
  void execute_set_input(int irqline, int state);

  // MAME's core
  void burst_stall_save(uint32_t t1, uint32_t t2, int index, int size, bool iswriteop);

  struct {
    uint32_t t1 = 0, t2 = 0;
    int index = 0, size = 0;
    bool burst_mode = false;
    bool iswriteop = false;
  } m_stall_state;
  bool m_stalled;

  uint32_t m_r[0x20];
  uint32_t m_rcache[I960_RCACHE_SIZE][0x10];
  uint32_t m_rcache_frame_addr[I960_RCACHE_SIZE];
  // rcache_pos = how deep in the stack we are.  0-(I960_RCACHE_SIZE-1) means in-cache.
  // I960_RCACHE_SIZE or greater means out of cache, must save to memory.
  int32_t m_rcache_pos;

  double m_fp[4];

  uint32_t m_SAT;
  uint32_t m_PRCB;
  uint32_t m_PC;
  uint32_t m_AC;
  uint32_t m_IP;
  uint32_t m_PIP;
  uint32_t m_ICR;

  int m_immediate_irq;
  int m_immediate_vector;
  int m_immediate_pri;

  int8_t m_irq_line_state[4];   // CLEAR_LINE, ASSERT_LINE or IRQ_HELD (asserted, not taken yet)
  static constexpr int8_t IRQ_HELD = 2;
  bool IRQHeld() const
  {
    return m_irq_line_state[0] == IRQ_HELD || m_irq_line_state[1] == IRQ_HELD || m_irq_line_state[2] == IRQ_HELD || m_irq_line_state[3] == IRQ_HELD;
  }

  Cache m_cache;
  Program m_program;

  int m_icount;

  uint32_t i960_read_dword_unaligned(uint32_t address);
  std::pair<uint32_t, uint16_t> i960_read_dword_unaligned_flags(uint32_t address);
  uint16_t i960_read_word_unaligned(uint32_t address);
  void i960_write_dword_unaligned(uint32_t address, uint32_t data);
  uint16_t i960_write_dword_unaligned_flags(uint32_t address, uint32_t data);
  void i960_write_word_unaligned(uint32_t address, uint16_t data);
  void send_iac(uint32_t adr);
  uint32_t get_ea(uint32_t opcode);
  uint32_t get_1_ri(uint32_t opcode);
  uint32_t get_2_ri(uint32_t opcode);
  uint64_t get_2_ri64(uint32_t opcode);
  void set_ri(uint32_t opcode, uint32_t val);
  void set_ri2(uint32_t opcode, uint32_t val, uint32_t val2);
  void set_ri64(uint32_t opcode, uint64_t val);
  double get_1_rif(uint32_t opcode);
  double get_2_rif(uint32_t opcode);
  void set_rif(uint32_t opcode, double val);
  double get_1_rifl(uint32_t opcode);
  double get_2_rifl(uint32_t opcode);
  void set_rifl(uint32_t opcode, double val);
  uint32_t get_1_ci(uint32_t opcode);
  uint32_t get_2_ci(uint32_t opcode);
  uint32_t get_disp(uint32_t opcode);
  uint32_t get_disp_s(uint32_t opcode);
  void cmp_s(int32_t v1, int32_t v2);
  void cmp_u(uint32_t v1, uint32_t v2);
  void concmp_s(int32_t v1, int32_t v2);
  void concmp_u(uint32_t v1, uint32_t v2);
  void cmp_d(double v1, double v2);
  void bxx(uint32_t opcode, int mask);
  void bxx_s(uint32_t opcode, int mask);
  void fxx(uint32_t opcode, int mask);
  void test(uint32_t opcode, int mask);
  double round_to_int(double val);
  void execute_op(uint32_t opcode);
  void execute_burst_stall_op(uint32_t opcode);
  void take_interrupt(int vector, int lvl);
  void check_immediate_irqs();
  void check_pending_irqs();
  void do_call(uint32_t adr, int type, uint32_t stack);
  void do_ret_0();
  void do_ret();

  // Supermodel
  FetchRegion m_fetch[MAX_FETCH_REGIONS];
  int m_numFetch;
  bool m_fatal;
  bool m_inRun = false;
  bool m_fetching = false;          // an opcode fetch through the bus (no wait states charged)
  int  m_cyclesRequested = 0;
  std::function<void(UINT32)> m_trace;
  std::function<void()> m_delayFn;
  int  m_delayRemaining = 0;
  bool m_delayArmed = false;      // armed by the instruction now running: counting starts after it
};

#endif  // INCLUDED_I960_H
