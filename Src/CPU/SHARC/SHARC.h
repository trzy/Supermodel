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
 * SHARC.h
 *
 * Analog Devices ADSP-21062 "SHARC", ported from MAME's interpreter (Ville
 * Linde; src/devices/cpu/sharc, MAME 0.289). The geometry coprocessor of Sega
 * Model 2B. The instruction code is MAME's, with Supermodel glue around it: a
 * bus interface instead of MAME address spaces, Run() instead of the
 * scheduler, save states. MAME's recompiler is not ported.
 *
 * Based on original work by Ville Linde in MAME. Portions Copyright (c) Ville
 * Linde, used under the BSD 3-Clause License: see Docs/README.txt.
 *
 * Only the ADSP-21062 (2 Mbit) internal memory map is emulated.
 */

#ifndef INCLUDED_SHARC_H
#define INCLUDED_SHARC_H

#include "Types.h"
#include <cstdint>
#include <functional>
#include <string>
#include <utility>

class CBlockFile;

/*
 * External bus. Gets the accesses MAME sends to the AS_DATA (32-bit words,
 * word addresses) and AS_PROGRAM (48-bit words in the low bits of a 64-bit
 * value, word addresses) spaces outside the internal memory: data addresses
 * from 0x80000 up, program addresses outside 0x20000-0x3ffff. Internal RAM
 * and the IOP registers stay inside the core.
 *
 * A bus that can't complete an access (e.g. an empty FIFO) calls
 * CSHARC::Stall() before returning; see Stall().
 */
class ISHARCBus
{
public:
  virtual ~ISHARCBus() {};
  virtual UINT32 ReadData(UINT32 addr) = 0;
  virtual void   WriteData(UINT32 addr, UINT32 data) = 0;
  virtual UINT64 ReadProgram(UINT32 addr) { return 0; }
  virtual void   WriteProgram(UINT32 addr, UINT64 data) {}
};

class CSHARC
{
public:
  enum
  {
    BOOT_MODE_EPROM,
    BOOT_MODE_HOST,
    BOOT_MODE_LINK,
    BOOT_MODE_NOBOOT
  };

  // Registers for GetReg()/SetReg() (MAME's state entries)
  enum
  {
    REG_PC = 0, REG_PCSTK, REG_PCSTKP, REG_LSTKP, REG_FADDR, REG_DADDR,
    REG_MODE1, REG_MODE2, REG_ASTAT, REG_STKY, REG_IRPTL, REG_IMASK, REG_IMASKP,
    REG_USTAT1, REG_USTAT2, REG_LCNTR, REG_CURLCNTR, REG_STSTKP, REG_PX_LO, REG_PX_HI,
    REG_MRF_LO, REG_MRF_HI, REG_MRB_LO, REG_MRB_HI,
    REG_R0,
    REG_I0 = REG_R0 + 16,
    REG_M0 = REG_I0 + 16,
    REG_L0 = REG_M0 + 16,
    REG_B0 = REG_L0 + 16,
    NUM_REGS = REG_B0 + 16
  };

  CSHARC();
  ~CSHARC();
  CSHARC(const CSHARC &) = delete;
  CSHARC &operator=(const CSHARC &) = delete;

  void AttachBus(ISHARCBus *bus);   // before Reset() (EPROM boot reads the bus)
  void SetBootMode(int mode) { m_boot_mode = mode; }   // before Reset(); Model 2B: BOOT_MODE_HOST

  void Reset();                     // MAME device_reset (clears the internal RAM)

  // Returns the cycles used. Like MAME's execute_run, an IDLE instruction ends
  // the slice early; the next Run() while idle (no IRQ pending) uses all its
  // cycles.
  int  Run(int cycles);
  int  CyclesRun() const;           // so far in the current Run()

  // Called by the bus from inside ReadData/WriteData (or the Program
  // variants) when the access can't complete, e.g. an empty FIFO. The value
  // returned by that read is discarded, the instruction is undone and retried
  // on the next Run(), and the current Run() ends, charging only the cycles
  // of the instructions that completed. Ignored outside instruction execution
  // (opcode fetches, DMA, External*() calls).
  void Stall();

  // Ends the current Run() after the current instruction (MAME's
  // abort_timeslice()), e.g. when it wakes another chip. Ignored outside Run().
  void EndSlice();

  // INPUT_LINE_HALT: while halted, Run() executes nothing and returns all the
  // cycles (like a halted MAME CPU)
  void SetHalt(bool halted) { m_halted = halted; }
  bool IsHalted() const     { return m_halted; }

  void SetIRQLine(int line, bool asserted);     // IRQ0-2
  void SetFlagInput(int flag, bool state);      // FLAG0-3 inputs
  void SetWriteStall(bool stall);               // MAME's write_stall()
  void SetFlagOutputCallback(std::function<void(int flag, int state)> cb) { m_flag_out = std::move(cb); }

  // MAME's external_iop_write() / external_dma_write(); ExternalIOPRead()
  // reads the IOP registers the core implements (others read 0)
  void   ExternalIOPWrite(UINT32 addr, UINT32 data);
  UINT32 ExternalIOPRead(UINT32 addr);
  void   ExternalDMAWrite(UINT32 addr, UINT64 data);

  void SetTraceCallback(std::function<void(UINT32 pc)> cb) { m_trace = std::move(cb); }

  UINT32 GetPC() const;
  UINT32 GetReg(unsigned n) const;
  void   SetReg(unsigned n, UINT32 v);
  static const char *GetRegName(unsigned n);
  bool   IsFatalHalted() const { return m_fatal; }

  // Debugger memory access: internal memory or the bus (no IOP side effects)
  UINT64 ReadProgramWord(UINT32 addr);
  UINT32 ReadDataWord(UINT32 addr);

  std::string Disassemble(UINT32 pc);           // reads the opcode with ReadProgramWord()
  static std::string Disassemble(UINT32 pc, UINT64 opcode);

  void SaveState(CBlockFile *StateFile, const char *name);
  void LoadState(CBlockFile *StateFile, const char *name);

  // MAME's ASTAT bit positions
  enum ASTAT_SHIFT
  {
    AZ_SHIFT = 0, AV_SHIFT = 1, AN_SHIFT = 2, AC_SHIFT = 3, AS_SHIFT = 4, AI_SHIFT = 5,
    MN_SHIFT = 6, MV_SHIFT = 7, MU_SHIFT = 8, MI_SHIFT = 9, AF_SHIFT = 10, SV_SHIFT = 11,
    SZ_SHIFT = 12, SS_SHIFT = 13, BTF_SHIFT = 18,
    FLG0_SHIFT = 19, FLG1_SHIFT = 20, FLG2_SHIFT = 21, FLG3_SHIFT = 22
  };

private:
  // ASTAT flags
  static constexpr uint32_t AZ =              0x00000001;    // ALU result zero
  static constexpr uint32_t AV =              0x00000002;    // ALU overflow
  static constexpr uint32_t AN =              0x00000004;    // ALU result negative
  static constexpr uint32_t AC =              0x00000008;    // ALU fixed-point carry
  static constexpr uint32_t AS =              0x00000010;    // ALU X input sign
  static constexpr uint32_t AI =              0x00000020;    // ALU floating-point invalid operation
  static constexpr uint32_t MN =              0x00000040;    // Multiplier result negative
  static constexpr uint32_t MV =              0x00000080;    // Multiplier overflow
  static constexpr uint32_t MU =              0x00000100;    // Multiplier underflow
  static constexpr uint32_t MI =              0x00000200;    // Multiplier floating-point invalid operation
  static constexpr uint32_t AF =              0x00000400;    // Last ALU operation was a floating-point operation
  static constexpr uint32_t SV =              0x00000800;    // Shifter overflow
  static constexpr uint32_t SZ =              0x00001000;    // Shifter result zero
  static constexpr uint32_t SS =              0x00002000;    // Shifter input sign
  static constexpr uint32_t BTF =             0x00040000;    // Bit Test Flag
  static constexpr uint32_t FLG0 =            0x00080000;    // FLAG0
  static constexpr uint32_t FLG1 =            0x00100000;    // FLAG1
  static constexpr uint32_t FLG2 =            0x00200000;    // FLAG2
  static constexpr uint32_t FLG3 =            0x00400000;    // FLAG3

  // STKY flags
  static constexpr uint32_t AUS =             0x00000001;    // ALU floating-point underflow
  static constexpr uint32_t AVS =             0x00000002;    // ALU floating-point overflow
  static constexpr uint32_t AOS =             0x00000004;    // ALU fixed-point overflow
  static constexpr uint32_t AIS =             0x00000020;    // ALU floating-point invalid operation
  static constexpr uint32_t MOS =             0x00000040;    // Multiplier fixed-point overflow
  static constexpr uint32_t MVS =             0x00000080;    // Multiplier floating-point overflow
  static constexpr uint32_t MUS =             0x00000100;    // Multiplier underflow
  static constexpr uint32_t MIS =             0x00000200;    // Multiplier floating-point invalid operation
  static constexpr uint32_t PCFL =            0x00200000;    // PC stack full
  static constexpr uint32_t PCEM =            0x00400000;    // PC stack empty
  static constexpr uint32_t SSOV =            0x00800000;    // Status stack overflow
  static constexpr uint32_t SSEM =            0x01000000;    // Status stack empty
  static constexpr uint32_t LSOV =            0x02000000;    // Loop stacks overflow
  static constexpr uint32_t LSEM =            0x04000000;    // Loop stacks empty

  // MODE1 flags
  static constexpr uint32_t MODE1_BR8 =       0x00000001;    // Bit-reverse for I8
  static constexpr uint32_t MODE1_BR0 =       0x00000002;    // Bit-reverse for I0
  static constexpr uint32_t MODE1_SRCU =      0x00000004;    // Alternate register select for computational units
  static constexpr uint32_t MODE1_SRD1H =     0x00000008;    // DAG alternate register select (7-4)
  static constexpr uint32_t MODE1_SRD1L =     0x00000010;    // DAG alternate register select (3-0)
  static constexpr uint32_t MODE1_SRD2H =     0x00000020;    // DAG alternate register select (15-12)
  static constexpr uint32_t MODE1_SRD2L =     0x00000040;    // DAG alternate register select (11-8)
  static constexpr uint32_t MODE1_SRRFH =     0x00000080;    // Register file alternate select for R(15-8)
  static constexpr uint32_t MODE1_SRRFL =     0x00000400;    // Register file alternate select for R(7-0)
  static constexpr uint32_t MODE1_NESTM =     0x00000800;    // Interrupt nesting enable
  static constexpr uint32_t MODE1_IRPTEN =    0x00001000;    // Global interrupt enable
  static constexpr uint32_t MODE1_ALUSAT =    0x00002000;    // Enable ALU fixed-point saturation
  static constexpr uint32_t MODE1_SSE =       0x00004000;    // Enable short word sign extension
  static constexpr uint32_t MODE1_TRUNCATE =  0x00008000;    // (1) Floating-point truncation / (0) round to nearest
  static constexpr uint32_t MODE1_RND32 =     0x00010000;    // (1) 32-bit floating-point rounding / (0) 40-bit rounding
  static constexpr uint32_t MODE1_CSEL =      0x00060000;    // CSelect

  // MODE2 flags
  static constexpr uint32_t MODE2_IRQ0E =     0x00000001;    // IRQ0 (1) Edge sens. / (0) Level sens.
  static constexpr uint32_t MODE2_IRQ1E =     0x00000002;    // IRQ1 (1) Edge sens. / (0) Level sens.
  static constexpr uint32_t MODE2_IRQ2E =     0x00000004;    // IRQ2 (1) Edge sens. / (0) Level sens.
  static constexpr uint32_t MODE2_CADIS =     0x00000010;    // Cache disable
  static constexpr uint32_t MODE2_TIMEN =     0x00000020;    // Timer enable
  static constexpr uint32_t MODE2_BUSLK =     0x00000040;    // External bus lock
  static constexpr uint32_t MODE2_FLG0O =     0x00008000;    // FLAG0 (1) Output / (0) Input
  static constexpr uint32_t MODE2_FLG1O =     0x00010000;    // FLAG1 (1) Output / (0) Input
  static constexpr uint32_t MODE2_FLG2O =     0x00020000;    // FLAG2 (1) Output / (0) Input
  static constexpr uint32_t MODE2_FLG3O =     0x00040000;    // FLAG3 (1) Output / (0) Input
  static constexpr uint32_t MODE2_CAFRZ =     0x00080000;    // Cache freeze

  enum { CLEAR_LINE = 0, ASSERT_LINE = 1 };

  using opcode_func = void (CSHARC::*)();
  struct SHARC_OP
  {
    uint32_t op_mask;
    uint32_t op_bits;
    opcode_func handler;
  };

  union SHARC_REG
  {
    int32_t r;
    float f;
  };

  struct SHARC_DMA_REGS
  {
    uint32_t control;
    uint32_t int_index;
    uint32_t int_modifier;
    uint32_t int_count;
    uint32_t chain_ptr;
    uint32_t gen_purpose;
    uint32_t ext_index;
    uint32_t ext_modifier;
    uint32_t ext_count;
  };

  struct SHARC_DMA_OP
  {
    uint32_t src;
    uint32_t dst;
    uint32_t chain_ptr;
    int32_t src_modifier;
    int32_t dst_modifier;
    int32_t src_count;
    int32_t dst_count;
    int32_t pmode;
    int32_t chained_direction;
    bool active;
    bool chained;
  };

  // MAME's sharc_internal_state (sharcinternal.ipp), without the recompiler
  // and timer fields
  struct sharc_internal_state
  {
    struct SHARC_DAG
    {
      uint32_t i[8];
      uint32_t m[8];
      uint32_t b[8];
      uint32_t l[8];
    };

    struct SHARC_LADDR
    {
      uint32_t addr;
      uint32_t code;
      uint32_t loop_type;

      uint32_t pack() const
      {
        return (loop_type << 30) | (code << 24) | addr;
      }

      void unpack(uint32_t in)
      {
        addr = in & 0xffffff;
        code = (in >> 24) & 0x1f;
        loop_type = (in >> 30) & 3;
      }
    };

    SHARC_REG r[16];
    SHARC_REG reg_alt[16];

    uint32_t pc;
    uint64_t mrf;
    uint64_t mrb;

    uint32_t pcstack[30];
    uint32_t lcstack[6];
    uint32_t lastack[6];
    uint32_t lstkp;

    uint32_t faddr;
    uint32_t daddr;
    uint32_t pcstk;
    uint32_t pcstkp;
    SHARC_LADDR laddr;
    uint32_t curlcntr;
    uint32_t lcntr;
    uint8_t extdma_shift;
    uint32_t iop_write_num;
    uint32_t iop_data;

    /* Data Address Generator (DAG) */
    SHARC_DAG dag1;     // (DM bus)
    SHARC_DAG dag2;     // (PM bus)
    SHARC_DAG dag1_alt;
    SHARC_DAG dag2_alt;

    SHARC_DMA_REGS dma[12];

    /* System registers */
    uint32_t mode1;
    uint32_t mode2;
    uint32_t astat;
    uint32_t stky;
    uint32_t irptl;
    uint32_t imask;
    uint32_t imaskp;
    uint32_t ustat1;
    uint32_t ustat2;

    uint32_t flag[4];

    uint32_t syscon;
    uint32_t sysstat;

    struct
    {
      uint32_t mode1;
      uint32_t astat;
    } status_stack[5];
    int32_t status_stkp;

    uint64_t px;

    int icount;
    uint64_t opcode;

    uint32_t nfaddr;

    int32_t idle;
    int32_t irq_pending;
    int32_t active_irq_num;

    SHARC_DMA_OP dma_op[12];
    uint32_t dma_status;
    bool write_stalled;

    int32_t interrupt_active;

    uint32_t iop_delayed_reg;
    uint32_t iop_delayed_data;

    uint32_t delay_slot1, delay_slot2;

    int32_t systemreg_latency_cycles;
    int32_t systemreg_latency_reg;
    uint32_t systemreg_latency_data;
    uint32_t systemreg_previous_data;

    uint32_t astat_old;
    uint32_t astat_old_old;
    uint32_t astat_old_old_old;
  };

  // Supermodel: state an instruction can change before its external memory
  // access, restored when the bus stalls it
  struct StallCheckpoint
  {
    SHARC_REG r[16];
    uint64_t mrf, mrb, px;
    uint32_t astat, stky;
    uint32_t dag1_i[8], dag2_i[8];
  };

  static const SHARC_OP s_sharc_opcode_table[];
  static const size_t s_num_ops;

  static const uint32_t recips_mantissa_lookup[128];
  static const uint32_t rsqrts_mantissa_lookup[128];

  sharc_internal_state m_core_state;
  sharc_internal_state *const m_core;

  int m_boot_mode;
  opcode_func m_sharc_op[512];

  // Internal memory: two 0x8000 x 32-bit blocks (ADSP-21062)
  uint32_t m_blocks[2][0x8000];
  bool m_dm_short_se;               // MAME's m_dm_short_view selected (MODE1 SSE)
  uint32_t m_msgr[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };   // Supermodel: message registers (IOP 08-0F)

  // Supermodel
  ISHARCBus *m_bus;
  std::function<void(int, int)> m_flag_out;
  std::function<void(UINT32)> m_trace;
  int  m_iop_delay_cycles;          // MAME's delayed_iop_timer
  bool m_halted;
  bool m_fatal;
  bool m_inRun;
  int  m_cyclesRequested;
  bool m_stalled;                   // Stall() called during the current access
  bool m_stall_allowed;             // executing an instruction handler
  bool m_retry;                     // re-execute m_core->opcode without fetching
  bool m_ckpt_valid;
  StallCheckpoint m_ckpt;

  const char *tag() const { return "SHARC"; }
  void flag_out(int flag, int state) { if (m_flag_out) m_flag_out(flag, state); }
  void stall_checkpoint();
  void stall_checkpoint_dm(uint32_t address) { if (!m_ckpt_valid && address >= 0x80000) stall_checkpoint(); }
  void stall_checkpoint_pm(uint32_t address) { if (!m_ckpt_valid && (address & ~0x1ffffu) != 0x20000) stall_checkpoint(); }
  void stall_restore();
  void stall_check();
  void advance_iop_timer(int cycles);

  static std::string disassemble_one(uint32_t pc, uint64_t opcode);
  void build_opcode_table();
  void execute_run();
  void execute_set_input(int inputnum, int state);
  void set_flag_input(int flag_num, int state);
  void write_stall(int state);
  void external_iop_write(uint32_t address, uint32_t data);
  void external_dma_write(uint32_t address, uint64_t data);

  // Internal memory
  uint64_t pm_r(unsigned n, uint32_t offset) const;
  void pm_w(unsigned n, uint32_t offset, uint64_t data, uint64_t mem_mask);
  uint32_t dm_short_r(unsigned n, uint32_t offset) const;
  uint32_t dm_short_se_r(unsigned n, uint32_t offset) const;
  void dm_short_w(unsigned n, uint32_t offset, uint32_t data);
  uint32_t iop_r(uint32_t offset, bool side_effects = true);
  void iop_w(uint32_t offset, uint32_t data);
  uint32_t dm_internal_r(uint32_t address, bool side_effects = true);
  void dm_internal_w(uint32_t address, uint32_t data);
  bool pm_internal(uint32_t address, unsigned &block, uint32_t &offset) const;
  uint64_t program_read_qword(uint32_t address);
  void program_write_qword(uint32_t address, uint64_t data, uint64_t mem_mask = ~uint64_t(0));

  // MAME's core
  void sharc_iop_delayed_write_callback();
  inline void CHANGE_PC(uint32_t newpc);
  inline void CHANGE_PC_DELAYED(uint32_t newpc);
  void sharc_iop_delayed_w(uint32_t reg, uint32_t data, int cycles);
  uint32_t pm_read32(uint32_t address);
  void pm_write32(uint32_t address, uint32_t data);
  uint64_t pm_read48(uint32_t address);
  void pm_write48(uint32_t address, uint64_t data);
  uint32_t dm_read32(uint32_t address);
  void dm_write32(uint32_t address, uint32_t data);
  void schedule_chained_dma_op(int channel, uint32_t dma_chain_ptr, int chained_direction);
  void schedule_dma_op(int channel, uint32_t src, uint32_t dst, int src_modifier, int dst_modifier, int src_count, int dst_count, int pmode);
  void dma_op(int channel);
  void sharc_dma_exec(int channel);
  void dma_run_cycle(int channel);
  void add_systemreg_write_latency_effect(int sysreg, uint32_t data, uint32_t prev_data);
  void systemreg_write_latency_effect();
  uint32_t GET_UREG(int ureg);
  void SET_UREG(int ureg, uint32_t data);
  void SHIFT_OPERATION_IMM(int shiftop, int data, int rn, int rx);
  void COMPUTE(uint32_t opcode);
  void check_interrupts();
  void PUSH_PC();
  uint32_t POP_PC();
  uint32_t TOP_PC();
  void PUSH_LOOP();
  void POP_LOOP();
  void PUSH_STATUS_STACK();
  void POP_STATUS_STACK();
  int IF_CONDITION_CODE(int cond);
  int DO_CONDITION_CODE(int cond);
  void sharcop_compute_dreg_dm_dreg_pm();
  void sharcop_compute();
  void sharcop_compute_ureg_dmpm_premod();
  void sharcop_compute_ureg_dmpm_postmod();
  void sharcop_compute_dm_to_dreg_immmod();
  void sharcop_compute_dreg_to_dm_immmod();
  void sharcop_compute_pm_to_dreg_immmod();
  void sharcop_compute_dreg_to_pm_immmod();
  void sharcop_compute_ureg_to_ureg();
  void sharcop_imm_shift_dreg_dmpm();
  void sharcop_imm_shift();
  void sharcop_compute_modify();
  void sharcop_direct_call();
  void sharcop_direct_jump();
  void sharcop_relative_call();
  void sharcop_relative_jump();
  void sharcop_indirect_jump();
  void sharcop_indirect_call();
  void sharcop_relative_jump_compute();
  void sharcop_relative_call_compute();
  void sharcop_indirect_jump_compute_dreg_dm();
  void sharcop_relative_jump_compute_dreg_dm();
  void sharcop_rts();
  void sharcop_rti();
  void sharcop_do_until_counter_imm();
  void sharcop_do_until_counter_ureg();
  void sharcop_do_until();
  void sharcop_dm_to_ureg_direct();
  void sharcop_ureg_to_dm_direct();
  void sharcop_pm_to_ureg_direct();
  void sharcop_ureg_to_pm_direct();
  void sharcop_dm_to_ureg_indirect();
  void sharcop_ureg_to_dm_indirect();
  void sharcop_pm_to_ureg_indirect();
  void sharcop_ureg_to_pm_indirect();
  void sharcop_imm_to_dmpm();
  void sharcop_imm_to_ureg();
  void sharcop_sysreg_bitop();
  void sharcop_modify();
  void sharcop_bit_reverse();
  void sharcop_push_pop_stacks();
  void sharcop_nop();
  void sharcop_idle();
  [[noreturn]] void sharcop_unimplemented();

  // ALU helpers
  inline SHARC_REG FADD(int fx, int fy);
  inline SHARC_REG FSUB(int fx, int fy);
  inline SHARC_REG FAVG(int fx, int fy);
  inline SHARC_REG FABS(int fx);
  inline SHARC_REG FMIN(int fx, int fy);
  inline SHARC_REG FMAX(int fx, int fy);
  inline std::pair<SHARC_REG, SHARC_REG> FADD_FSUB(int fx, int fy);
  inline uint32_t SCALB(SHARC_REG fx, int ry);

  // Multiplier helpers
  inline SHARC_REG FMUL(int fx, int fy);

  // Supermodel (review fixes): rounding mode and float to fixed conversion
  float RoundToZero(double value, double error) const;
  float AddF(float a, float b) const;
  float MulF(float a, float b) const;
  int32_t FixValue(float value, bool &overflow) const;

  // ALU fixed-point
  inline void compute_add(int rn, int rx, int ry);
  inline void compute_sub(int rn, int rx, int ry);
  inline void compute_add_ci(int rn, int rx, int ry);
  inline void compute_sub_ci(int rn, int rx, int ry);
  inline void compute_comp(int rx, int ry);
  inline void compute_add_ci(int rn, int rx);
  inline void compute_sub_ci(int rn, int rx);
  inline void compute_inc(int rn, int rx);
  inline void compute_dec(int rn, int rx);
  inline void compute_neg(int rn, int rx);
  inline void compute_abs(int rn, int rx);
  inline void compute_pass(int rn, int rx);
  inline void compute_and(int rn, int rx, int ry);
  inline void compute_or(int rn, int rx, int ry);
  inline void compute_xor(int rn, int rx, int ry);
  inline void compute_not(int rn, int rx);
  inline void compute_min(int rn, int rx, int ry);
  inline void compute_max(int rn, int rx, int ry);
  inline void compute_clip(int rn, int rx, int ry);

  // ALU floating-point
  inline void compute_fadd(int fn, int fx, int fy);
  inline void compute_fsub(int fn, int fx, int fy);
  inline void compute_fadd_abs(int fn, int fx, int fy);
  inline void compute_fsub_abs(int fn, int fx, int fy);
  inline void compute_favg(int fn, int fx, int fy);
  inline void compute_fcomp(int fx, int fy);
  inline void compute_fneg(int fn, int fx);
  inline void compute_fabs(int fn, int fx);
  inline void compute_fpass(int fn, int fx);
  inline void compute_scalb(int fn, int fx, int ry);
  inline void compute_logb(int rn, int fx);
  inline void compute_fix_scaled(int rn, int fx, int ry);
  inline void compute_fix(int rn, int fx);
  inline void compute_float_scaled(int fn, int rx, int ry);
  inline void compute_float(int fn, int rx);
  inline void compute_recips(int fn, int fx);
  inline void compute_rsqrts(int fn, int fx);
  inline void compute_fcopysign(int fn, int fx, int fy);
  inline void compute_fmin(int fn, int fx, int fy);
  inline void compute_fmax(int fn, int fx, int fy);
  inline void compute_fclip(int fn, int fx, int fy);

  // Multiplier
  inline void compute_mul_uuin(int rn, int rx, int ry);
  inline void compute_mul_ssin(int rn, int rx, int ry);
  inline uint32_t compute_mrf_plus_mul_ssin(int rx, int ry);
  inline uint32_t compute_mrb_plus_mul_ssin(int rx, int ry);
  inline void compute_fmul(int fn, int fx, int fy);

  // Dual add/subtract
  inline void compute_dual_add_sub(int ra, int rs, int rx, int ry);
  inline void compute_dual_fadd_fsub(int fa, int fs, int fx, int fy);

  // Fixed-Point multiply/accumulate and add, subtract or average
  inline void compute_mul_ssfr_add(int rm, int rxm, int rym, int ra, int rxa, int rya);
  inline void compute_mul_ssfr_sub(int rm, int rxm, int rym, int ra, int rxa, int rya);

  // Floating-Point multiplication and ALU operation
  inline void compute_fmul_fadd(int fm, int fxm, int fym, int fa, int fxa, int fya);
  inline void compute_fmul_fsub(int fm, int fxm, int fym, int fa, int fxa, int fya);
  inline void compute_fmul_float_scaled(int fm, int fxm, int fym, int fa, int rxa, int rya);
  inline void compute_fmul_fix_scaled(int fm, int fxm, int fym, int ra, int fxa, int rya);
  inline void compute_fmul_favg(int fm, int fxm, int fym, int fa, int fxa, int fya);
  inline void compute_fmul_fabs(int fm, int fxm, int fym, int fa, int fxa);
  inline void compute_fmul_fmax(int fm, int fxm, int fym, int fa, int fxa, int fya);
  inline void compute_fmul_fmin(int fm, int fxm, int fym, int fa, int fxa, int fya);

  // Multiplication and dual add/subtract
  inline void compute_fmul_dual_fadd_fsub(int fm, int fxm, int fym, int fa, int fs, int fxa, int fya);

  inline void compute_multi_mr_to_reg(int ai, int rk);
  inline void compute_multi_reg_to_mr(int ai, int rk);
};

#endif  // INCLUDED_SHARC_H
