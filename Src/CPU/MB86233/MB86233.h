// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
// Ported to Supermodel from MAME (src/devices/cpu/mb86233/mb86233.h).

/*
 * MB86233.h
 *
 * Fujitsu MB86233 "TGP" DSP (Sega Model 1 geometry coprocessor).
 *
 * Address spaces (all 32-bit words, word addressed):
 *   program : 16-bit address, opcode fetch and "mov (o)" reads
 *   data    : 16-bit address. 0x000-0x0ff and 0x200-0x3ff are the two
 *             on-chip RAM banks (kept inside this object, see below); all
 *             other addresses go to IMB86233Bus::ReadData/WriteData
 *             (Model 1: 0x100 = input FIFO read, 0x400 = output FIFO write).
 *   io      : 16-bit address, the "(e)" external accesses.
 *   rf      : 4-bit address, register file 0x20-0x2f mapped as I/O ports.
 *
 * Framework deviation: in MAME the two RAM banks are declared by the
 * machine's data address map (model1_state::copro_data_map) rather than by
 * the core. Here they live in the core (they are on-chip on the real part)
 * and are saved with the core state. EnableInternalRAM(false) routes every
 * data address to the bus instead.
 *
 * Timing: MAME's cycle unit is one instruction slot. execute_clocks_to_cycles
 * divides the input clock by 3 (cycles = ceil(clocks / 3)). Model 1 clocks
 * the TGP at 40 MHz (40_MHz_XTAL in model1.cpp), so 13.333... M cycles/s.
 * Most instructions cost 1 cycle; floating point ALU ops cost 1 extra.
 * Run(n) may overrun by 1 cycle and returns the actual amount used.
 *
 * Stall: call Stall() from inside a bus read handler (MAME's
 * on_fifo_empty_pre_sync). The read's value is discarded, the instruction
 * is rewound (re-executed next Run), 1 cycle is charged, and Run() returns.
 * In MAME the timeslice end comes from gen_fifo's machine-wide sync timer
 * (which aborts the timeslice); here the core performs that abort itself,
 * with the same cycle accounting as MAME's abort_timeslice().
 */

#ifndef INCLUDED_MB86233_H
#define INCLUDED_MB86233_H

#include "Types.h"
#include <functional>
#include <string>

class CBlockFile;

/*
 * IMB86233Bus:
 *
 * External bus for the MB86233. All addresses are word addresses.
 * Defaults return 0 / ignore writes (MAME unmapped behaviour).
 */
class IMB86233Bus
{
public:
  virtual UINT32 ReadProgram(UINT16 addr)         { return 0; }
  virtual UINT32 ReadData(UINT16 addr)            { return 0; }
  virtual void   WriteData(UINT16 addr, UINT32 v) { }
  virtual UINT32 ReadIO(UINT16 addr)              { return 0; }
  virtual void   WriteIO(UINT16 addr, UINT32 v)   { }
  virtual UINT32 ReadRF(UINT8 addr)               { return 0; }   // addr 0..15
  virtual void   WriteRF(UINT8 addr, UINT32 v)    { }
  virtual ~IMB86233Bus() { }
};

class CMB86233
{
public:
  // MAME's regs_ids, followed by Supermodel additions (MAME registers PC,
  // PPC and ST via STATE_GENPC/GENPCBASE/GENFLAGS, and RPC under REG_R).
  enum regs_ids {
    REG_A, REG_B, REG_D, REG_P,
    REG_R, REG_C0, REG_C1,
    REG_B0, REG_B1, REG_X0, REG_X1, REG_I0, REG_I1,
    REG_SFT, REG_VSM, REG_MASK, REG_M,
    REG_PCS0, REG_PCS1, REG_PCS2, REG_PCS3,
    REG_SP,
    // Supermodel additions
    REG_PC, REG_PPC, REG_ST, REG_RPC, REG_VSMR,
    REG_COUNT
  };

  enum st_flags {
    F_ZRC  = 0x00000001,
    F_ZRD  = 0x00000002,
    F_SGC  = 0x00000004,
    F_SGD  = 0x00000008,
    F_CPC  = 0x00000010,
    F_CPD  = 0x00000020,
    F_OVC  = 0x00000040,
    F_OVD  = 0x00000080,
    F_UNC  = 0x00000100,
    F_UND  = 0x00000200,
    F_DVZC = 0x00000400,
    F_DVZD = 0x00000800,
    F_CA   = 0x00001000,
    F_CPP  = 0x00002000,
    F_OVM  = 0x00004000,
    F_UNM  = 0x00008000,
    F_SIF0 = 0x00010000,
    F_SIF1 = 0x00020000,
    F_SOF0 = 0x00040000,

    F_PIF  = 0x00100000,
    F_POF  = 0x00200000,
    F_PAIF = 0x00400000,
    F_PAOF = 0x00800000,
    F_F0S  = 0x01000000,
    F_F1S  = 0x02000000,
    F_IT   = 0x04000000,
    F_ZX0  = 0x08000000,
    F_ZX1  = 0x10000000,
    F_ZX2  = 0x20000000,
    F_ZC0  = 0x40000000,
    F_ZC1  = 0x80000000u
  };

  // Clock conversion (MAME execute_clocks_to_cycles / cycles_to_clocks)
  static UINT64 ClocksToCycles(UINT64 clocks) { return (clocks + 3 - 1) / 3; }
  static UINT64 CyclesToClocks(UINT64 cycles) { return cycles * 3; }
  static const unsigned MODEL1_CLOCK_HZ = 40000000;

  CMB86233();

  void AttachBus(IMB86233Bus *bus) { m_bus = bus; }
  // Fast path for opcode fetch and program reads: addresses < numWords come
  // from this array (not copied, must stay valid), the rest go to the bus.
  void SetProgramROM(const UINT32 *words, unsigned numWords) { m_rom = words; m_romWords = numWords; }
  void EnableInternalRAM(bool enable) { m_internalRam = enable; }

  void Reset();                 // MAME device_reset (internal RAM is not cleared)
  int  Run(int cycles);         // returns cycles consumed (may exceed request by 1)
  void Stall() { m_stall = true; }
  void AbortTimeslice();

  // HALT input line (Model 1's FIFOs drive it). While halted Run() executes
  // nothing and consumes all requested cycles, like a halted MAME CPU.
  void SetHalt(bool state) { m_halt = state; }
  bool IsHalted() const     { return m_halt; }

  void gpio0_w(int state) { m_gpio0 = state != 0; }
  void gpio1_w(int state) { m_gpio1 = state != 0; }
  void gpio2_w(int state) { m_gpio2 = state != 0; }
  void gpio3_w(int state) { m_gpio3 = state != 0; }

  UINT16 GetPC() const  { return m_pc; }
  UINT16 GetPPC() const { return m_ppc; }
  UINT32 GetReg(int id) const;
  void   SetReg(int id, UINT32 v);
  static const char *GetRegName(int id);

  UINT32 *GetInternalRAM() { return m_ram; }   // 0x400 words, banks at 0x000 and 0x200

  std::string Disassemble(UINT16 pc);          // uses the program fetch path
  static std::string Disassemble(UINT32 opcode);

  void SetTraceCallback(std::function<void(UINT16 pc)> cb) { m_trace = std::move(cb); }

  void SaveState(CBlockFile *StateFile, const char *name);
  void LoadState(CBlockFile *StateFile, const char *name);

private:
  IMB86233Bus  m_nullBus;
  IMB86233Bus *m_bus;
  const UINT32 *m_rom;
  unsigned m_romWords;
  bool m_internalRam;
  UINT32 m_ram[0x400];
  std::function<void(UINT16)> m_trace;

  int m_icount;
  int m_stolen;
  bool m_halt;

  UINT32 m_st, m_a, m_b, m_d, m_p;
  UINT32 m_alu_stmask, m_alu_stset, m_alu_r1, m_alu_r2;
  UINT16 m_ppc, m_pc, m_sp, m_b0, m_b1, m_x0, m_x1, m_i0, m_i1, m_vsmr, m_pcs[4], m_mask, m_m;
  UINT8 m_r, m_rpc, m_c0, m_c1, m_sft, m_vsm;
  bool m_gpio0, m_gpio1, m_gpio2, m_gpio3;

  bool m_stall;

  // memory accessors (replace MAME memory_access specifics)
  UINT32 prog_r(UINT16 a) { return (a < m_romWords) ? m_rom[a] : m_bus->ReadProgram(a); }
  UINT32 data_r(UINT16 a);
  void   data_w(UINT16 a, UINT32 v);
  UINT32 io_r(UINT16 a)           { return m_bus->ReadIO(a); }
  void   io_w(UINT16 a, UINT32 v) { m_bus->WriteIO(a, v); }

  static UINT32 set_exp(UINT32 val, UINT32 exp);
  static UINT32 set_mant(UINT32 val, UINT32 mant);
  static UINT32 get_exp(UINT32 val);
  static UINT32 get_mant(UINT32 val);

  void alu_update_st();
  void alu_pre(UINT32 alu);
  void alu_post_1(UINT32 alu);
  void alu_post_2(UINT32 alu);
  UINT16 ea_pre_0(UINT32 r);
  void ea_post_0(UINT32 r);
  UINT16 ea_pre_1(UINT32 r);
  void ea_post_1(UINT32 r);
  void pcs_push();
  void pcs_pop();
  inline void stset_set_sz_int(UINT32 val);
  inline void stset_set_sz_fp(UINT32 val);

  UINT32 read_reg(UINT32 r);
  void write_reg(UINT32 r, UINT32 v);
  void write_mem_internal_1(UINT32 r, UINT32 v, bool bank);
  void write_mem_io_1(UINT32 r, UINT32 v);
};

#endif // INCLUDED_MB86233_H
