// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
// Ported to Supermodel from MAME (src/mame/sega/model1.cpp, model1.h, model1_m.cpp).

/*
 * Model1.cpp
 *
 * Sega Model 1 system board. See Model1.h.
 *
 * Memory map (V60, 24-bit addresses):
 *
 *  000000-0FFFFF   ROM (ROMA)
 *  100000-1FFFFF   banked data ROM (ROMO, 1 MB banks of maincpu 1000000-17FFFFF)
 *  200000-2FFFFF   ROM (ROMX)
 *  400000-40FFFF   RAM A (battery backed)
 *  500000-53FFFF   RAM B
 *  600000-60FFFF   display list 0
 *  610000-61FFFF   display list 1
 *  680000-680003   display list control
 *  700000-70FFFF   tilemap RAM
 *  780000-7FFFFF   tilemap character RAM
 *  900000-903FFF   palette RAM
 *  910000-91BFFF   color translation RAM
 *  B00000-B01003   communication board (Virtua Racing, Virtua Formula, Wing War)
 *  C00000-C00FFF   dual port RAM shared with the I/O board (low byte of each word)
 *  C40000-C40003   uPD71051 UART to the sound board (low byte of each word)
 *  D00000-DDFFFF   coprocessor ports (mirrored; also in I/O space)
 *  E00000-E0000F   glue: interrupt control and mask, ROM banking, timers
 *  F80000-FFFFFF   ROM (ROM0)
 *
 * ROM regions (Games.xml), offsets as in MAME's "maincpu" region:
 *
 *  rom_low     000000      linear (only for sets that MAME reloads at 0)
 *  rom_main    200000      16-bit (two byte-wide ROMs, stride 2)
 *  rom_data    1000000     16-bit banked data
 *  rom_boot    F80000      linear
 *  tgp_program, copro_tables, copro_data (optional), polygons,
 *  sound_program, mpcm1, mpcm2
 */

#include "Model1.h"
#include "Model1SoundBoard.h"
#include "Model3/DSB.h"
#include "ROMSet.h"
#include "BlockFile.h"
#include "Inputs/Inputs.h"
#include "OSD/Logger.h"
#include "OSD/Video.h"
#include "Graphics/SuperAA.h"
#include <algorithm>
#include <cstring>

namespace
{
  constexpr int V60_TIME = 5;             // time units per V60 cycle (16 MHz)
  constexpr int TGP_TIME = 6;             // time units per TGP cycle (40 MHz / 3)
  constexpr int LINE_CYCLES = 656;        // V60 cycles per scan line (16 MHz pixel clock, 656 per line)
  constexpr int LINE_TIME = LINE_CYCLES * V60_TIME;
  constexpr int TOTAL_LINES = 424;
  constexpr int VBLANK_LINE = 384;
  constexpr int QUANTUM = 64 * V60_TIME;  // how far one CPU may run ahead of the other
  constexpr int UART_DIVIDER = 32;        // UART clock: 16 MHz / 2 / 16 = 500 kHz, a V60 cycle is 1/16 MHz

  constexpr uint32_t MAINCPU_SIZE = 0x2000000;

  inline uint16_t Combine(uint16_t old, uint16_t data, uint16_t mask)
  {
    return (old & ~mask) | (data & mask);
  }

  template <class T> void WriteVector(CBlockFile *file, const std::vector<T> &v)
  {
    file->Write(v.data(), (uint32_t) (v.size() * sizeof(T)));
  }

  template <class T> void ReadVector(CBlockFile *file, std::vector<T> &v)
  {
    file->Read(v.data(), (uint32_t) (v.size() * sizeof(T)));
  }

  inline uint32_t Combine32(uint32_t old, uint32_t data, uint32_t mask)
  {
    return (old & ~mask) | (data & mask);
  }
}


/******************************************************************************
 Coprocessor (model1_m.cpp)
******************************************************************************/

void CModel1::Halt(CPUID cpu)
{
  m_halted[cpu] = true;
}

void CModel1::Unhalt(CPUID cpu, CPUID by)
{
  if (!m_halted[cpu])
    return;
  m_halted[cpu] = false;
  // It resumes at the time of the CPU that unblocked it, which then yields so
  // that the scheduler runs the CPU that is behind
  if (by != CPU_NONE)
  {
    m_time[cpu] = std::max(m_time[cpu], m_time[by]);
    if (by == CPU_V60)
      m_v60.AbortTimeslice();
    else
      m_tgp.AbortTimeslice();
  }
}

uint32_t CModel1::FIFOPop(CFIFO &fifo, CPUID reader, CPUID writer)
{
  int which = &fifo == &m_fifoIn ? 0 : 1;
  if (fifo.values.empty())
  {
    // MAME: the reader stalls (retries), and after a sync, if the FIFO is still
    // empty, it's halted until the next push. Here the sync is the scheduler
    // running the other CPU, so the reader is halted right away.
    if (reader == CPU_V60)
      m_v60.Stall();
    else
      m_tgp.Stall();
    if (!m_emptyTriggered[which])
    {
      m_emptyTriggered[which] = true;
      Halt(reader);
    }
    return 0;
  }

  uint32_t t = fifo.values.front();
  fifo.values.pop_front();
  if (!fifo.extra.empty())
  {
    fifo.values.push_back(fifo.extra.front());
    fifo.extra.pop_front();
    if (fifo.extra.empty() && m_fullTriggered[which])
    {
      m_fullTriggered[which] = false;
      Unhalt(writer, reader);
    }
  }
  return t;
}

void CModel1::FIFOPush(CFIFO &fifo, uint32_t value, CPUID writer, CPUID reader)
{
  int which = &fifo == &m_fifoIn ? 0 : 1;
  if (!fifo.extra.empty() || fifo.values.size() >= CFIFO::SIZE)
  {
    // Overflow: kept, and the writer is halted until it drains
    fifo.extra.push_back(value);
    if (!m_fullTriggered[which])
    {
      m_fullTriggered[which] = true;
      Halt(writer);
      if (writer == CPU_V60)
        m_v60.AbortTimeslice();
      else
        m_tgp.AbortTimeslice();
    }
    return;
  }

  fifo.values.push_back(value);
  if (m_emptyTriggered[which])
  {
    m_emptyTriggered[which] = false;
    Unhalt(reader, writer);
  }
}

void CModel1::CoproReset()
{
  m_v60CoproRAMAdr = 0;
  std::fill(std::begin(m_coproRAMAdr), std::end(m_coproRAMAdr), 0);
  m_coproSinCosBase = 0;
  m_coproInvBase = 0;
  m_coproISqrtBase = 0;
  std::fill(std::begin(m_coproAtanBase), std::end(m_coproAtanBase), 0);
  std::fill(std::begin(m_v60CoproRAMLatch), std::end(m_v60CoproRAMLatch), 0);
  std::fill(m_coproRAM.begin(), m_coproRAM.end(), 0);
}

uint32_t CModel1::copro_sincos_r(unsigned offset)
{
  uint32_t ang = m_coproSinCosBase + offset * 0x4000;
  uint32_t index = ang & 0x3fff;
  if (ang & 0x4000)
    index = std::min(0x4000 - (int)index, 0x3fff);
  uint32_t result = m_coproTables[index];
  if(ang & 0x8000)
    result ^= 0x80000000;
  return result;
}

uint32_t CModel1::copro_inv_r(unsigned offset)
{
  uint32_t index = ((m_coproInvBase >> 9) & 0x3ffe) | (offset & 1);
  uint32_t result = m_coproTables[index | 0x8000];
  uint8_t bexp = (m_coproInvBase >> 23) & 0xff;
  uint8_t exp = (result >> 23) + (0x7f - bexp);
  result = (result & 0x807fffff) | (exp << 23);
  if(m_coproInvBase & 0x80000000)
    result ^= 0x80000000;
  return result;
}

uint32_t CModel1::copro_isqrt_r(unsigned offset)
{
  uint32_t index = 0x2000 ^ (((m_coproISqrtBase>> 10) & 0x3ffe) | (offset & 1));
  uint32_t result = m_coproTables[index | 0xc000];
  uint8_t bexp = (m_coproISqrtBase >> 24) & 0x7f;
  uint8_t exp = (result >> 23) + (0x3f - bexp);
  result = (result & 0x807fffff) | (exp << 23);
  if(!(offset & 1))
    result &= 0x7fffffff;
  return result;
}

uint32_t CModel1::copro_atan_r()
{
  uint32_t idx = m_coproAtanBase[3] & 0xffff;
  if(idx & 0xc000)
    idx = 0x3fff;
  uint32_t result = m_coproTables[idx | 0x4000];

  // Correct for table bug, it seems that the hardware does something equivalent somehow,
  // or maybe there are boards with updated opr roms

  uint16_t dt = (result >> 16) + result;
  if(dt & 0x001) {
    if((result & 0x00f) == 0x00e)
      result -= 0x00000001;
    else
      result -= 0x00010000;
  }
  if(dt & 0x010) {
    if((result & 0x0f0) == 0x0e0)
      result -= 0x00000010;
    else
      result -= 0x00100000;
  }
  if(dt & 0x100) {
    if((result & 0xf00) == 0xe00)
      result -= 0x00000100;
    else
      result -= 0x01000000;
  }

  bool s0 = m_coproAtanBase[0] & 0x80000000;
  bool s1 = m_coproAtanBase[1] & 0x80000000;
  bool s2 = m_coproAtanBase[2] & 0x80000000;

  if(s0 ^ s1 ^ s2)
    result >>= 16;
  if(s2)
    result += 0x4000;
  if((s0 && !s2) || (s1 && s2))
    result += 0x8000;

  return result & 0xffff;
}

uint32_t CModel1::copro_data_r(unsigned offset)
{
  if (m_coproData.empty())
    return 0;
  uint32_t index = (m_coproDataBase & ~0x7fff) | offset;
  index &= (uint32_t) m_coproData.size() - 1;
  return m_coproData[index];
}

// TGP data space: internal RAM (inside the core), FIFO in at 0x100, FIFO out at 0x400
UINT32 CModel1::ReadData(UINT16 addr)
{
  if (addr == 0x100)
    return FIFOPop(m_fifoIn, CPU_TGP, CPU_V60);
  return 0;
}

void CModel1::WriteData(UINT16 addr, UINT32 v)
{
  if (addr == 0x400)
    FIFOPush(m_fifoOut, v, CPU_TGP, CPU_V60);
}

UINT32 CModel1::ReadIO(UINT16 addr)
{
  if (addr >= 0x8000)
    return copro_data_r(addr - 0x8000);
  if ((addr & ~0x18) == 0x0000)
    return m_coproRAMAdr[(addr >> 3) & 3];
  if ((addr & ~0x18) == 0x0001)
  {
    uint32_t &adr = m_coproRAMAdr[(addr >> 3) & 3];
    uint32_t val = m_coproRAM[adr & 0x1fff];
    adr += (adr & 0x40000) ? 4 : 1;
    return val;
  }
  if (addr >= 0x20 && addr <= 0x23)
    return copro_sincos_r(addr - 0x20);
  if (addr >= 0x24 && addr <= 0x27)
    return copro_atan_r();
  if (addr >= 0x28 && addr <= 0x29)
    return copro_inv_r(addr - 0x28);
  if (addr >= 0x2a && addr <= 0x2b)
    return copro_isqrt_r(addr - 0x2a);
  return 0;
}

void CModel1::WriteIO(UINT16 addr, UINT32 v)
{
  if ((addr & ~0x18) == 0x0000)
    m_coproRAMAdr[(addr >> 3) & 3] = v;
  else if ((addr & ~0x18) == 0x0001)
  {
    // Sega Model 1 stores vertices to test in RAM at 4 bytes each (X, Y, Z, radius)
    // so a loop wanting to walk all of the X positions needs to skip 4 bytes each time.
    uint32_t &adr = m_coproRAMAdr[(addr >> 3) & 3];
    m_coproRAM[adr & 0x1fff] = v;
    adr += (adr & 0x40000) ? 4 : 1;
  }
  else if (addr >= 0x20 && addr <= 0x23)
    m_coproSinCosBase = v;
  else if (addr >= 0x24 && addr <= 0x27)
    m_coproAtanBase[addr - 0x24] = v;
  else if (addr >= 0x28 && addr <= 0x29)
    m_coproInvBase = v;
  else if (addr >= 0x2a && addr <= 0x2b)
    m_coproISqrtBase = v;
  else if (addr == 0x2e)
    m_coproDataBase = v;
}

// The V60 side of the coprocessor ports, the same in program and I/O space
bool CModel1::ReadCopro(uint32_t addr, uint16_t mask, uint16_t *data)
{
  switch (addr & 0xFE0000)
  {
  case 0xD00000:  // RAM address
    *data = m_v60CoproRAMAdr;
    return true;
  case 0xD20000:  // RAM data
  {
    unsigned offset = (addr >> 1) & 1;
    if (!offset)
      *data = (uint16_t) m_coproRAM[m_v60CoproRAMAdr & 0x1fff];
    else
    {
      *data = (uint16_t) (m_coproRAM[m_v60CoproRAMAdr & 0x1fff] >> 16);
      if(m_v60CoproRAMAdr & 0x8000)
        m_v60CoproRAMAdr++;
    }
    return true;
  }
  case 0xD80000:  // FIFO
  {
    unsigned offset = (addr >> 1) & 1;
    if (!offset)
    {
      m_v60FIFORead = FIFOPop(m_fifoOut, CPU_V60, CPU_TGP);
      *data = (uint16_t) m_v60FIFORead;
    }
    else
      *data = (uint16_t) (m_v60FIFORead >> 16);
    return true;
  }
  case 0xDC0000:  // FIFO status
    *data = 0xffff;
    return true;
  default:
    return false;
  }
}

bool CModel1::WriteCopro(uint32_t addr, uint16_t data, uint16_t mask)
{
  switch (addr & 0xFE0000)
  {
  case 0xD00000:
    m_v60CoproRAMAdr = Combine(m_v60CoproRAMAdr, data, mask);
    return true;
  case 0xD20000:
  {
    unsigned offset = (addr >> 1) & 1;
    m_v60CoproRAMLatch[offset] = Combine(m_v60CoproRAMLatch[offset], data, mask);
    if (offset)
    {
      uint32_t v = m_v60CoproRAMLatch[0] | (m_v60CoproRAMLatch[1] << 16);
      m_coproRAM[m_v60CoproRAMAdr & 0x1fff] = v;
      if(m_v60CoproRAMAdr & 0x8000)
        m_v60CoproRAMAdr++;
    }
    return true;
  }
  case 0xD80000:
  {
    unsigned offset = (addr >> 1) & 1;
    if(offset) {
      m_v60FIFOWrite = (m_v60FIFOWrite & 0x0000ffff) | (data << 16);
      FIFOPush(m_fifoIn, m_v60FIFOWrite, CPU_V60, CPU_TGP);
    } else
      m_v60FIFOWrite = (m_v60FIFOWrite & 0xffff0000) | data;
    return true;
  }
  case 0xDC0000:
    return true;
  default:
    return false;
  }
}


/******************************************************************************
 Interrupts, timers, banking (model1.cpp)
******************************************************************************/

void CModel1::IRQRaise(int level)
{
  m_irqStatus |= (1 << level);
  m_v60.SetIRQLine(true);
}

int CModel1::IRQAcknowledge()
{
  for (int i = 0; i < 8; i++)
    if (m_irqStatus & (1 << i))
    {
      m_lastIRQ = i;
      break;
    }

  return m_lastIRQ;
}

void CModel1::IRQControlWrite(uint8_t data)
{
  switch (data)
  {
  case 0x10:
    m_irqStatus = 0;
    m_v60.SetIRQLine(false);
    break;

  case 0x20:
    m_irqStatus &= ~(1 << m_lastIRQ);
    if (m_irqStatus == 0)
      m_v60.SetIRQLine(false);
    break;
  }
}

// E00002: per-level IRQ mask, active low (bit n set = IRQ level n masked).
// vf/vr unmask vblank only (and vf also level 3, its UART queue pump, while its
// sound queue is non-empty), swa unmasks levels 0/1 (level 0 is its UART pump),
// wingwar and netmerc unmask level 1 only.
void CModel1::IRQMaskWrite(uint8_t data)
{
  m_irqMask = data;
  SoundReady();
}

void CModel1::SoundReady()
{
  if ((m_uart.TxRdy() || m_uart.RxRdy()) && !(m_irqMask & 0x08))
    IRQRaise(3);
}

uint16_t CModel1::TimerRead(unsigned offset)
{
  uint16_t result = m_timerVal[offset];

  if (m_timerPeriod[offset])
  {
    result = (uint16_t) (m_timerRemaining[offset] / 0x800);
    m_timerVal[offset] = result;
  }
  return result;
}

void CModel1::TimerPeriodWrite(unsigned offset, uint16_t data, uint16_t mask)
{
  m_timerPeriod[offset] = Combine(m_timerPeriod[offset], data, mask);
  m_timerRemaining[offset] = m_timerPeriod[offset] ? int64_t(0x800) * m_timerPeriod[offset] : 0;
}

void CModel1::RunTimers(int v60Cycles)
{
  for (int i = 0; i < 2; i++)
  {
    if (!m_timerPeriod[i])
      continue;
    m_timerRemaining[i] -= v60Cycles;
    while (m_timerRemaining[i] <= 0)
    {
      if (!(m_irqMask & 0x01))
        IRQRaise(0);
      m_timerRemaining[i] += int64_t(0x800) * m_timerPeriod[i];
    }
  }
}

void CModel1::SetBank(unsigned bank)
{
  m_bank = bank & 7;
  m_v60.SetFetchRegion(0x100000, 0x100000, &m_maincpu[0x1000000 + 0x100000 * m_bank]);
}

void CModel1::BankWrite(uint16_t data, uint16_t mask)
{
  if (mask & 0x00ff)
  {
    switch(data & 0xf) {
    case 0x1: // 100000-1fffff data roms banking
      SetBank((data >> 4) & 0x7);   // netmerc has bit 0x80 set when banking, probably not a bank bit.
      break;
    case 0x2: // 200000-2fffff data roms banking (unused, all known games have only one bank)
      break;
    case 0xf: // f00000-ffffff program roms banking (unused, all known games have only one bank)
      break;
    }
  }
}


/******************************************************************************
 V60 address space
******************************************************************************/

uint16_t CModel1::ReadWord(uint32_t addr, uint16_t mask)
{
  addr &= 0xFFFFFE;
  const uint8_t *rom = nullptr;
  switch (addr >> 20)
  {
  case 0x0:
    rom = &m_maincpu[addr];
    break;
  case 0x1:
    rom = &m_maincpu[0x1000000 + 0x100000 * m_bank + (addr & 0xFFFFF)];
    break;
  case 0x2:
    rom = &m_maincpu[addr];
    break;
  case 0xF:
    if (addr >= 0xF80000)
      rom = &m_maincpu[addr];
    break;
  case 0x4:
    if (addr < 0x410000)
      return m_ramA[(addr & 0xFFFF) >> 1];
    break;
  case 0x5:
    if (addr < 0x540000)
      return m_ramB[(addr & 0x3FFFF) >> 1];
    break;
  case 0x6:
    if (addr < 0x610000)
      return m_displayList0[(addr & 0xFFFF) >> 1];
    if (addr < 0x620000)
      return m_displayList1[(addr & 0xFFFF) >> 1];
    if (addr >= 0x680000 && addr < 0x680004)
      return m_geometry.ReadListCtl((addr >> 1) & 1);
    break;
  case 0x7:
    if (addr < 0x710000)
      return m_tiles.ReadTile((addr & 0xFFFF) >> 1);
    if (addr >= 0x780000)
      return m_tiles.ReadChar((addr & 0x7FFFF) >> 1);
    break;
  case 0x9:
    if (addr < 0x904000)
      return m_paletteRAM[(addr & 0x3FFF) >> 1];
    if (addr >= 0x910000 && addr < 0x91C000)
      return m_colorXlat[(addr - 0x910000) >> 1];
    break;
  case 0xB:
    // Communication board shared memory: as RAM (the board isn't emulated)
    if (addr < 0xB01004 && (m_game.name == "vr" || m_game.name == "vformula" || m_game.parent == "vr" || m_game.name.compare(0, 7, "wingwar") == 0))
      return m_commRAM[(addr & 0x1FFF) >> 1];
    break;
  case 0xC:
    if (addr < 0xC01000)
    {
      // I/O board dual port RAM, low byte of each word. MAME inserts a wait
      // state (one cycle) on reads.
      if (m_logIO && ((addr & 0xFFF) >> 1) >= 0x0B)
        DebugLog("DPRAM read  %03X = %02X (PC=%06X)\n", (addr & 0xFFF) >> 1, m_dpram[(addr & 0xFFF) >> 1], m_v60.GetPPC());
      return 0xFF00 | m_dpram[(addr & 0xFFF) >> 1];
    }
    if (addr >= 0xC40000 && addr < 0xC40004)
      return 0xFF00 | m_uart.Read((addr >> 1) & 1);
    break;
  case 0xD:
  {
    uint16_t data;
    if (ReadCopro(addr, mask, &data))
      return data;
    break;
  }
  case 0xE:
    if (addr == 0xE00002)
      return 0xFF00 | m_irqMask;
    if (addr >= 0xE0000C && addr < 0xE00010)
      return TimerRead((addr >> 1) & 1);
    break;
  default:
    break;
  }

  if (rom)
    return rom[0] | (rom[1] << 8);
  return 0xFFFF;
}

void CModel1::WriteWord(uint32_t addr, uint16_t data, uint16_t mask)
{
  addr &= 0xFFFFFE;
  switch (addr >> 20)
  {
  case 0x4:
    if (addr < 0x410000)
    {
      uint16_t &w = m_ramA[(addr & 0xFFFF) >> 1];
      w = Combine(w, data, mask);
    }
    return;
  case 0x5:
    if (addr < 0x540000)
    {
      uint16_t &w = m_ramB[(addr & 0x3FFFF) >> 1];
      w = Combine(w, data, mask);
    }
    return;
  case 0x6:
    if (addr < 0x610000)
    {
      uint16_t &w = m_displayList0[(addr & 0xFFFF) >> 1];
      w = Combine(w, data, mask);
    }
    else if (addr < 0x620000)
    {
      uint16_t &w = m_displayList1[(addr & 0xFFFF) >> 1];
      w = Combine(w, data, mask);
    }
    else if (addr >= 0x680000 && addr < 0x680004)
      m_geometry.WriteListCtl((addr >> 1) & 1, data, mask);
    return;
  case 0x7:
    if (addr < 0x710000)
      m_tiles.WriteTile((addr & 0xFFFF) >> 1, data, mask);
    else if (addr >= 0x780000)
      m_tiles.WriteChar((addr & 0x7FFFF) >> 1, data, mask);
    return;
  case 0x9:
    if (addr < 0x904000)
    {
      unsigned i = (addr & 0x3FFF) >> 1;
      m_paletteRAM[i] = Combine(m_paletteRAM[i], data, mask);
      m_palette[i] = 0xFF000000 | Model1::CGeometry::PaletteToRGB(m_paletteRAM[i]);
    }
    else if (addr >= 0x910000 && addr < 0x91C000)
    {
      uint16_t &w = m_colorXlat[(addr - 0x910000) >> 1];
      w = Combine(w, data, mask);
    }
    return;
  case 0xB:
    if (addr < 0xB01004)
    {
      uint16_t &w = m_commRAM[(addr & 0x1FFF) >> 1];
      w = Combine(w, data, mask);
    }
    return;
  case 0xC:
    if (addr < 0xC01000)
    {
      if (mask & 0x00FF)
      {
        if (m_logIO)
          DebugLog("DPRAM write %03X = %02X (PC=%06X)\n", (addr & 0xFFF) >> 1, data & 0xFF, m_v60.GetPPC());
        m_dpram[(addr & 0xFFF) >> 1] = (uint8_t) data;
      }
    }
    else if (addr >= 0xC40000 && addr < 0xC40004)
    {
      if (mask & 0x00FF)
      {
        m_uart.Write((addr >> 1) & 1, (uint8_t) data);
        SoundReady();
      }
    }
    return;
  case 0xD:
    WriteCopro(addr, data, mask);
    return;
  case 0xE:
    if (addr == 0xE00000)
    {
      if (mask & 0x00FF)
        IRQControlWrite((uint8_t) data);
    }
    else if (addr == 0xE00002)
    {
      if (mask & 0x00FF)
        IRQMaskWrite((uint8_t) data);
    }
    else if (addr == 0xE00004)
      BankWrite(data, mask);
    else if (addr == 0xE00006)
      m_timerMode = Combine(m_timerMode, data, mask);
    else if (addr >= 0xE00008 && addr < 0xE0000C)
      TimerPeriodWrite((addr >> 1) & 1, data, mask);
    return;
  default:
    return;
  }
}

UINT8 CModel1::Read8(UINT32 addr)
{
  if (addr & 1)
    return (UINT8) (ReadWord(addr, 0xFF00) >> 8);
  return (UINT8) ReadWord(addr, 0x00FF);
}

UINT16 CModel1::Read16(UINT32 addr)
{
  return ReadWord(addr, 0xFFFF);
}

void CModel1::Write8(UINT32 addr, UINT8 data)
{
  if (addr & 1)
    WriteWord(addr, data << 8, 0xFF00);
  else
    WriteWord(addr, data, 0x00FF);
}

void CModel1::Write16(UINT32 addr, UINT16 data)
{
  WriteWord(addr, data, 0xFFFF);
}

// I/O space: only the coprocessor ports (MAME's model1_io map)
UINT8 CModel1::CIOBus::Read8(UINT32 addr)
{
  uint16_t data = 0xFFFF;
  uint16_t mask = (addr & 1) ? 0xFF00 : 0x00FF;
  board->ReadCopro(addr & 0xFFFFFE, mask, &data);
  return (UINT8) ((addr & 1) ? data >> 8 : data);
}

UINT16 CModel1::CIOBus::Read16(UINT32 addr)
{
  uint16_t data = 0xFFFF;
  board->ReadCopro(addr & 0xFFFFFE, 0xFFFF, &data);
  return data;
}

void CModel1::CIOBus::Write8(UINT32 addr, UINT8 data)
{
  if (addr & 1)
    board->WriteCopro(addr & 0xFFFFFE, data << 8, 0xFF00);
  else
    board->WriteCopro(addr & 0xFFFFFE, data, 0x00FF);
}

void CModel1::CIOBus::Write16(UINT32 addr, UINT16 data)
{
  board->WriteCopro(addr & 0xFFFFFE, data, 0xFFFF);
}


/******************************************************************************
 Inputs (I/O board simulation)
******************************************************************************/

// The I/O board BIOS copies the analog channels to dual port RAM 0-7 and the
// three digital ports (active low) to 8-10
void CModel1::UpdateInputs(void)
{
  if (!m_inputs)
    return;

  auto sw = [](const std::shared_ptr<CSwitchInput> &in) -> bool { return in && in->value; };
  uint8_t in0 = 0, in1 = 0, in2 = 0;
  uint8_t an[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  const uint32_t inputs = m_game.inputs;

  if (inputs & Game::INPUT_COMMON)
  {
    in0 |= sw(m_inputs->coin[0]) ? 0x01 : 0;
    in0 |= sw(m_inputs->coin[1]) ? 0x02 : 0;
    in0 |= sw(m_inputs->test[0]) ? 0x04 : 0;
    in0 |= sw(m_inputs->service[0]) ? 0x08 : 0;
    in0 |= sw(m_inputs->start[0]) ? 0x10 : 0;
  }

  if (inputs & Game::INPUT_FIGHTING)
  {
    // Virtua Fighter: start 2, and punch/kick/guard + joystick for each player
    // (the buttons in the same bit order as Virtua Fighter 3 on Model 3)
    in0 |= sw(m_inputs->start[1]) ? 0x20 : 0;
    uint8_t *port[2] = { &in1, &in2 };
    for (int p = 0; p < 2; p++)
    {
      uint8_t &v = *port[p];
      v |= sw(m_inputs->punch[p]) ? 0x01 : 0;
      v |= sw(m_inputs->kick[p]) ? 0x02 : 0;
      v |= sw(m_inputs->guard[p]) ? 0x04 : 0;
      v |= sw(m_inputs->down[p]) ? 0x10 : 0;
      v |= sw(m_inputs->up[p]) ? 0x20 : 0;
      v |= sw(m_inputs->right[p]) ? 0x40 : 0;
      v |= sw(m_inputs->left[p]) ? 0x80 : 0;
    }
  }

  if (inputs & Game::INPUT_VEHICLE)
  {
    // Virtua Racing / Virtua Formula: wheel, accelerator, brake
    an[0] = m_inputs->steering ? (uint8_t) m_inputs->steering->value : 0x80;
    an[1] = m_inputs->accelerator ? (uint8_t) std::max<unsigned>(1, m_inputs->accelerator->value) : 0x30;
    an[2] = m_inputs->brake ? (uint8_t) std::max<unsigned>(1, m_inputs->brake->value) : 0x30;
  }

  // Virtua Racing's VR buttons, Wing War's view buttons
  if (inputs & Game::INPUT_VR4)
  {
    in0 |= sw(m_inputs->vr[0]) ? 0x20 : 0;
    in0 |= sw(m_inputs->vr[1]) ? 0x40 : 0;
    in0 |= sw(m_inputs->vr[2]) ? 0x80 : 0;
    in1 |= sw(m_inputs->vr[3]) ? 0x01 : 0;
  }

  if (inputs & Game::INPUT_SHIFTUPDOWN)
  {
    in1 |= sw(m_inputs->gearShiftDown) ? 0x10 : 0;
    in1 |= sw(m_inputs->gearShiftUp) ? 0x20 : 0;
  }

  if (inputs & Game::INPUT_ANALOG_JOYSTICK)
  {
    // Star Wars Arcade and Wing War: flight stick (X reversed) and throttle
    uint8_t x = m_inputs->analogJoyX ? (uint8_t) m_inputs->analogJoyX->value : 0x80;
    uint8_t y = m_inputs->analogJoyY ? (uint8_t) m_inputs->analogJoyY->value : 0x80;
    an[0] = 0xFF - x;
    an[1] = y;
    if (m_game.name.compare(0, 7, "wingwar") == 0)
    {
      // throttle (1-0xFF, idle at 1), machine gun, missile, smoke
      an[2] = m_inputs->accelerator ? (uint8_t) std::max<unsigned>(1, m_inputs->accelerator->value) : 1;
      in1 |= sw(m_inputs->analogJoyTrigger1) ? 0x10 : 0;
      in1 |= sw(m_inputs->analogJoyTrigger2) ? 0x20 : 0;
      in1 |= sw(m_inputs->analogJoyEvent1) ? 0x40 : 0;
    }
    else
    {
      // Star Wars Arcade (MAME's swa ports): pilot stick 27-227 (X reversed),
      // throttle 28-200 (reversed), pilot buttons 1-3. The gunner's stick
      // (channels 4 and 5) has no Supermodel input and stays centered.
      an[0] = (uint8_t) (227 - x * 200 / 255);
      an[1] = (uint8_t) (27 + y * 200 / 255);
      unsigned throttle = m_inputs->accelerator ? m_inputs->accelerator->value : 0;
      an[2] = (uint8_t) (200 - throttle * 172 / 255);
      an[4] = 0x7F;
      an[5] = 0x7F;
      in0 |= sw(m_inputs->start[1]) ? 0x20 : 0;
      in1 |= sw(m_inputs->analogJoyTrigger1) ? 0x01 : 0;
      in1 |= sw(m_inputs->analogJoyTrigger2) ? 0x02 : 0;
      in1 |= sw(m_inputs->analogJoyEvent1) ? 0x10 : 0;
    }
  }

  for (int i = 0; i < 8; i++)
    m_dpram[i] = an[i];
  m_dpram[8] = (uint8_t) ~in0;
  m_dpram[9] = (uint8_t) ~in1;
  m_dpram[10] = (uint8_t) ~in2;

  // After a reset, the real board's Z80 BIOS takes a while before it reports
  // ready. Games depend on it: the first thing Virtua Fighter does after the
  // I/O board check is send a command to the sound board, and if that arrives
  // before the sound program is ready (about 5 frames after reset) half of it
  // is lost and the sound program ignores everything after it.
  if (m_ioBoardBootFrames > 0)
  {
    m_ioBoardBootFrames--;
    m_dpram[0x21] = 0;
    return;
  }

  // Commands: the game waits for 21 to read 0x40 (board ready), clears it,
  // writes a command to 20 and waits for the board to clear 20. Seen: 1 (with
  // "SEGA" at 1A-1D, a presence check) and 3 (the EEPROM settings to 100-17F,
  // which are kept there and saved with the NVRAM). Each is done here at once.
  if (m_dpram[0x20])
  {
    if (m_logIO)
      DebugLog("I/O board command %02X (%02X %02X %02X %02X)\n", m_dpram[0x20], m_dpram[0x1A], m_dpram[0x1B], m_dpram[0x1C], m_dpram[0x1D]);
    m_dpram[0x20] = 0;
  }
  m_dpram[0x21] = 0x40;
}


/******************************************************************************
 Emulation
******************************************************************************/

// Runs one scan line of both CPUs. The CPU that is behind runs next, at most
// QUANTUM time units past the other one. Halted CPUs (blocked on a FIFO) don't
// run; when unblocked, they continue from the time of the CPU that unblocked
// them.
void CModel1::RunLine(void)
{
  m_lineEnd += LINE_TIME;

  for (int guard = 0; guard < 100000; guard++)
  {
    bool ready[2];
    for (int i = 0; i < 2; i++)
      ready[i] = !m_halted[i] && m_time[i] < m_lineEnd;
    if (!ready[CPU_V60] && !ready[CPU_TGP])
      break;

    CPUID cpu = (ready[CPU_V60] && (!ready[CPU_TGP] || m_time[CPU_V60] <= m_time[CPU_TGP])) ? CPU_V60 : CPU_TGP;
    CPUID other = cpu == CPU_V60 ? CPU_TGP : CPU_V60;
    int64_t limit = m_lineEnd;
    if (ready[other])
      limit = std::min(limit, m_time[other] + QUANTUM);

    m_running = cpu;
    if (cpu == CPU_V60)
    {
      int cycles = (int) std::max<int64_t>(1, (limit - m_time[cpu] + V60_TIME - 1) / V60_TIME);
      int done = m_v60.Run(cycles);
      m_time[cpu] += int64_t(std::max(done, 1)) * V60_TIME;
    }
    else
    {
      int cycles = (int) std::max<int64_t>(1, (limit - m_time[cpu] + TGP_TIME - 1) / TGP_TIME);
      int done = m_tgp.Run(cycles);
      m_time[cpu] += int64_t(std::max(done, 1)) * TGP_TIME;
    }
    m_running = CPU_NONE;
  }

  // Halted CPUs idle until the end of the line
  for (int i = 0; i < 2; i++)
    if (m_halted[i])
      m_time[i] = std::max(m_time[i], m_lineEnd);

  RunTimers(LINE_CYCLES);

  // UART clock
  m_uartClockFrac += LINE_CYCLES;
  unsigned clocks = m_uartClockFrac / UART_DIVIDER;
  m_uartClockFrac %= UART_DIVIDER;
  if (clocks)
    m_uart.Clock(clocks);
}

void CModel1::BuildVideoFrame(void)
{
  using namespace Model1;
  const size_t pixels = size_t(SCREEN_WIDTH) * SCREEN_HEIGHT;
  m_video.valid = true;
  m_video.software = m_softwareRender;

  m_geometry.Render(&m_video.geometry, m_frameNumber);

  // Debugging aid: log the quads whose bounding box touches a native rectangle "x1,y1,x2,y2"
  if (const char *rect = getenv("SUPERMODEL_MODEL1_QUADLOG"))
  {
    int rx1, ry1, rx2, ry2;
    if (sscanf(rect, "%d,%d,%d,%d", &rx1, &ry1, &rx2, &ry2) == 4)
    {
      DebugLog("Quads at %d,%d-%d,%d, frame %d:\n", rx1, ry1, rx2, ry2, m_framesRun);
      for (size_t b = 0; b < m_video.geometry.below.size(); b++)
      {
        const Batch &batch = m_video.geometry.below[b];
        for (uint32_t i = 0; i < batch.count; i++)
        {
          const ScreenQuad &q = m_video.geometry.quads[batch.first + i];
          float minx = std::min({ q.x[0], q.x[1], q.x[2], q.x[3] }), maxx = std::max({ q.x[0], q.x[1], q.x[2], q.x[3] });
          float miny = std::min({ q.y[0], q.y[1], q.y[2], q.y[3] }), maxy = std::max({ q.y[0], q.y[1], q.y[2], q.y[3] });
          if (maxx < rx1 || minx > rx2 || maxy < ry1 || miny > ry2)
            continue;
          DebugLog("  b%u #%4u z=%10.3f zmode=%u col=%06X flags=%08X adr=%06X box=%.1f,%.1f-%.1f,%.1f\n", (unsigned) b, i, q.z, (q.flags >> 10) & 3,
            q.color & 0xFFFFFF, q.flags, q.adr, minx, miny, maxx, maxy);
        }
      }
    }
  }

  // Tilemaps below the 3D: the backdrop (pen 0), tilemap B opaque, then A
  m_video.back.assign(pixels, m_palette[0]);
  m_tiles.DrawLayer(6, m_video.back.data(), SCREEN_WIDTH, SCREEN_WIDTH, SCREEN_HEIGHT, true, m_palette.data());
  m_tiles.DrawLayer(4, m_video.back.data(), SCREEN_WIDTH, SCREEN_WIDTH, SCREEN_HEIGHT, true, m_palette.data());
  m_tiles.DrawLayer(2, m_video.back.data(), SCREEN_WIDTH, SCREEN_WIDTH, SCREEN_HEIGHT, false, m_palette.data());
  m_tiles.DrawLayer(0, m_video.back.data(), SCREEN_WIDTH, SCREEN_WIDTH, SCREEN_HEIGHT, false, m_palette.data());

  // HUD tilemaps, on transparent
  m_video.hud.assign(pixels, 0);
  m_tiles.DrawLayer(7, m_video.hud.data(), SCREEN_WIDTH, SCREEN_WIDTH, SCREEN_HEIGHT, false, m_palette.data());
  m_tiles.DrawLayer(5, m_video.hud.data(), SCREEN_WIDTH, SCREEN_WIDTH, SCREEN_HEIGHT, false, m_palette.data());
  m_tiles.DrawLayer(3, m_video.hud.data(), SCREEN_WIDTH, SCREEN_WIDTH, SCREEN_HEIGHT, false, m_palette.data());
  m_tiles.DrawLayer(1, m_video.hud.data(), SCREEN_WIDTH, SCREEN_WIDTH, SCREEN_HEIGHT, false, m_palette.data());

  if (m_softwareRender)
  {
    // MAME's screen_update_model1(), exactly
    std::vector<uint32_t> &img = m_video.softwareImage;
    img = m_video.back;
    CGeometry::Rasterize(m_video.geometry, m_video.geometry.below, img.data());
    for (size_t i = 0; i < pixels; i++)
      if (m_video.hud[i] & 0xFF000000)
        img[i] = m_video.hud[i];
    if (!m_video.geometry.above.empty())
    {
      std::vector<uint32_t> snapshot(img);
      CGeometry::Rasterize(m_video.geometry, m_video.geometry.above, img.data());
      for (size_t i = 0; i < pixels; i++)
      {
        uint32_t c = snapshot[i];
        if (((c >> 16) & 0xFF) > 8 || ((c >> 8) & 0xFF) > 8 || (c & 0xFF) > 8)
          img[i] = c;
      }
    }
    for (auto &c : img)
      c = 0xFF000000 | (c & 0xFFFFFF);
  }
}

void CModel1::RunFrame(void)
{
  // Debugging aid: stop after one frame (e.g. to compare renderers on one frame of a save state)
  if (m_freezeAfterFrames >= 0 && m_framesRun >= m_freezeAfterFrames)
  {
    const char *dump = getenv("SUPERMODEL_MODEL1_DUMP");
    if (dump && m_render && !m_dumped)
    {
      m_render->RequestDump(dump);
      m_dumped = true;
    }
    RenderFrame();
    return;
  }
  m_framesRun++;

  // Debugging aid: write each of the first frames after start or a state load to <prefix>_<n>.bmp
  if (const char *prefix = getenv("SUPERMODEL_MODEL1_DUMP_SEQUENCE"))
  {
    int count = getenv("SUPERMODEL_MODEL1_DUMP_COUNT") ? atoi(getenv("SUPERMODEL_MODEL1_DUMP_COUNT")) : 30;
    if (m_render && m_framesRun <= count)
    {
      char file[1024];
      snprintf(file, sizeof(file), "%s_%03d.bmp", prefix, m_framesRun);
      m_render->RequestDump(file);
    }
  }

  UpdateInputs();

  for (m_line = 0; m_line < TOTAL_LINES; m_line++)
  {
    if (m_line == VBLANK_LINE)
    {
      // Start of vertical blanking: the video board scans the list, and the
      // vblank interrupt
      m_geometry.VBlank(m_frameNumber);
      if (!(m_irqMask & 0x02))
        IRQRaise(1);
    }
    RunLine();
  }

  // The screen is drawn at the end of vertical blanking (MAME's
  // VIDEO_UPDATE_AFTER_VBLANK)
  BuildVideoFrame();
  m_frameNumber++;

  if (m_logIO && (m_frameNumber % 60) == 0)
    DebugLog("Model 1 frame %llu: V60 PC=%06X%s TGP PC=%04X%s FIFO in=%u+%u out=%u+%u IRQ mask=%02X status=%02X listctl=%04X/%04X quads=%u\n",
      (unsigned long long) m_frameNumber, m_v60.GetPC(), m_halted[CPU_V60] ? " (halted)" : "", m_tgp.GetPC(), m_halted[CPU_TGP] ? " (halted)" : "",
      (unsigned) m_fifoIn.values.size(), (unsigned) m_fifoIn.extra.size(), (unsigned) m_fifoOut.values.size(), (unsigned) m_fifoOut.extra.size(),
      m_irqMask, m_irqStatus, m_geometry.ReadListCtl(0), m_geometry.ReadListCtl(1), (unsigned) m_video.geometry.quads.size());

  // Debugging aid: "frame:hexbytes" sends bytes to the sound board at that frame
  if (const char *inject = getenv("SUPERMODEL_MODEL1_SNDINJECT"))
  {
    unsigned frame = 0;
    char hex[256] = { 0 };
    if (m_soundBoard && sscanf(inject, "%u:%255s", &frame, hex) == 2 && frame == (unsigned) m_frameNumber)
    {
      for (size_t i = 0; hex[i] && hex[i + 1]; i += 2)
      {
        unsigned b;
        sscanf(&hex[i], "%2x", &b);
        m_soundBoard->WriteMIDI((UINT8) b);
      }
      DebugLog("Injected sound bytes %s at frame %u\n", hex, frame);
    }
  }

  if (m_soundBoard)
    m_soundBoard->RunFrame();

  RenderFrame();
}

void CModel1::RenderFrame(void)
{
  if (BeginFrameVideo() && m_render)
  {
    m_render->DrawFrame(m_video);
    if (m_superAA)
      m_superAA->Draw();
  }
  EndFrameVideo();
}

void CModel1::Reset(void)
{
  SetBank(0);
  m_irqStatus = 0;
  m_lastIRQ = 0;
  m_irqMask = 0xFF;
  m_v60.SetIRQLine(false);
  m_uart.Reset();
  m_uart.WriteCTS(0);
  CoproReset();

  m_fifoIn.values.clear();
  m_fifoIn.extra.clear();
  m_fifoOut.values.clear();
  m_fifoOut.extra.clear();
  for (int i = 0; i < 2; i++)
  {
    m_halted[i] = false;
    m_emptyTriggered[i] = false;
    m_fullTriggered[i] = false;
    m_timerPeriod[i] = 0;
    m_timerVal[i] = 0;
    m_timerRemaining[i] = 0;
    m_time[i] = 0;
  }
  m_timerMode = 0;
  m_lineEnd = 0;
  m_uartClockFrac = 0;
  m_ioBoardBootFrames = IO_BOARD_BOOT_FRAMES;

  m_v60.Reset();
  m_tgp.Reset();
  m_geometry.Reset();
  m_tiles.Reset();
  if (m_soundBoard)
    m_soundBoard->Reset();

  DebugLog("Model 1 reset\n");
}


/******************************************************************************
 Save states and NVRAM
******************************************************************************/

void CModel1::SaveState(CBlockFile *SaveState)
{
  SaveState->NewBlock("Model 1", __FILE__);
  WriteVector(SaveState, m_ramA);
  WriteVector(SaveState, m_ramB);
  WriteVector(SaveState, m_displayList0);
  WriteVector(SaveState, m_displayList1);
  WriteVector(SaveState, m_paletteRAM);
  WriteVector(SaveState, m_colorXlat);
  WriteVector(SaveState, m_dpram);
  WriteVector(SaveState, m_commRAM);
  SaveState->Write(&m_bank, sizeof(m_bank));

  // The FIFOs (MAME doesn't save them, and hopes they are empty)
  for (CFIFO *fifo : { &m_fifoIn, &m_fifoOut })
  {
    uint32_t n = (uint32_t) fifo->values.size();
    SaveState->Write(&n, sizeof(n));
    for (uint32_t v : fifo->values)
      SaveState->Write(&v, sizeof(v));
    n = (uint32_t) fifo->extra.size();
    SaveState->Write(&n, sizeof(n));
    for (uint32_t v : fifo->extra)
      SaveState->Write(&v, sizeof(v));
  }
  SaveState->Write(m_halted, sizeof(m_halted));
  SaveState->Write(m_emptyTriggered, sizeof(m_emptyTriggered));
  SaveState->Write(m_fullTriggered, sizeof(m_fullTriggered));
  WriteVector(SaveState, m_coproRAM);
  SaveState->Write(&m_v60CoproRAMAdr, sizeof(m_v60CoproRAMAdr));
  SaveState->Write(m_v60CoproRAMLatch, sizeof(m_v60CoproRAMLatch));
  SaveState->Write(&m_v60FIFORead, sizeof(m_v60FIFORead));
  SaveState->Write(&m_v60FIFOWrite, sizeof(m_v60FIFOWrite));
  SaveState->Write(&m_coproSinCosBase, sizeof(m_coproSinCosBase));
  SaveState->Write(&m_coproInvBase, sizeof(m_coproInvBase));
  SaveState->Write(&m_coproISqrtBase, sizeof(m_coproISqrtBase));
  SaveState->Write(m_coproAtanBase, sizeof(m_coproAtanBase));
  SaveState->Write(&m_coproDataBase, sizeof(m_coproDataBase));
  SaveState->Write(m_coproRAMAdr, sizeof(m_coproRAMAdr));

  SaveState->Write(&m_irqStatus, sizeof(m_irqStatus));
  SaveState->Write(&m_irqMask, sizeof(m_irqMask));
  SaveState->Write(&m_lastIRQ, sizeof(m_lastIRQ));
  SaveState->Write(m_timerPeriod, sizeof(m_timerPeriod));
  SaveState->Write(m_timerVal, sizeof(m_timerVal));
  SaveState->Write(&m_timerMode, sizeof(m_timerMode));
  SaveState->Write(m_timerRemaining, sizeof(m_timerRemaining));
  SaveState->Write(m_time, sizeof(m_time));
  SaveState->Write(&m_lineEnd, sizeof(m_lineEnd));
  SaveState->Write(&m_uartClockFrac, sizeof(m_uartClockFrac));
  SaveState->Write(&m_frameNumber, sizeof(m_frameNumber));

  m_v60.SaveState(SaveState, "Model 1 V60");
  m_tgp.SaveState(SaveState, "Model 1 TGP");
  m_geometry.SaveState(SaveState);
  m_tiles.SaveState(SaveState);
  m_uart.SaveState(SaveState, "Model 1 UART");
  if (m_soundBoard)
    m_soundBoard->SaveState(SaveState);
}

void CModel1::LoadState(CBlockFile *SaveState)
{
  if (Result::OKAY != SaveState->FindBlock("Model 1"))
  {
    ErrorLog("Unable to load Model 1 state. Save state file is corrupt.");
    return;
  }
  ReadVector(SaveState, m_ramA);
  ReadVector(SaveState, m_ramB);
  ReadVector(SaveState, m_displayList0);
  ReadVector(SaveState, m_displayList1);
  ReadVector(SaveState, m_paletteRAM);
  ReadVector(SaveState, m_colorXlat);
  ReadVector(SaveState, m_dpram);
  ReadVector(SaveState, m_commRAM);
  unsigned bank = 0;
  SaveState->Read(&bank, sizeof(bank));
  SetBank(bank);

  for (CFIFO *fifo : { &m_fifoIn, &m_fifoOut })
  {
    uint32_t n = 0, v = 0;
    fifo->values.clear();
    fifo->extra.clear();
    SaveState->Read(&n, sizeof(n));
    for (uint32_t i = 0; i < n && i < 0x10000; i++)
    {
      SaveState->Read(&v, sizeof(v));
      fifo->values.push_back(v);
    }
    SaveState->Read(&n, sizeof(n));
    for (uint32_t i = 0; i < n && i < 0x10000; i++)
    {
      SaveState->Read(&v, sizeof(v));
      fifo->extra.push_back(v);
    }
  }
  SaveState->Read(m_halted, sizeof(m_halted));
  SaveState->Read(m_emptyTriggered, sizeof(m_emptyTriggered));
  SaveState->Read(m_fullTriggered, sizeof(m_fullTriggered));
  ReadVector(SaveState, m_coproRAM);
  SaveState->Read(&m_v60CoproRAMAdr, sizeof(m_v60CoproRAMAdr));
  SaveState->Read(m_v60CoproRAMLatch, sizeof(m_v60CoproRAMLatch));
  SaveState->Read(&m_v60FIFORead, sizeof(m_v60FIFORead));
  SaveState->Read(&m_v60FIFOWrite, sizeof(m_v60FIFOWrite));
  SaveState->Read(&m_coproSinCosBase, sizeof(m_coproSinCosBase));
  SaveState->Read(&m_coproInvBase, sizeof(m_coproInvBase));
  SaveState->Read(&m_coproISqrtBase, sizeof(m_coproISqrtBase));
  SaveState->Read(m_coproAtanBase, sizeof(m_coproAtanBase));
  SaveState->Read(&m_coproDataBase, sizeof(m_coproDataBase));
  SaveState->Read(m_coproRAMAdr, sizeof(m_coproRAMAdr));

  SaveState->Read(&m_irqStatus, sizeof(m_irqStatus));
  SaveState->Read(&m_irqMask, sizeof(m_irqMask));
  SaveState->Read(&m_lastIRQ, sizeof(m_lastIRQ));
  SaveState->Read(m_timerPeriod, sizeof(m_timerPeriod));
  SaveState->Read(m_timerVal, sizeof(m_timerVal));
  SaveState->Read(&m_timerMode, sizeof(m_timerMode));
  SaveState->Read(m_timerRemaining, sizeof(m_timerRemaining));
  SaveState->Read(m_time, sizeof(m_time));
  SaveState->Read(&m_lineEnd, sizeof(m_lineEnd));
  SaveState->Read(&m_uartClockFrac, sizeof(m_uartClockFrac));
  SaveState->Read(&m_frameNumber, sizeof(m_frameNumber));

  for (size_t i = 0; i < m_paletteRAM.size(); i++)
    m_palette[i] = 0xFF000000 | Model1::CGeometry::PaletteToRGB(m_paletteRAM[i]);
  m_framesRun = 0;
  m_ioBoardBootFrames = 0;

  m_v60.LoadState(SaveState, "Model 1 V60");
  m_tgp.LoadState(SaveState, "Model 1 TGP");
  m_geometry.LoadState(SaveState);
  m_tiles.LoadState(SaveState);
  m_uart.LoadState(SaveState, "Model 1 UART");
  if (m_soundBoard)
    m_soundBoard->LoadState(SaveState);
}

// NVRAM: RAM A (battery backed) and the dual port RAM area where the I/O
// board keeps its EEPROM settings (MAME 0.190's "nvram" share at C00200)
void CModel1::SaveNVRAM(CBlockFile *NVRAM)
{
  NVRAM->NewBlock("Model 1 NVRAM", __FILE__);
  WriteVector(NVRAM, m_ramA);
  NVRAM->Write(&m_dpram[0x100], 0x80);
}

void CModel1::LoadNVRAM(CBlockFile *NVRAM)
{
  if (Result::OKAY != NVRAM->FindBlock("Model 1 NVRAM"))
  {
    ErrorLog("Unable to load Model 1 NVRAM. Save file is corrupt.");
    return;
  }
  ReadVector(NVRAM, m_ramA);
  NVRAM->Read(&m_dpram[0x100], 0x80);
}

void CModel1::ClearNVRAM(void)
{
  // vf (at least) depends on default being 1-filled for ranking to initialize properly
  std::fill(m_ramA.begin(), m_ramA.end(), 0xFFFF);
  std::fill(m_dpram.begin() + 0x100, m_dpram.begin() + 0x180, 0xFF);
}


/******************************************************************************
 Initialization
******************************************************************************/

void CModel1::AttachRenderers(IRender2D *Render2DPtr, IRender3D *Render3DPtr, SuperAA *superAA)
{
  m_render = dynamic_cast<CModel1Render *>(Render2DPtr);
  m_superAA = superAA;
  if (!m_render)
    ErrorLog("Model 1: the 2D renderer is not a Model 1 renderer. Nothing will be drawn.");
  else
  {
    float expand = m_config["Model1EdgeExpand"].ValueAsDefault<float>(0.5f);
    bool crackFill = m_config["Model1CrackFill"].ValueAsDefault<bool>(true);
    if (const char *e = getenv("SUPERMODEL_MODEL1_EDGE_EXPAND"))
      expand = (float) atof(e);
    if (const char *c = getenv("SUPERMODEL_MODEL1_CRACK_FILL"))
      crackFill = atoi(c) != 0;
    m_render->SetEdgeExpand(expand, crackFill);
  }
}

bool CModel1::LoadROMs(const ROMSet &rom_set)
{
  m_maincpu.assign(MAINCPU_SIZE, 0xFF);

  struct Placement { const char *region; uint32_t base; bool required; };
  const Placement placements[] =
  {
    { "rom_low",  0x0000000, false },
    { "rom_main", 0x0200000, false },
    { "rom_data", 0x1000000, false },
    { "rom_boot", 0x0F80000, true }
  };
  for (const Placement &p : placements)
  {
    ROM rom = rom_set.get_rom(p.region);
    if (!rom.size)
    {
      if (p.required)
      {
        ErrorLog("Model 1: ROM region '%s' is missing.", p.region);
        return false;
      }
      continue;
    }
    size_t size = std::min<size_t>(rom.size, MAINCPU_SIZE - p.base);
    rom.CopyTo(&m_maincpu[p.base], size);
  }

  auto loadWords = [&](const char *region, std::vector<uint32_t> &dest, size_t words, bool required) -> bool
  {
    ROM rom = rom_set.get_rom(region);
    if (!rom.size)
    {
      if (required)
        ErrorLog("Model 1: ROM region '%s' is missing.", region);
      dest.clear();
      return !required;
    }
    if (!words)
    {
      // Power of 2 size, for masking
      words = 1;
      while (words * 4 < rom.size)
        words <<= 1;
    }
    dest.assign(words, 0);
    rom.CopyTo(reinterpret_cast<uint8_t *>(dest.data()), std::min(rom.size, words * 4));
    return true;
  };

  // Without its program the TGP does nothing useful, but the rest still runs
  loadWords("tgp_program", m_tgpProgram, 0x800, false);
  if (m_tgpProgram.empty())
  {
    ErrorLog("Model 1: TGP program ROM (region 'tgp_program') is missing. The coprocessor won't work.");
    m_tgpProgram.assign(0x800, 0);
  }
  if (!loadWords("copro_tables", m_coproTables, 0x10000, true))
    return false;
  if (!loadWords("polygons", m_polyROM, 0x400000, true))
    return false;
  loadWords("copro_data", m_coproData, 0, false);
  return true;
}

Result CModel1::Init(const Game &game, const ROMSet &rom_set)
{
  m_game = game;

  if (!LoadROMs(rom_set))
    return Result::FAIL;

  m_ramA.assign(0x10000 / 2, 0xFFFF);
  m_ramB.assign(0x40000 / 2, 0);
  m_displayList0.assign(0x10000 / 2, 0);
  m_displayList1.assign(0x10000 / 2, 0);
  m_paletteRAM.assign(0x4000 / 2, 0);
  m_palette.assign(0x4000 / 2, 0xFF000000);
  m_colorXlat.assign(0xC000 / 2, 0);
  m_dpram.assign(0x800, 0xFF);
  m_commRAM.assign(0x2000 / 2, 0);
  m_coproRAM.assign(0x2000, 0);

  m_ioBus.board = this;
  m_v60.AttachBus(this, &m_ioBus);
  m_v60.SetIRQAckCallback([this]() { return IRQAcknowledge(); });
  m_v60.SetFetchRegion(0x000000, 0x100000, &m_maincpu[0x000000]);
  m_v60.SetFetchRegion(0x200000, 0x100000, &m_maincpu[0x200000]);
  m_v60.SetFetchRegion(0xF80000, 0x080000, &m_maincpu[0xF80000]);
  SetBank(0);

  m_tgp.AttachBus(this);
  m_tgp.SetProgramROM(m_tgpProgram.data(), (unsigned) m_tgpProgram.size());

  m_geometry.AttachMemory(m_displayList0.data(), m_displayList1.data(), m_polyROM.data(), (uint32_t) m_polyROM.size(), m_paletteRAM.data(), m_colorXlat.data());

  // Sound board, linked through the UARTs
  ROM soundProgram = rom_set.get_rom("sound_program");
  ROM mpcm1 = rom_set.get_rom("mpcm1");
  ROM mpcm2 = rom_set.get_rom("mpcm2");
  if (soundProgram.size)
  {
    m_soundBoard = std::make_unique<CModel1SoundBoard>(m_config);
    // The board keeps pointers to the sample ROMs
    m_sampleROM1.assign(0x400000, 0);
    m_sampleROM2.assign(0x400000, 0);
    if (mpcm1.size)
      mpcm1.CopyTo(m_sampleROM1.data(), std::min<size_t>(mpcm1.size, m_sampleROM1.size()));
    if (mpcm2.size)
      mpcm2.CopyTo(m_sampleROM2.data(), std::min<size_t>(mpcm2.size, m_sampleROM2.size()));
    std::vector<uint8_t> program(soundProgram.size);
    soundProgram.CopyTo(program.data(), program.size());
    if (Result::OKAY != m_soundBoard->Init(program.data(), program.size(), m_sampleROM1.data(), m_sampleROM1.size(), m_sampleROM2.data(), m_sampleROM2.size()))
      return Result::FAIL;
    m_uart.OnTransmit = [this](uint8_t data)
    {
      if (m_logIO)
        DebugLog("Main -> sound: %02X (frame %llu)\n", data, (unsigned long long) m_frameNumber);
      m_soundBoard->WriteMIDI(data, (m_line + 0.5) / TOTAL_LINES);
    };
    m_soundBoard->SetMIDIOutCallback([this](UINT8 data)
    {
      if (m_logIO)
        DebugLog("Sound -> main: %02X (frame %llu)\n", data, (unsigned long long) m_frameNumber);
      m_uart.ReceiveByte(data);
    });
  }
  else
    ErrorLog("Model 1: no sound program, sound board disabled.");

  // Digital Sound Board (Star Wars Arcade): the Z80 MPEG board, the same as
  // Model 3's DSB1. As in MAME, it receives everything the sound 68K sends.
  ROM mpegProgram = rom_set.get_rom("mpeg_program");
  if (mpegProgram.size && m_soundBoard)
  {
    if (game.mpeg_board != Game::MPEG_BOARD_DSB1)
      ErrorLog("Model 1: MPEG ROMs need <mpeg_board>DSB1</mpeg_board> in the game XML.");
    else
    {
      m_dsbProgram.assign(128 * 1024, 0);
      m_dsbMPEG.assign(16 * 0x100000, 0);
      mpegProgram.CopyTo(m_dsbProgram.data(), m_dsbProgram.size());
      ROM mpegMusic = rom_set.get_rom("mpeg_music");
      if (mpegMusic.size)
        mpegMusic.CopyTo(m_dsbMPEG.data(), std::min<size_t>(mpegMusic.size, m_dsbMPEG.size()));
      m_dsb = std::make_unique<CDSB1>(m_config);
      if (Result::OKAY != m_dsb->Init(m_dsbProgram.data(), m_dsbMPEG.data()))
        return Result::FAIL;
      m_soundBoard->AttachDSB(m_dsb.get());
    }
  }
  m_uart.OnRxRdy = [this](bool) { SoundReady(); };
  m_uart.OnTxRdy = [this](bool) { SoundReady(); };

  m_softwareRender = m_config["Model1SoftwareRender"].ValueAsDefault<bool>(false);
  m_logIO = m_config["Model1LogIO"].ValueAsDefault<bool>(false) || getenv("SUPERMODEL_MODEL1_LOGIO") != nullptr;
  if (getenv("SUPERMODEL_MODEL1_SOFTWARE"))
    m_softwareRender = true;
  if (const char *freeze = getenv("SUPERMODEL_MODEL1_FREEZE"))
    m_freezeAfterFrames = atoi(freeze);
  if (const char *range = getenv("SUPERMODEL_MODEL1_DISASM"))
  {
    // Debugging aid: "start,end" (hex) disassembled to the log
    unsigned start = 0, end = 0;
    if (sscanf(range, "%x,%x", &start, &end) == 2)
    {
      m_v60.SetDisasmReadCallback([this](UINT32 a) { return a < MAINCPU_SIZE ? m_maincpu[a] : (UINT8) 0xFF; });
      for (UINT32 pc = start; pc < end; )
      {
        unsigned len = 0;
        std::string text = m_v60.Disassemble(pc, &len);
        InfoLog("%06X: %s\n", pc, text.c_str());
        pc += len ? len : 1;
      }
    }
  }

  InfoLog("Model 1: %s initialized.", m_game.title.c_str());
  return Result::OKAY;
}

CModel1::CModel1(Util::Config::Node &config, CInputs *inputs, COutputs *outputs)
  : m_config(config),
    m_inputs(inputs),
    m_outputs(outputs),
    m_tiles(0x3FFF)
{
}

CModel1::~CModel1(void)
{
}
