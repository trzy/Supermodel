/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2003-2026 The Supermodel Team
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
 * 53C810.cpp
 * 
 * Implementation of the C53C810 class: NCR 53C810 SCSI controller.
 *
 * References:
 * -----------
 * [810A]  Technical Manual, LSI53C810A PCI to SCSI I/O Processor, Version 2.1,
 *         March 2001. Page numbers cited without a document refer to this
 *         manual.
 * [825A]  Technical Manual, LSI53C825A/825AE PCI to SCSI I/O Processor,
 *         Version 3.1, January 2001 (DB14-000159-00, Fourth Edition). Used
 *         for target mode operation and the 53C825 used by the Real3D
 *         Pro-1000.
 *
 * Notes:
 * ------
 * - VF3 does something weird: it writes DSP (triggering automatic code
 *   execution because MAN=0) and THEN sets single step mode, expecting an
 *   interrupt to occur. I suspect this is incorrect operation and that
 *   the SCRIPTS processor either enters single-step mode while the memory
 *   transfer is underway (though it is unlikely because it's such a short
 *   transfer), or that single step can occur even when the device is "halted"
 *   (which would mean the SCRIPTS processor executes an invalid instruction
 *   once or twice without the VF3 SCSI driver noticing). Unfortunately, the
 *   former is not feasible to emulate. If automatic SCRIPTS execution is
 *   disabled when single-stepping is enabled, Scud Race breaks (glitchy,
 *   jerky graphics). Enabling automatic execution and also allowing single
 *   stepping to occur when the processor is halted seems to work, but it 
 *   causes invalid instructions to be hit each time.
 * - pg 2-22 (42) of [810A] has description of how to clear interrupts.
 * - Model 3 games use the SCRIPTS processor only as a DMA engine (memory move
 *   and interrupt instructions, in initiator mode). The Real3D Pro-1000 uses
 *   it as a SCSI target (its 53C825 runs target mode SCRIPTS). Target mode
 *   support covers what the Pro-1000 firmware's SCRIPTS use: JUMP, CALL,
 *   RETURN, register read/write, and the target mode I/O instructions.
 *   Initiator mode I/O instructions and phase tests are not emulated.
 * - Instructions that wait on the SCSI bus (e.g., WAIT SELECT) set the
 *   waiting flag and leave DSP unchanged, ending the current Run(). The next
 *   Run() executes them again. Because no SCSI bus is attached yet, a target
 *   is never selected and block moves wait indefinitely. SIGP aborts WAIT
 *   SELECT.
 * - TODO: DSP is inaccurate while an instruction waits. On the real chip, DSP
 *   is advanced past an instruction as soon as it is fetched, so a CPU
 *   reading DSP during WAIT SELECT would see the address of the following
 *   instruction. Here, DSP still points at the waiting instruction. Likewise,
 *   INT leaves DSP pointing at itself (VF3 relies on this; see above). The
 *   Pro-1000 firmware does not read DSP while SCRIPTS wait.
 *
 */

#include "53C810.h"

#include <cstring>
#include "Supermodel.h"
#include "CPU/PowerPC/ppc.h"

/******************************************************************************
 Save States
******************************************************************************/

// TODO: target mode state (waiting, carry, ATN, and connection status) is not
// saved. Model 3 never uses it and the Pro-1000 does not yet support save
// states. Saving in the middle of a SCSI transaction may not be meaningful.
void C53C810::SaveState(CBlockFile *SaveState)
{
  SaveState->NewBlock("53C810", __FILE__);
  SaveState->Write(Ctx.regs, sizeof(Ctx.regs));
  SaveState->Write(&Ctx.regTEMP, sizeof(Ctx.regTEMP));
  SaveState->Write(&Ctx.regDSP, sizeof(Ctx.regDSP));
  SaveState->Write(&Ctx.regDSPS, sizeof(Ctx.regDSPS));
  SaveState->Write(&Ctx.regDBC, sizeof(Ctx.regDBC));
  SaveState->Write(&Ctx.regDCMD, sizeof(Ctx.regDCMD));
  SaveState->Write(&Ctx.regDCNTL, sizeof(Ctx.regDCNTL));
  SaveState->Write(&Ctx.regDMODE, sizeof(Ctx.regDMODE));
  SaveState->Write(&Ctx.regDSTAT, sizeof(Ctx.regDSTAT));
  SaveState->Write(&Ctx.regDIEN, sizeof(Ctx.regDIEN));
  SaveState->Write(&Ctx.regISTAT, sizeof(Ctx.regISTAT));
}

void C53C810::LoadState(CBlockFile *SaveState)
{
  if (Result::OKAY != SaveState->FindBlock("53C810"))
  {
    ErrorLog("Unable to load 53C810 state. Save state file is corrupt.");
    return;
  }
  
  SaveState->Read(Ctx.regs, sizeof(Ctx.regs));
  SaveState->Read(&Ctx.regTEMP, sizeof(Ctx.regTEMP));
  SaveState->Read(&Ctx.regDSP, sizeof(Ctx.regDSP));
  SaveState->Read(&Ctx.regDSPS, sizeof(Ctx.regDSPS));
  SaveState->Read(&Ctx.regDBC, sizeof(Ctx.regDBC));
  SaveState->Read(&Ctx.regDCMD, sizeof(Ctx.regDCMD));
  SaveState->Read(&Ctx.regDCNTL, sizeof(Ctx.regDCNTL));
  SaveState->Read(&Ctx.regDMODE, sizeof(Ctx.regDMODE));
  SaveState->Read(&Ctx.regDSTAT, sizeof(Ctx.regDSTAT));
  SaveState->Read(&Ctx.regDIEN, sizeof(Ctx.regDIEN));
  SaveState->Read(&Ctx.regISTAT, sizeof(Ctx.regISTAT));
}


/******************************************************************************
 SCRIPTS Emulation
******************************************************************************/

static inline UINT32 Fetch(struct NCR53C810Context *Ctx, UINT32 offset)
{
  UINT32 data = Ctx->Bus->Read32(Ctx->regDSP + offset);
  return FLIPENDIAN32(data);  // remember: bus is big endian, need to convert to little endian
}

static Result SCRIPTS_Invalid(struct NCR53C810Context *Ctx);

static constexpr UINT8 ISTAT_SRST = 0x40; // software reset
static constexpr UINT8 ISTAT_SIGP = 0x20; // signal process
static constexpr UINT8 ISTAT_CON = 0x08;  // connected to SCSI bus

static inline bool TargetMode(const struct NCR53C810Context *Ctx)
{
  return (Ctx->regs[0x00] & 1) != 0;  // SCNTL0.TRG
}

static inline INT32 SignExtend24(UINT32 x)
{
  return (INT32) ((x & 0x00FFFFFF) ^ 0x00800000) - 0x00800000;
}

// Destination of a jump. Relative addresses are offsets from the next
// instruction (DSP has not yet been advanced past this one).
static UINT32 JumpAddress(const struct NCR53C810Context *Ctx, bool relative)
{
  if (relative)
    return Ctx->regDSP + 8 + SignExtend24(Ctx->regDSPS);
  return Ctx->regDSPS;
}

// Evaluates the condition of a transfer control instruction (JUMP, CALL,
// RETURN, INT). Returns true if the transfer should occur. When both phase
// and data are compared, both must be true to branch on true and both must
// be false to branch on false.
static bool TransferConditionMet(const struct NCR53C810Context *Ctx)
{
  bool jumpIfTrue = (Ctx->regDBC & 0x080000) != 0;
  if ((Ctx->regDBC & 0x200000))     // carry test (phase and data compares are illegal)
    return Ctx->carry == jumpIfTrue;

  bool compared = false;
  bool allMatch = true;             // every enabled comparison matches the condition
  if ((Ctx->regDBC & 0x020000))     // compare phase
  {
    bool phase;
    if (TargetMode(Ctx))
      phase = Ctx->atn;             // in target mode, tests ATN
    else
    {
      DebugLog("53C810: initiator mode phase comparison not emulated (DSP=%08X)\n", Ctx->regDSP);
      phase = false;
    }
    compared = true;
    allMatch = allMatch && (phase == jumpIfTrue);
  }
  if ((Ctx->regDBC & 0x040000))     // compare data with SFBR (mask bits that are set are not compared)
  {
    UINT8 compareMask = ~(Ctx->regDBC >> 8) & 0xFF;
    bool data = (Ctx->regs[0x08] & compareMask) == (Ctx->regDBC & compareMask);
    compared = true;
    allMatch = allMatch && (data == jumpIfTrue);
  }
  if (!compared)
    return jumpIfTrue;              // unconditional (e.g., INT 0x98080000 used by Model 3)
  return allMatch;
}

// JUMP, CALL, RETURN
static Result SCRIPTS_TransferControl(struct NCR53C810Context *Ctx)
{
  unsigned opcode = (Ctx->regDCMD >> 3) & 7;
  if (!TransferConditionMet(Ctx))
  {
    Ctx->regDSP += 8;
    return Result::OKAY;
  }
  bool relative = (Ctx->regDBC & 0x800000) != 0;
  switch (opcode)
  {
  case 0:   // JUMP
    Ctx->regDSP = JumpAddress(Ctx, relative);
    break;
  case 1:   // CALL (return address saved in TEMP)
    Ctx->regTEMP = Ctx->regDSP + 8;
    Ctx->regDSP = JumpAddress(Ctx, relative);
    break;
  case 2:   // RETURN
    Ctx->regDSP = Ctx->regTEMP;
    break;
  default:
    return ErrorLog("53C810: invalid transfer control instruction (%02X%06X, DSP=%08X)", Ctx->regDCMD, Ctx->regDBC, Ctx->regDSP);
  }
  return Result::OKAY;
}

// Register access by SCRIPTS (no side effects)
static UINT8 ScriptsReadRegister(const struct NCR53C810Context *Ctx, unsigned reg)
{
  switch (reg)
  {
  case 0x1C: case 0x1D: case 0x1E: case 0x1F:   // TEMP
    return (Ctx->regTEMP >> ((reg - 0x1C) * 8)) & 0xFF;
  case 0x30: case 0x31: case 0x32: case 0x33:   // DSPS
    return (Ctx->regDSPS >> ((reg - 0x30) * 8)) & 0xFF;
  default:
    return reg < sizeof(Ctx->regs) ? Ctx->regs[reg] : 0;
  }
}

static void ScriptsWriteRegister(struct NCR53C810Context *Ctx, unsigned reg, UINT8 data)
{
  switch (reg)
  {
  case 0x1C: case 0x1D: case 0x1E: case 0x1F:   // TEMP
  {
    unsigned shift = (reg - 0x1C) * 8;
    Ctx->regTEMP = (Ctx->regTEMP & ~(0xFF << shift)) | (UINT32(data) << shift);
    break;
  }
  case 0x30: case 0x31: case 0x32: case 0x33:   // DSPS
  {
    unsigned shift = (reg - 0x30) * 8;
    Ctx->regDSPS = (Ctx->regDSPS & ~(0xFF << shift)) | (UINT32(data) << shift);
    break;
  }
  default:
    if (reg < sizeof(Ctx->regs))
      Ctx->regs[reg] = data;
    break;
  }
}

// Read/write register instructions: move SFBR to register (opcode 5),
// register to SFBR (6), and read-modify-write (7)
static Result SCRIPTS_ReadWriteRegister(struct NCR53C810Context *Ctx)
{
  unsigned opcode = (Ctx->regDCMD >> 3) & 7;
  unsigned oper = Ctx->regDCMD & 7;
  unsigned reg = (Ctx->regDBC >> 16) & 0x7F;
  bool useSFBR = opcode == 7 && (Ctx->regDBC & 0x800000);  // SFBR replaces data8 (read-modify-write only)
  UINT8 operand = useSFBR ? Ctx->regs[0x08] : (Ctx->regDBC >> 8) & 0xFF;
  UINT8 src = (opcode == 5) ? Ctx->regs[0x08] : ScriptsReadRegister(Ctx, reg);
  unsigned result;
  bool carryIn = Ctx->carry;

  switch (oper)
  {
  case 0:   // move data
    result = operand;
    break;
  case 1:   // shift left through carry
    Ctx->carry = (src & 0x80) != 0;
    result = (src << 1) | (carryIn ? 0x01 : 0);
    break;
  case 2:   // OR
    result = src | operand;
    break;
  case 3:   // XOR
    result = src ^ operand;
    break;
  case 4:   // AND
    result = src & operand;
    break;
  case 5:   // shift right through carry
    Ctx->carry = (src & 1) != 0;
    result = (src >> 1) | (carryIn ? 0x80 : 0);
    break;
  case 6:   // add
    result = src + operand;
    Ctx->carry = result > 0xFF;
    break;
  default:  // add with carry
    result = src + operand + (carryIn ? 1 : 0);
    Ctx->carry = result > 0xFF;
    break;
  }

  if (opcode == 6)
    Ctx->regs[0x08] = result & 0xFF;  // SFBR
  else
    ScriptsWriteRegister(Ctx, reg, result & 0xFF);
  Ctx->regDSP += 8;
  return Result::OKAY;
}

// I/O instructions in target mode: RESELECT, DISCONNECT, WAIT SELECT, SET,
// CLEAR
static Result SCRIPTS_IO(struct NCR53C810Context *Ctx)
{
  unsigned opcode = (Ctx->regDCMD >> 3) & 7;
  if (!TargetMode(Ctx))
    return SCRIPTS_Invalid(Ctx);  // initiator mode I/O instructions are not emulated

  bool relative = (Ctx->regDCMD & 4) != 0;  // bit 26 (bit 25 is table indirect, bit 24 is select with ATN)
  switch (opcode)
  {
  case 0:   // RESELECT
    // Takes the alternate address only if we are selected by an initiator
    // before winning arbitration. SIGP has no effect. No SCSI bus is attached
    // yet, so there is no initiator to reselect.
    Ctx->waiting = true;
    return Result::OKAY;
  case 2:   // WAIT SELECT
    if ((Ctx->regISTAT & ISTAT_SIGP))
    {
      // Signaled by the CPU: give up and take the alternate address
      Ctx->regDSP = JumpAddress(Ctx, relative);
      return Result::OKAY;
    }
    // No SCSI bus is attached yet, so we are never selected
    Ctx->waiting = true;
    return Result::OKAY;
  case 1:   // DISCONNECT
    Ctx->regISTAT &= ~ISTAT_CON;
    Ctx->regDSP += 8;
    return Result::OKAY;
  case 3:   // SET
  case 4:   // CLEAR
  {
    bool set = opcode == 3;
    if ((Ctx->regDBC & 0x400))  // carry
      Ctx->carry = set;
    if ((Ctx->regDBC & 0x200))  // target mode
      Ctx->regs[0x00] = set ? (Ctx->regs[0x00] | 1) : (Ctx->regs[0x00] & ~1);
    // ATN and ACK are latched in SOCL but are not driven onto the bus in
    // target mode
    if ((Ctx->regDBC & 0x040))  // ACK
      Ctx->regs[0x09] = set ? (Ctx->regs[0x09] | 0x40) : (Ctx->regs[0x09] & ~0x40);
    if ((Ctx->regDBC & 0x008))  // ATN
      Ctx->regs[0x09] = set ? (Ctx->regs[0x09] | 0x08) : (Ctx->regs[0x09] & ~0x08);
    Ctx->regDSP += 8;
    return Result::OKAY;
  }
  default:
    return ErrorLog("53C810: invalid I/O instruction (%02X%06X, DSP=%08X)", Ctx->regDCMD, Ctx->regDBC, Ctx->regDSP);
  }
}

// Block move
static Result SCRIPTS_BlockMove(struct NCR53C810Context *Ctx)
{
  if (!TargetMode(Ctx))
    return SCRIPTS_Invalid(Ctx);  // initiator mode block moves are not emulated

  // Without a SCSI bus, there is no initiator to transfer data with
  Ctx->waiting = true;
  return Result::OKAY;
}

//TODO: what happens if interrupt is executed in single step mode?
static Result SCRIPTS_Int_IntFly(struct NCR53C810Context *Ctx)
{
  if (!TransferConditionMet(Ctx))
  {
    Ctx->regDSP += 8;
    return Result::OKAY;
  }
  Ctx->halt = true;   // halt SCRIPTS execution
  Ctx->regISTAT |= 1; // DMA interrupt pending
  Ctx->regDSTAT |= 4; // SCRIPTS interrupt instruction received
  if (Ctx->regDIEN & 4)
    Ctx->IRQ->Assert(Ctx->scsiIRQ);
  if ((Ctx->regDBC&0x100000)) // INTFLY
    return ErrorLog("53C810 INTFLY instruction not emulated!");
  // DSP not incremented (VF3 relies on this)
  return Result::OKAY;
}

static Result SCRIPTS_MoveMemory(struct NCR53C810Context *Ctx)
{
  UINT32    src, dest;
  unsigned  numBytes, i;

  // Get operands
  src = Ctx->regDSPS;
  dest = Ctx->regTEMP = Fetch(Ctx, 8);  // word 3
  numBytes = Ctx->regDBC;
  // Not implemented: illegal instruction interrupt when src and dest are not aligned the same way

  DebugLog("53C810: Move Memory %08X -> %08X, %X\n", src, dest, numBytes);
  //if (dest==0x94000000)printf("53C810: Move Memory %08X -> %08X, %X\n", src, dest, numBytes);

  // Perform a 32-bit copy if possible
  for (i = 0; i < (numBytes/4); i++)
  {
    Ctx->Bus->Write32(dest, Ctx->Bus->Read32(src));
    dest += 4;
    src += 4;
  }

  // Finish off the last few odd bytes
  numBytes &= 3;
  while (numBytes)
  {
    Ctx->Bus->Write8(dest++, Ctx->Bus->Read8(src++));
    --numBytes;
  }

  // Update registers
  Ctx->regDBC = 0;
  Ctx->regDSPS = src;
  Ctx->regTEMP = dest;
  Ctx->regDSP += 12;

  return Result::OKAY;
}

// Invalid instruction handler
static Result SCRIPTS_Invalid(struct NCR53C810Context *Ctx)
{
  DebugLog("53C810 encountered an unrecognized instruction (%02X%06X, DSP=%08X)\n!", Ctx->regDCMD, Ctx->regDBC, Ctx->regDSP);
  return Result::FAIL;
}

// Maximum number of instructions executed when SCRIPTS are started by a
// register write
static constexpr unsigned MAX_IMMEDIATE_INSTRUCTIONS = 100;

Result C53C810::ExecuteInstruction(void)
{
  Ctx.waiting = false;

  // Fetch instruction (first two words are always fetched)
  UINT32 op = Fetch(&Ctx, 0);   // word 1
  Ctx.regDBC = op&0x00FFFFFF;
  Ctx.regDCMD = (op>>24)&0xFF;
  Ctx.regDSPS = Fetch(&Ctx, 4); // word 2

  // Execute!
  return OpTable[Ctx.regDCMD](&Ctx);
}

unsigned C53C810::Run(unsigned maxInstructions)
{
  // Run as long as the processor is started and not halted. An unrecognized
  // instruction stops execution (without halting) until SCRIPTS are started
  // again. An instruction waiting on the SCSI bus ends this run and is
  // executed again by the next one.
  unsigned numExecuted = 0;
  while (numExecuted < maxInstructions && !Ctx.halt && !Ctx.idle)
  {
    ++numExecuted;
    if (ExecuteInstruction() != Result::OKAY)
    {
      Ctx.idle = true;
      break;
    }
    if (Ctx.waiting)
      break;
  }
  return numExecuted;
}

void C53C810::SingleStep(void)
{
  // Executes even when halted (see notes at top of file)
  ExecuteInstruction();

  // Issue IRQ and finish
  Ctx.regISTAT |= 1;            // DMA interrupt pending
  Ctx.regDSTAT |= 8;            // single step interrupt
  if (Ctx.regDIEN & 8)
  {
    Ctx.IRQ->Assert(Ctx.scsiIRQ); // generate an interrupt
    DebugLog("53C810: Asserted IRQ\n");
  }
}

// Insert instructions into the LUT under control of the mask
void C53C810::Insert(UINT8 mask, UINT8 op, Result (*Handler)(struct NCR53C810Context *))
{
  UINT32  i;

  for (i = 0; i < 256; i++)
  {
    if ((i&mask) == op)
      OpTable[i] = Handler;
  }
}

void C53C810::BuildOpTable(void)
{
  Insert(0, 0, &SCRIPTS_Invalid);
  Insert(0xC0, 0x00, &SCRIPTS_BlockMove);
  Insert(0xC0, 0x40, &SCRIPTS_IO);
  Insert(0xF8, 0x68, &SCRIPTS_ReadWriteRegister);
  Insert(0xF8, 0x70, &SCRIPTS_ReadWriteRegister);
  Insert(0xF8, 0x78, &SCRIPTS_ReadWriteRegister);
  Insert(0xE0, 0x80, &SCRIPTS_TransferControl);
  Insert(0xE0, 0xC0, &SCRIPTS_MoveMemory);
  Insert(0xF8, 0x98, &SCRIPTS_Int_IntFly);
}


/******************************************************************************
 Register and PCI Access Handlers
******************************************************************************/

void C53C810::WriteRegister(unsigned reg, UINT8 data)
{
  if (reg >= 0x60)
  {
    ErrorLog("Write to invalid 53C810 register (%02X).", reg);
    return;
  }
  
  DebugLog("53C810 write: %02X=%02X (PC=%08X, LR=%08X)\n", reg, data, ppc_get_pc(), ppc_get_lr());
  
  // Dump everything into the register file
  Ctx.regs[reg&0xFF] = data;
  
  // Do something extra with the ones that we actually care about
  // TO-DO: prevent invalid/reserved/read-only registers from being written?
  switch(reg)
  {
  case 0x14:    // ISTAT
    if ((data & ISTAT_SRST))
    {
      // Software reset: operating registers return to their defaults (PCI
      // configuration is unaffected) for as long as SRST remains set
      Reset();
      Ctx.regISTAT = ISTAT_SRST;
      Ctx.regs[0x14] = ISTAT_SRST;
      Ctx.IRQ->Deassert(Ctx.scsiIRQ);
      break;
    }
    Ctx.regISTAT = (data & ~ISTAT_CON) | (Ctx.regISTAT & ISTAT_CON); // CON is read-only
    DebugLog("ISTAT=%02X\n", data);
    // SIGP aborts WAIT SELECT: let a waiting SCRIPTS processor respond
    // immediately
    if ((data & ISTAT_SIGP) && Ctx.waiting && !Ctx.halt)
      Run(MAX_IMMEDIATE_INSTRUCTIONS);
    break;
  case 0x1B:    // CTEST3
    Ctx.regs[0x1B] &= ~0x04;  // CLF (clear DMA FIFO) self-clears once FIFO is cleared, which is instantaneous here
    break;
  case 0x4F:    // STEST3
    Ctx.regs[0x4F] &= ~0x02;  // CSF (clear SCSI FIFO) self-clears once FIFO is cleared, which is instantaneous here
    break;
  case 0x1C:    // TEMP 7-0
    Ctx.regTEMP &= 0xFFFFFF00;
    Ctx.regTEMP |= data;
    break;
  case 0x1D:    // TEMP 15-8
    Ctx.regTEMP &= 0xFFFF00FF;
    Ctx.regTEMP |= (data<<8);
    break;
  case 0x1E:    // TEMP 23-16
    Ctx.regTEMP &= 0xFF00FFFF;
    Ctx.regTEMP |= (data<<16);
    break;
  case 0x1F:    // TEMP 31-24
    Ctx.regTEMP &= 0x00FFFFFF;
    Ctx.regTEMP |= (data<<24);
    break;
  case 0x24:    // DBC 7-0
    Ctx.regDBC &= 0xFFFFFF00;
    Ctx.regDBC |= data;
    break;
  case 0x25:    // DBC 15-8
    Ctx.regDBC &= 0xFFFF00FF;
    Ctx.regDBC |= (data<<8);
    break;
  case 0x26:    // DBC 23-16
    Ctx.regDBC &= 0xFF00FFFF;
    Ctx.regDBC |= (data<<16);
    break;
  case 0x27:    // DCMD
    Ctx.regDCMD = data;
    break;  
  case 0x2C:    // DSP 7-0
    Ctx.regDSP &= 0xFFFFFF00;
    Ctx.regDSP |= data;
    break;
  case 0x2D:    // DSP 15-8
    Ctx.regDSP &= 0xFFFF00FF;
    Ctx.regDSP |= (data<<8);
    break;
  case 0x2E:    // DSP 23-16
    Ctx.regDSP &= 0xFF00FFFF;
    Ctx.regDSP |= (data<<16);
    break;
  case 0x2F:    // DSP 31-24
    Ctx.regDSP &= 0x00FFFFFF;
    Ctx.regDSP |= (data<<24);
    Ctx.halt = false; // writing this register un-halts 53C810 operation (pg.6-31 of [810A])
    if (!(Ctx.regDMODE&1))  // if MAN=0, start SCRIPTS automatically
    // To-Do: is this correct? Should single step really be tested first?
    //if (!(Ctx.regDCNTL&0x10) && !(Ctx.regDMODE&1))  // if MAN=0 and not single stepping, start SCRIPTS automatically
    {
      DebugLog("53C810: Automatically starting (PC=%08X, LR=%08X, single step=%d)\n", ppc_get_pc(), ppc_get_lr(), !!(Ctx.regDCNTL&0x10));
      Ctx.idle = false;
      Run(MAX_IMMEDIATE_INSTRUCTIONS);  // automatic
    }
    break;  
  case 0x30:    // DSPS 7-0
    Ctx.regDSPS &= 0xFFFFFF00;
    Ctx.regDSPS |= data;
    break;
  case 0x31:    // DSPS 15-8
    Ctx.regDSPS &= 0xFFFF00FF;
    Ctx.regDSPS |= (data<<8);
    break;
  case 0x32:    // DSPS 23-16
    Ctx.regDSPS &= 0xFF00FFFF;
    Ctx.regDSPS |= (data<<16);
    break;
  case 0x33:    // DSPS 31-24
    Ctx.regDSPS &= 0x00FFFFFF;
    Ctx.regDSPS |= (data<<24);
    break;
  case 0x38:    // DMODE
    Ctx.regDMODE = data;
    break;
  case 0x39:    // DIEN
    Ctx.regDIEN = data;
    break;
  case 0x3B:    // DCNTL
    Ctx.regDCNTL = data;
    if ((Ctx.regDCNTL&0x14) == 0x14)    // single step
    {
      DebugLog("53C810: single step: %08X, (halt=%d)\n", Ctx.regDSP, Ctx.halt);
      SingleStep();
    }
    else if ((Ctx.regDCNTL&0x04))     // start DMA bit
    {
      DebugLog("53C810: Manually starting\n");
      Ctx.idle = false;
      Run(MAX_IMMEDIATE_INSTRUCTIONS);
    }
    break;
  default:
    break;
  }
} 

UINT8 C53C810::ReadRegister(unsigned reg)
{
  UINT8 ret;
  
  if (reg >= 0x60)
  {
    ErrorLog("Read from invalid 53C810 register (%02X).", reg);
    return 0;
  }
  
  DebugLog("53C810 read: %02X (PC=%08X, LR=%08X)\n", reg, ppc_get_pc(), ppc_get_lr());
  
  // Some registers require special handling
  switch(reg)
  {
  case 0x0C:    // DSTAT
    // For now, we don't generate stacked interrupts, so always clear IRQ status
    ret = Ctx.regDSTAT;
    //TO-DO: manual says these should be cleared here but MAME never clears them. What's up with that?
    Ctx.regISTAT &= 0xFE; // clear DIP bit (DMA interrupt)
    Ctx.regDSTAT &= 0xF7; // clear SSI (single step interrupt)
    //Ctx.regISTAT |= 1;  // doing this is another way to fix VF3
    //Ctx.regDSTAT |= 8;
    Ctx.IRQ->Deassert(Ctx.scsiIRQ);
    //DebugLog("53C810: DSTAT read\n");
    return ret;
  case 0x14:    // ISTAT
    //DebugLog("53C810: ISTAT read\n");
    return Ctx.regISTAT;
  case 0x1A:    // CTEST2
    // Bit 6 reflects SIGP, and reading clears SIGP
    if ((Ctx.regISTAT & ISTAT_SIGP))
    {
      Ctx.regISTAT &= ~ISTAT_SIGP;
      return Ctx.regs[0x1A] | 0x40;
    }
    break;
  case 0x1C:    // TEMP 7-0
    return Ctx.regTEMP&0xFF;
  case 0x1D:    // TEMP 15-8
    return (Ctx.regTEMP>>8)&0xFF;
  case 0x1E:    // TEMP 23-16
    return (Ctx.regTEMP>>16)&0xFF;
  case 0x1F:    // TEMP 31-24
    return (Ctx.regTEMP>>24)&0xFF;
  case 0x24:    // DBC 7-0
    return Ctx.regDBC&0xFF;
  case 0x25:    // DBC 15-8
    return (Ctx.regDBC>>8)&0xFF;
  case 0x26:    // DBC 23-16
    return (Ctx.regDBC>>16)&0xFF;
  case 0x27:    // DCMD
    return Ctx.regDCMD;
  case 0x2C:    // DSP 7-0
    return Ctx.regDSP&0xFF;
  case 0x2D:    // DSP 15-8
    return (Ctx.regDSP>>8)&0xFF;
  case 0x2E:    // DSP 23-16
    return (Ctx.regDSP>>16)&0xFF;
  case 0x2F:    // DSP 31-24
    return (Ctx.regDSP>>24)&0xFF;
  case 0x30:    // DSPS 7-0
    return Ctx.regDSPS&0xFF;
  case 0x31:    // DSPS 15-8
    return (Ctx.regDSPS>>8)&0xFF;
  case 0x32:    // DSPS 23-16
    return (Ctx.regDSPS>>16)&0xFF;
  case 0x33:    // DSPS 31-24
    return (Ctx.regDSPS>>24)&0xFF;
  case 0x38:    // DMODE
    return Ctx.regDMODE;
  case 0x39:    // DIEN
    return Ctx.regDIEN;
  case 0x3B:    // DCNTL
    return Ctx.regDCNTL;
  default:  // get it from the register file
    break;
  }
  
  // Register file should be up to date
  return Ctx.regs[reg&0xFF];
} 

UINT32 C53C810::ReadPCIConfigSpace(unsigned device, unsigned reg, unsigned bits, unsigned offset) const
{
  UINT32  d;
  
  if (bits == 8)
  {
    DebugLog("53C810 %d-bit PCI read request for reg=%02X\n", bits, reg);
    return 0;
  }
  
  switch (reg)
  {
  case 0x00:  // Device ID and Vendor ID
    d = FLIPENDIAN32(0x00011000); // 0x1000 = LSI Logic, 0x0001 = 53c810a
    switch (bits)
    {
    case 8:
      d >>= (3-offset)*8;         // offset will be 0-3; select appropriate byte
      d &= 0xFF;
      break;
    case 16:
      d >>= (2-offset)*8;         // offset will be 0 or 2 only; select either high or low word
      d &= 0xFFFF;
      break;
    default:
      break;
    }
    return d;
  default:
    DebugLog("53C810 PCI read request for reg=%02X (%d-bit)\n", reg, bits);
    break;
  }

  return 0;
}
  
void C53C810::WritePCIConfigSpace(unsigned device, unsigned reg, unsigned bits, unsigned offset, UINT32 data)
{
  DebugLog("53C810 PCI %d-bit write request for reg=%02X, data=%08X\n", bits, reg, data);

  if (reg == 4) // set base address of SCSI device
      baseAddress = data & 0xFF;
}

UINT8 C53C810::GetBaseAddress(void) const
{
  return baseAddress;
}

void C53C810::Reset(void)
{
  memset(Ctx.regs, 0, sizeof(Ctx.regs));
  Ctx.regs[0x00] = 0xC0;  // SCNTL0
  Ctx.regs[0x0C] = 0x80;  // DSTAT
  Ctx.regs[0x0F] = 0x02;  // SSTAT2
  Ctx.regs[0x18] = 0xFF;  // reserved
  Ctx.regs[0x19] = 0xF0;  // CTEST1
  Ctx.regs[0x1A] = 0x01;  // CTEST2
  Ctx.regs[0x46] = 0x60;  // MACNTL
  Ctx.regs[0x47] = 0x0F;  // GPCNTL
  Ctx.regs[0x4C] = 0x03;  // STEST0 
  Ctx.regTEMP = 0;
  Ctx.regDSP = 0;
  Ctx.regDSPS = 0;
  Ctx.regDBC = 0;
  Ctx.regDCMD = 0;
  Ctx.regDCNTL = 0;
  Ctx.regDMODE = 0;
  Ctx.regDSTAT = 0x80;  // DMA FIFO empty
  Ctx.regDIEN = 0;
  Ctx.regISTAT = 0;
  Ctx.halt = false;
  Ctx.idle = true;    // nothing runs until SCRIPTS are started
  Ctx.waiting = false;
  Ctx.carry = false;
  Ctx.atn = false;
  
  DebugLog("53C810 reset\n");
}


/******************************************************************************
 Configuration, Initialization, and Shutdown
******************************************************************************/

void C53C810::Init(IBus *BusObjectPtr, IIRQController *IRQObjectPtr, uint16_t scsiIRQBit)
{
  Ctx.Bus = BusObjectPtr; 
  Ctx.IRQ = IRQObjectPtr;
  Ctx.scsiIRQ = scsiIRQBit;
}

C53C810::C53C810(void)
{ 
  BuildOpTable();
  Ctx.Bus = NULL;
  Ctx.IRQ = NULL;
  scsiIRQ = 0;
  baseAddress = 0;
  DebugLog("Built 53C810\n");
}

C53C810::~C53C810(void)
{ 
  Ctx.Bus = NULL;
  Ctx.IRQ = NULL;
  DebugLog("Destroyed 53C810\n");
}
