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
 * Model2.cpp
 *
 * Sega Model 2A-CRX board, from MAME's model2.cpp (memory map, coprocessor,
 * geometrizer ports, interrupts, timers, I/O). See Model2.h.
 *
 * Based on original work by R. Belmont, Olivier Galibert, ElSemi, Angelo
 * Salese and Matthew Daniels in MAME. Portions Copyright (c) R. Belmont,
 * Olivier Galibert, ElSemi, Angelo Salese and Matthew Daniels, used under the
 * BSD 3-Clause License: see Docs/README.txt.
 */

#include "Model2.h"
#include "Model3/SoundBoard.h"
#include "Model3/DSB.h"
#include "Model1/Model1SoundBoard.h"
#include "ROMSet.h"
#include "BlockFile.h"
#include "Inputs/Inputs.h"
#include "OSD/Logger.h"
#include "OSD/Video.h"
#include "Graphics/SuperAA.h"
#include "Util/Format.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <cstring>
#include <iostream>
#include <set>

// A second sound board (Model 1's) mixed into the SCSP board's frame through
// its DSB hook: the Model 2 board sends it commands, resets it and saves it
namespace
{
  class Model1SoundMixer : public CDSB
  {
  public:
    explicit Model1SoundMixer(CModel1SoundBoard *board) : m_board(board) {}
    void SendCommand(UINT8) override {}
    void RunFrame(float *audioL, float *audioR, int numSamples) override { m_board->MixFrameInto(audioL, audioR, numSamples); }
    void Reset(void) override {}
    void SaveState(CBlockFile *) override {}
    void LoadState(CBlockFile *) override {}
    Result Init(const UINT8 *, const UINT8 *) override { return Result::OKAY; }
  private:
    CModel1SoundBoard *m_board;
  };
}

namespace
{
  // Time units of 1.25 ns (800 MHz): an i960 cycle (25 MHz) is 32, a TGP
  // cycle (50 MHz / 3) 48, a SHARC cycle (32 MHz) 25. The screen is System
  // 24's: 16 MHz pixel clock, 656 pixels by 424 lines (MAME: "might not be
  // accurate for Model 2").
  constexpr int I960_TIME = 32;
  constexpr int TGP_TIME = 48;
  constexpr int SHARC_TIME = 25;
  constexpr int TGPX4_TIME = 40;            // 20 MHz
  constexpr int LINE_TIME = 32800;          // 656 / 16 MHz = 41 us
  constexpr int TOTAL_LINES = 424;
  constexpr int VBLANK_LINE = 384;
  constexpr int QUANTUM = 64 * I960_TIME;   // how far one CPU may run ahead of the other
  constexpr int UART_TIME = 1600;           // UART clock 500 kHz

  constexpr int SCREEN_WIDTH = 496;
  constexpr int SCREEN_HEIGHT = 384;

  constexpr uint32_t PROGRAM_SIZE = 0x200000;
  constexpr uint32_t MAIN_DATA_SIZE = 0x2000000;

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

  inline uint32_t Get32(const std::vector<uint8_t> &v, uint32_t offset)
  {
    uint32_t d;
    memcpy(&d, &v[offset], 4);
    return d;
  }

  inline void Put32(std::vector<uint8_t> &v, uint32_t offset, uint32_t data, uint32_t mask)
  {
    uint32_t d;
    memcpy(&d, &v[offset], 4);
    d = Combine32(d, data, mask);
    memcpy(&v[offset], &d, 4);
  }

  // A 16-bit device on the 32-bit bus: the halves of the dword are the words
  // at offset and offset + 1 (MAME splits the access the same way)
  template <class R> uint32_t Read16x2(uint32_t mask, R read)
  {
    uint32_t d = 0;
    if (mask & 0x0000FFFF)
      d |= read(0);
    if (mask & 0xFFFF0000)
      d |= uint32_t(read(1)) << 16;
    return d;
  }

  template <class W> void Write16x2(uint32_t data, uint32_t mask, W write)
  {
    if (mask & 0x0000FFFF)
      write(0, uint16_t(data), uint16_t(mask));
    if (mask & 0xFFFF0000)
      write(1, uint16_t(data >> 16), uint16_t(mask >> 16));
  }
}


/******************************************************************************
 Coprocessor (TGP)
******************************************************************************/

void CModel2::Halt(CPUID cpu)
{
  m_halted[cpu] = true;
}

// A halted CPU resumes at the time it was unblocked (the other CPU's exact
// time, part way through its slice)
void CModel2::Unhalt(CPUID cpu, int64_t time)
{
  m_halted[cpu] = false;
  m_time[cpu] = std::max(m_time[cpu], time);
}

uint32_t CModel2::FIFOPop(CFIFO &fifo, CPUID reader, CPUID writer)
{
  int which = &fifo == &m_fifoIn ? 0 : 1;
  if (fifo.values.empty())
  {
    // MAME: the reader stalls (retries), and after a sync, if the FIFO is still
    // empty, it's halted until the next push. Here the sync is the scheduler
    // running the other CPU, so the reader is halted right away.
    if (reader == CPU_COPRO && m_model2b)
      return 0;                                       // the SHARC polls the empty flag
    if (reader == CPU_I960)
      m_i960.Stall();
    else if (m_model2c)
      m_tgpx4.Stall();
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
      Unhalt(writer, Now(reader));
    }
  }
  UpdateSHARCFlags();
  return t;
}

void CModel2::FIFOPush(CFIFO &fifo, uint32_t value, CPUID writer, CPUID reader)
{
  int which = &fifo == &m_fifoIn ? 0 : 1;
  if (!fifo.extra.empty() || fifo.values.size() >= fifo.size)
  {
    // Overflow: kept, and the writer is halted until it drains (the SHARC
    // polls the full flag)
    fifo.extra.push_back(value);
    if (writer == CPU_COPRO && m_model2b)
      return;
    if (!m_fullTriggered[which])
    {
      m_fullTriggered[which] = true;
      Halt(writer);
      if (writer == CPU_COPRO && m_model2c)
        m_tgpx4.EndSlice();
      else if (writer == CPU_COPRO)
        m_tgp.AbortTimeslice();
    }
    return;
  }

  fifo.values.push_back(value);
  if (m_emptyTriggered[which])
  {
    m_emptyTriggered[which] = false;
    Unhalt(reader, Now(writer));
  }
  UpdateSHARCFlags();
}

// 2C: the TGPx4's FIFO ports
UINT32 CModel2::FIFOInPop()
{
  return FIFOPop(m_fifoIn, CPU_COPRO, CPU_I960);
}

void CModel2::FIFOOutPush(UINT32 data)
{
  FIFOPush(m_fifoOut, data, CPU_COPRO, CPU_I960);
}

// MAME's generic_fifo clear: emptied, a writer waiting for room resumes
void CModel2::FIFOInClear()
{
  m_fifoIn.values.clear();
  m_fifoIn.extra.clear();
  if (m_fullTriggered[0])
  {
    m_fullTriggered[0] = false;
    Unhalt(CPU_I960, Now(CPU_COPRO));
  }
}

void CModel2::FIFOOutClear()
{
  m_fifoOut.values.clear();
  m_fifoOut.extra.clear();
  if (m_fullTriggered[1])
  {
    m_fullTriggered[1] = false;
    Unhalt(CPU_COPRO, Now(CPU_COPRO));
  }
}

// 2B: the SHARC's flag inputs 0 (FIFO in empty) and 1 (FIFO out full)
void CModel2::UpdateSHARCFlags()
{
  if (!m_model2b)
    return;
  m_sharc.SetFlagInput(0, m_fifoIn.values.empty());
  m_sharc.SetFlagInput(1, m_fifoOut.values.size() >= m_fifoOut.size);
}

// copro_ctl1_w: bit 31 going high starts a program upload (the TGP is held),
// going low boots it
void CModel2::CoproCtlWrite(uint32_t data, uint32_t mask)
{
  if ((data ^ m_coproCtl) == 0x80000000)
  {
    if (data & 0x80000000)
    {
      DebugLog("Model 2: start copro upload\n");
      m_coproCount = 0;
      // (MAME's copro_halt() does nothing on 2C: the TGPx4 is only held from reset)
      if (!m_model2c)
        m_coproHeld = true;
      if (m_model2b)
        m_sharc.SetHalt(true);
    }
    else
    {
      DebugLog("Model 2: boot copro, %u dwords\n", m_coproCount);
      m_coproHeld = false;
      if (m_model2b)
      {
        m_sharc.SetHalt(false);     // MAME: the halt line is released, no reset
        // Debugging aid: "start,end" (hex) of the SHARC's program disassembled to the log
        unsigned start = 0, end = 0;
        if (const char *range = getenv("SUPERMODEL_MODEL2_SHARC_DISASM"))
          if (sscanf(range, "%x,%x", &start, &end) == 2)
            for (UINT32 pc = start; pc < end; pc++)
              InfoLog("SHARC %05X: %012llX %s\n", pc, (unsigned long long) m_sharc.ReadProgramWord(pc), m_sharc.Disassemble(pc).c_str());
      }
      else if (!m_model2c)
        m_tgp.Reset();
      m_time[CPU_COPRO] = std::max(m_time[CPU_COPRO], I960Now());
    }
  }
  m_coproCtl = Combine32(m_coproCtl, data, mask);
}

void CModel2::CoproFIFOWrite(uint32_t data)
{
  if (m_coproCtl & 0x80000000)
  {
    // the program: into the TGP's program RAM, or by host DMA into the SHARC
    if (m_model2b)
      m_sharc.ExternalDMAWrite(m_coproCount, data & 0xffff);
    else if (m_model2c)
    {
      // 64-bit words, the low half first
      uint64_t &w = m_tgpx4.GetProgramRAM()[(m_coproCount / 2) & (CMB86235::PROGRAM_WORDS - 1)];
      if (m_coproCount & 1)
        w = (w & 0xffffffffULL) | (UINT64(data) << 32);
      else
        w = (w & 0xffffffff00000000ULL) | data;
    }
    else
      m_tgpProgram[m_coproCount & 0xFFF] = data;
    m_coproCount++;
  }
  else
    FIFOPush(m_fifoIn, data, CPU_I960, CPU_COPRO);
}

uint32_t CModel2::copro_sincos_r(unsigned offset)
{
  uint32_t ang = m_coproSinCosBase + offset * 0x4000;
  uint32_t index = ang & 0x3fff;
  if (ang & 0x4000)
    index = std::min(0x4000 - int(index), 0x3fff);
  uint32_t result = m_coproTables[index];
  if (ang & 0x8000)
    result ^= 0x80000000;
  return result;
}

uint32_t CModel2::copro_inv_r(unsigned offset)
{
  uint32_t index = ((m_coproInvBase >> 9) & 0x3ffe) | (offset & 1);
  uint32_t result = m_coproTables[index | 0x8000];
  uint8_t bexp = (m_coproInvBase >> 23) & 0xff;
  uint8_t exp = (result >> 23) + (0x7f - bexp);
  result = (result & 0x007fffff) | (uint32_t(exp) << 23);
  if (m_coproInvBase & 0x80000000 && offset)
    result |= 0x80000000;
  return result;
}

uint32_t CModel2::copro_isqrt_r(unsigned offset)
{
  uint32_t index = 0x2000 ^ (((m_coproISqrtBase >> 10) & 0x3ffe) | (offset & 1));
  uint32_t result = m_coproTables[index | 0xc000];
  uint8_t bexp = (m_coproISqrtBase >> 24) & 0x7f;
  uint8_t exp = (result >> 23) + (0x3f - bexp);
  result = (result & 0x807fffff) | (uint32_t(exp) << 23);
  if (!(offset & 1))
    result &= 0x7fffffff;
  return result;
}

uint32_t CModel2::copro_atan_r()
{
  uint8_t ie = 0x88 - (m_coproAtanBase[3] >> 23);

  bool s0 = m_coproAtanBase[0] & 0x80000000;
  bool s1 = m_coproAtanBase[1] & 0x80000000;
  bool s2 = (m_coproAtanBase[0] & 0x7fffffff) <= (m_coproAtanBase[1] & 0x7fffffff);

  uint32_t im = m_coproAtanBase[3] & 0x7fffff;
  uint32_t index = ie <= 0x17 ? (im | 0x800000) >> ie : 0;
  if (index == 0x4000)
    index = 0x3fff;

  uint32_t result = m_coproTables[index | 0x4000];

  if (s0 ^ s1 ^ s2)
    result >>= 16;
  if (s2)
    result += 0x4000;
  if ((s0 && !s2) || (s1 && s2))
    result += 0x8000;

  return result & 0xffff;
}

// The banked window (I/O space 0000-FFFF while bank bits 22-23 are set):
// copro data ROM (bit 23) or buffer RAM (bit 22)
uint32_t CModel2::copro_tgp_memory_r(unsigned offset)
{
  uint32_t adr = (m_coproBank & 0xff0000) | offset;
  if (adr & 0x800000)
  {
    if (m_coproData.empty())
      return 0;
    return m_coproData[adr & ((uint32_t) m_coproData.size() - 1)];
  }
  if (adr & 0x400000)
    return m_bufferRAM[adr & 0x7fff];
  return 0;
}

void CModel2::copro_tgp_memory_w(unsigned offset, uint32_t data)
{
  uint32_t adr = (m_coproBank & 0xff0000) | offset;
  if (adr & 0x400000)
    m_bufferRAM[adr & 0x7fff] = data;
}

// TGP I/O space. In MAME the bank window is a view over the whole space: while
// it's selected it hides the table ports too.
UINT32 CModel2::ReadIO(UINT16 addr)
{
  if (m_coproBank & 0xc00000)
    return copro_tgp_memory_r(addr);
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

void CModel2::WriteIO(UINT16 addr, UINT32 v)
{
  if (m_coproBank & 0xc00000)
    copro_tgp_memory_w(addr, v);
  else if (addr >= 0x20 && addr <= 0x23)
    m_coproSinCosBase = v;
  else if (addr >= 0x24 && addr <= 0x27)
  {
    m_coproAtanBase[addr - 0x24] = v;
    m_tgp.gpio0_w((m_coproAtanBase[0] & 0x7fffffff) <= (m_coproAtanBase[1] & 0x7fffffff));
  }
  else if (addr >= 0x28 && addr <= 0x29)
    m_coproInvBase = v;
  else if (addr >= 0x2a && addr <= 0x2b)
    m_coproISqrtBase = v;
}

// TGP register file ports: 1 = FIFO in, 2 = FIFO out, 3 = bank
UINT32 CModel2::ReadRF(UINT8 addr)
{
  if (addr == 1)
    return FIFOPop(m_fifoIn, CPU_COPRO, CPU_I960);
  return 0;
}

void CModel2::WriteRF(UINT8 addr, UINT32 v)
{
  if (addr == 2)
    FIFOPush(m_fifoOut, v, CPU_COPRO, CPU_I960);
  else if (addr == 3)
    m_coproBank = v;
}

// 2B SHARC data space (MAME's copro_sharc_map, word addresses): FIFO in, FIFO
// out, buffer RAM, copro data ROM
UINT32 CModel2::ReadData(UINT32 addr)
{
  if (m_model2c)
  {
    // MAME's copro_tgpx4_data_map: buffer RAM (mirrored), copro data ROM
    if (addr >= 0x400000 && addr < 0x800000)
      return m_bufferRAM[addr & 0x7fff];
    if (addr >= 0x800000 && addr < 0xa00000)
      return m_coproData.empty() ? 0 : m_coproData[(addr - 0x800000) & ((uint32_t) m_coproData.size() - 1)];
    return 0;
  }
  if (addr >= 0x0400000 && addr < 0x0c00000)
    return FIFOPop(m_fifoIn, CPU_COPRO, CPU_I960);
  if (addr >= 0x1400000 && addr < 0x1c00000)
    return m_bufferRAM[addr & 0x7fff];
  if (addr >= 0x1c00000 && addr < 0x1e00000)
    return m_coproData.empty() ? 0 : m_coproData[(addr - 0x1c00000) & ((uint32_t) m_coproData.size() - 1)];
  return 0;
}

void CModel2::WriteData(UINT32 addr, UINT32 data)
{
  if (m_model2c)
  {
    if (addr >= 0x400000 && addr < 0x800000)
      m_bufferRAM[addr & 0x7fff] = data;
    return;
  }
  if (addr >= 0x0c00000 && addr < 0x1400000)
    FIFOPush(m_fifoOut, data, CPU_COPRO, CPU_I960);
  else if (addr >= 0x1400000 && addr < 0x1c00000)
    m_bufferRAM[addr & 0x7fff] = data;
}


/******************************************************************************
 2B geometrizer, low level: the program the i960 uploads, run on a SHARC

 Worked out from Fighting Vipers' program (MAME never implemented this):
   400000 read    the display list, read from buffer RAM at the geometrizer
                  read address; jumps (bit 31) are followed where a command
                  word is read, as MAME's geo_parse does (data may have bit
                  31 set)
   410000 write   output to the z-sort/rasterizer (MAME's model2_3d_push; the
                  same words as MAME's simulation pushes, but command words
                  have FF in the top byte, which also ends polygon data)
   420000 write   slow polygon RAM (command 5, address bit 24 clear)
   430000 write   model address latch; 430001 read: the data there (bit 23:
                  polygon ROM, else slow polygon RAM; bit 24, fast polygon
                  RAM, is the SHARC's own memory); 430002 write: handshake
   FLAG0 in       frame sync: low, the program reads the display list; at
                  its end command it pads the output with FF000000 until
                  FLAG0 goes high, then pushes FF00000F and waits for low
 Here a frame runs at once where the simulation would parse the list.
******************************************************************************/

uint32_t CModel2::GeoFeed(bool command)
{
  for (int jumps = 0; jumps < 64; jumps++)
  {
    // a runaway list ends: the end command (with its check copies)
    if (++m_geoFeedCount > 0x40000)
      return 0x07800f0f;
    uint32_t word = m_bufferRAM[m_geoFeedAddress & 0x7fff];
    m_geoFeedAddress++;
    if (!command || !(word & 0x80000000))
      return word;
    m_geoFeedAddress = (word & 0x1ffff) / 4;
  }
  return 0x07800f0f;
}

UINT32 CModel2::GeoSHARCRead(UINT32 addr)
{
  if (addr >= 0x400000 && addr < 0x410000)
  {
    // A jump is only a jump where a command is read (data words may have bit
    // 31 set): Fighting Vipers' program reads commands, and only commands,
    // with R0 = DM(M0, I0)
    bool command = m_geoSharc.ReadProgramWord(m_geoSharc.GetPC()) == 0x403E00000000ULL;
    return GeoFeed(command);
  }
  if (addr == 0x430001)
  {
    if (m_geoROMAddress & 0x00800000)
      return m_polyROM[m_geoROMAddress & ((uint32_t) m_polyROM.size() - 1)];
    return m_geoPolyRAM[m_geoROMAddress & 0x7fff];
  }
  return 0;
}

void CModel2::GeoSHARCWrite(UINT32 addr, UINT32 data)
{
  if (addr >= 0x410000 && addr < 0x420000)
  {
    if (data == 0xff000000)
      m_geoEndSeen = true;
    else if (data == 0xff00000f)
      m_geoParked = true;
    m_video3d.RasterPush(data);
    m_geoOutCount++;
  }
  else if (addr >= 0x420000 && addr < 0x430000)
    m_geoPolyRAM[addr & 0x7fff] = data;
  else if (addr == 0x430000)
    m_geoROMAddress = data;
}

// Debugging aid: the simulation's rasterizer input against the program's. The
// known differences are allowed for: the program marks command words with FF
// (the simulation pushes the bare command), ends polygon data with the next
// command (the simulation with a zero attribute), and pads the frame end.
void CModel2::CompareGeometrizers(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b)
{
  static const char *const names[16] = { "nop", "polygon", "?2", "window", "texture/log", "?5", "?6", "?7",
                                         "zsort", "?9", "?A", "?B", "?C", "?D", "?E", "?F" };
  uint64_t stats[5] = {}, perCommand[16][2] = {};
  size_t i = 0, j = 0;
  unsigned command = 0;
  bool logged = false;
  while (i < a.size() && j < b.size())
  {
    uint32_t x = a[i], y = b[j];
    if ((y >> 24) == 0xff && (y & 0xffffff) == x)
    {
      command = x & 15;
      stats[0]++;
      i++, j++;
      continue;
    }
    if (x == 0 && i + 1 < a.size() && a[i + 1] == 0 && (y >> 24) == 0xff)
    {
      i += 2;   // the simulation's end of polygon data
      continue;
    }
    if (x == 0 && (y >> 24) == 0xff)
    {
      i++;
      continue;
    }
    uint32_t d = x > y ? x - y : y - x;
    int k = d == 0 ? 0 : d <= 4 ? 1 : d <= 0x400 ? 2 : 3;
    {
      bool lumaWord = (x & 0x7fff) == 0 && (y & 0x7fff) == 0 && x < 0x1000000 && y < 0x1000000 && command == 1;
      bool afterLuma = i > 0 && (a[i - 1] & 0x7fff) == 0 && a[i - 1] < 0x1000000 && a[i - 1] >= 0x8000 && command == 1;
      if (d != 0)
      {
        if (lumaWord && ((x ^ y) & 0x800000))
          m_geoClass[0]++;                                // face flipped
        else if (lumaWord)
          m_geoClass[1 + (d > 0x8000)]++;                 // luma off by one / by more
        else if (afterLuma)
          m_geoClass[3 + (k == 3)]++;                     // LOD distance, small / large
        else
          m_geoClass[5 + (k == 3)]++;                     // other words (vertices...), small / large
      }
    }
    stats[k]++;
    if (k >= 2)
      perCommand[command][k - 2]++;
    if (k == 3 && !logged && !m_geoCompareLogged)
    {
      // context of the first large difference
      logged = true;
      std::string sa, sb;
      for (size_t n = i >= 12 ? i - 12 : 0; n < std::min(a.size(), i + 6); n++)
      {
        char w[16];
        snprintf(w, sizeof(w), n == i ? "[%08X] " : "%08X ", a[n]);
        sa += w;
      }
      for (size_t n = j >= 12 ? j - 12 : 0; n < std::min(b.size(), j + 6); n++)
      {
        char w[16];
        snprintf(w, sizeof(w), n == j ? "[%08X] " : "%08X ", b[n]);
        sb += w;
      }
      DebugLog("Geometrizer compare frame %llu, command %s, word %u: simulation %s\n", (unsigned long long) m_frameNumber, names[command], (unsigned) i, sa.c_str());
      DebugLog("Geometrizer compare frame %llu, command %s, word %u: program    %s\n", (unsigned long long) m_frameNumber, names[command], (unsigned) j, sb.c_str());
    }
    i++, j++;
  }
  // what is left: the program's frame end padding, or a real difference in length
  for (; j < b.size(); j++)
    if (b[j] != 0xff000000 && b[j] != 0xff00000f)
      stats[4]++;
  stats[4] += a.size() - i;
  if (logged)
    m_geoCompareLogged = true;
  for (int k = 0; k < 5; k++)
    m_geoCompareStats[k] += stats[k];
  if ((m_frameNumber % 600) == 0)
    DebugLog("Geometrizer compare classes to frame %llu: face flips %llu, luma +-1 %llu, luma more %llu, distance small %llu, distance large %llu, other small %llu, other large %llu\n",
      (unsigned long long) m_frameNumber, (unsigned long long) m_geoClass[0], (unsigned long long) m_geoClass[1], (unsigned long long) m_geoClass[2],
      (unsigned long long) m_geoClass[3], (unsigned long long) m_geoClass[4], (unsigned long long) m_geoClass[5], (unsigned long long) m_geoClass[6]);
  if ((m_frameNumber % 60) == 0)
  {
    std::string per;
    for (int c = 0; c < 16; c++)
      if (perCommand[c][0] || perCommand[c][1])
      {
        char w[64];
        snprintf(w, sizeof(w), " %s %llu/%llu", names[c], (unsigned long long) perCommand[c][0], (unsigned long long) perCommand[c][1]);
        per += w;
      }
    DebugLog("Geometrizer compare frame %llu: %u/%u words; this frame: exact %llu, 1-4 %llu, to 400 %llu, more %llu, unmatched %llu;%s; all frames: exact %llu, 1-4 %llu, to 400 %llu, more %llu, unmatched %llu\n",
      (unsigned long long) m_frameNumber, (unsigned) a.size(), (unsigned) b.size(),
      (unsigned long long) stats[0], (unsigned long long) stats[1], (unsigned long long) stats[2], (unsigned long long) stats[3], (unsigned long long) stats[4], per.c_str(),
      (unsigned long long) m_geoCompareStats[0], (unsigned long long) m_geoCompareStats[1], (unsigned long long) m_geoCompareStats[2],
      (unsigned long long) m_geoCompareStats[3], (unsigned long long) m_geoCompareStats[4]);
  }
}

void CModel2::RunGeometrizer()
{
  // Compare mode: the simulation first, its rasterizer input kept
  std::vector<uint32_t> hle, lle;
  std::function<void(uint32_t)> trace = m_video3d.m_pushTrace;
  if (m_geoCompare)
  {
    m_video3d.m_pushTrace = [&](uint32_t data) { hle.push_back(data); };
    m_video3d.GeoParse(m_geoReadAddress);
    m_video3d.m_pushTrace = [&](uint32_t data) { lle.push_back(data); };
  }

  // The geometrizer's own clock is not modelled: a frame runs here at once
  // (a 40 MHz geometrizer has 667000 cycles in a 60 Hz frame; this is a runaway guard)
  constexpr int SLICE = 2000, BUDGET = 4000000;
  m_video3d.RasterFrameStart();
  m_geoFeedAddress = (m_geoReadAddress & 0x1ffff) / 4;
  m_geoFeedCount = 0;

  // The display list: until the end command pads the output
  m_geoEndSeen = false;
  m_geoSharc.SetFlagInput(0, false);
  int listCycles = 0;
  for (; listCycles < BUDGET && !m_geoEndSeen && !m_geoSharc.IsFatalHalted(); listCycles += SLICE)
    m_geoSharc.Run(SLICE);

  // Frame sync: it ends the frame and waits for the next
  m_geoParked = false;
  m_geoSharc.SetFlagInput(0, true);
  for (int cycles = 0; cycles < BUDGET && !m_geoParked && !m_geoSharc.IsFatalHalted(); cycles += SLICE)
    m_geoSharc.Run(SLICE);
  m_geoSharc.Run(SLICE);
  if (m_geoCompare)
  {
    m_video3d.m_pushTrace = trace;
    CompareGeometrizers(hle, lle);
  }
  if (m_logIO && (m_frameNumber % 60) == 0)
    DebugLog("Model 2 geometrizer frame %llu: read %05X, %u words in, %u out, end %d after %d cycles, parked %d, PC %05X%s, MSGR3 (bad command) %08X\n", (unsigned long long) m_frameNumber,
      m_geoReadAddress, m_geoFeedCount, m_geoOutCount, m_geoEndSeen, listCycles, m_geoParked, m_geoSharc.GetPC(), m_geoSharc.IsFatalHalted() ? " (FATAL)" : "",
      m_geoSharc.ExternalIOPRead(0x0b));
  m_geoOutCount = 0;
}


/******************************************************************************
 Geometrizer ports
******************************************************************************/

void CModel2::GeoPush(uint32_t data)
{
  m_bufferRAM[(m_geoWriteAddress / 4) & 0x7fff] = data;
  m_geoWriteAddress += 4;
}

uint32_t CModel2::GeoRead(uint32_t offset)
{
  uint32_t address = offset * 4;
  if (address == 0x2008)
    return m_geoWriteAddress;
  if (address == 0x3008)
    return m_geoReadAddress;
  DebugLog("Model 2: geo_r %08X (PC %08X)\n", address, m_i960.GetPIP());
  return 0;
}

void CModel2::GeoWrite(uint32_t offset, uint32_t data)
{
  uint32_t address = offset * 4;
  if (address < 0x1000)
  {
    uint32_t function = (address >> 4) & 0x3f;
    if (data & 0x80000000)
      GeoPush((data & 0x800fffff) | (function << 23));
    else if ((address & 0xf) == 0)
    {
      uint32_t r = (data & 0x000fffff) | (function << 23);
      if (((address >> 4) & 0xc0) && function == 1)
        r |= ((address >> 10) & 3) << 29;   // eye mode, used by Sega Rally on car select
      GeoPush(r);
    }
  }
  else if (address == 0x1008)
    m_geoWriteAddress = data & 0xfffff;
  else if (address == 0x3008)
    m_geoReadAddress = data & 0xfffff;
  else
    DebugLog("Model 2: geo_w %08X = %08X\n", address, data);
}


/******************************************************************************
 Interrupts and timers
******************************************************************************/

void CModel2::IRQUpdate()
{
  m_i960.SetIRQLine(0, (m_intReq & 0x001) != 0);
  m_i960.SetIRQLine(1, (m_intReq & 0x002) != 0);
  m_i960.SetIRQLine(2, (m_intReq & 0x3fc) != 0);
  m_i960.SetIRQLine(3, (m_intReq & 0xc00) != 0);
}

// MAME delays this by 80 ns (vcop2 needs it); here it takes effect at once
void CModel2::IRQEnableWrite(uint32_t data, uint32_t mask)
{
  m_intEna = Combine32(m_intEna, data, mask);
  SoundReady();
}

// The sound interrupt is asserted if either RxRDY or TxRDY is active
void CModel2::SoundReady()
{
  const uint32_t line = 1 << 10;
  if ((m_uart.TxRdy() || m_uart.RxRdy()) && (m_intEna & line))
  {
    m_intReq |= line;
    IRQUpdate();
  }
}

// The i960's time, including what it has run of its current slice (the
// timers are read in busy-wait loops: MAME reads them at the exact time)
int64_t CModel2::I960Now() const
{
  return m_time[CPU_I960] + (m_running == CPU_I960 ? int64_t(m_i960.CyclesRun()) * I960_TIME : 0);
}

// Either CPU's time, including what it has run of its current slice
int64_t CModel2::Now(CPUID cpu) const
{
  if (cpu == CPU_I960)
    return I960Now();
  if (m_running != CPU_COPRO)
    return m_time[CPU_COPRO];
  if (m_model2c)
    return m_time[CPU_COPRO] + int64_t(m_tgpx4.CyclesRun()) * TGPX4_TIME;
  return m_time[CPU_COPRO] + (m_model2b ? int64_t(m_sharc.CyclesRun()) * SHARC_TIME : int64_t(m_tgp.CyclesRun()) * TGP_TIME);
}

// The timers count down at 25 MHz (i960 cycles) and raise IRQ 2 at 0
uint32_t CModel2::TimerRead(unsigned n)
{
  if (m_timerRun[n])
  {
    int64_t elapsed = (I960Now() - m_timerStart[n]) / I960_TIME;
    // Run out (MAME's callback would already have run and reloaded it, the
    // IRQ is raised after this slice by RunTimers())
    if (elapsed >= int64_t(m_timerOrig[n]))
      return 0xfffff;
    m_timerVal[n] = m_timerOrig[n] - (uint32_t) elapsed;
  }
  return m_timerVal[n];
}

void CModel2::TimerWrite(unsigned n, uint32_t data, uint32_t mask)
{
  m_timerVal[n] = Combine32(m_timerVal[n], data, mask);
  m_timerOrig[n] = m_timerVal[n];
  m_timerStart[n] = I960Now();
  m_timerRun[n] = true;
}

void CModel2::RunTimers()
{
  for (unsigned n = 0; n < 4; n++)
  {
    if (!m_timerRun[n] || m_time[CPU_I960] - m_timerStart[n] < int64_t(m_timerOrig[n]) * I960_TIME)
      continue;
    const uint32_t line = 1 << (n + 2);
    if (m_intEna & line)
    {
      m_intReq |= line;
      IRQUpdate();
    }
    m_timerVal[n] = 0xfffff;
    m_timerRun[n] = false;
  }
}


/******************************************************************************
 I/O (315-5649, EEPROM)
******************************************************************************/

void CModel2::EEPROMWrite(uint8_t data)
{
  m_ctrlMode = data & 1;
  m_eeprom.Write((data >> 6) & 1, (data >> 7) & 1, (data >> 5) & 1);
}

uint8_t CModel2::In0Read()
{
  uint8_t data = m_in[0];
  if (m_ctrlMode)
    return 0xc0 | (uint8_t(m_eeprom.Read() & 1) << 5) | 0x10 | (data & 0x0f);
  return data;
}

// Rail Chase 2's drive board (pneumatic seat), as MAME simulates it so
// that the game passes its boot check: port D acknowledges the command last
// written to port E
uint8_t CModel2::RailChase2DriveRead() const
{
  uint8_t cmd = m_io.portValue[4];
  uint8_t data = 0xff;
  if (cmd == 0xe0 || cmd == 0x0e)
    data &= ~1;
  if (cmd == 0xd0 || cmd == 0x0d)
    data &= ~2;
  if (cmd == 0xb0 || cmd == 0x0b)
    data &= ~4;
  if (cmd == 0x70 || cmd == 0x07)
    data &= ~8;
  return data;
}

uint8_t CModel2::IORead(unsigned offset)
{
  switch (offset)
  {
  case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06:
    if (m_io.portConfig & (1 << offset))
    {
      switch (offset)
      {
      case 1:  return In0Read();    // port B
      case 2:  return m_in[1];      // port C
      case 3:                       // port D (or the drive board)
        if (m_wheelBoardReplies)
          return m_wheelBoard->Read();
        return m_driveBoard.present ? DriveBoardRead() : m_rchase2Drive ? RailChase2DriveRead() : m_in[2];
      case 6:  return 0xFF;         // port G: DIP switches
      default: return 0xFF;
      }
    }
    return m_io.portValue[offset];
  case 0x0c:
    // serial channel 2: the light guns (MAME's lightgun_mux_r): a byte of a
    // position, or the off-screen bits
    if (m_lightgunMux < 8)
    {
      uint16_t v = m_gun[m_lightgunMux >> 1];
      return (m_lightgunMux & 1) ? uint8_t(v >> 8) : uint8_t(v);
    }
    return 0xfc | m_gunOffscreen;
  case 0x0d:
    return 0x0c;                    // RS-422 status (MAME's hack: receive full, transmit empty)
  case 0x0f:
  {
    uint8_t data = m_analog[m_io.analogChannel];
    m_io.analogChannel = (m_io.analogChannel + 1) & 7;
    return data;
  }
  default:
    return 0xFF;
  }
}

void CModel2::IOWrite(unsigned offset, uint8_t data)
{
  switch (offset)
  {
  case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06:
    m_io.portValue[offset] = data;
    if (offset == 0)
      EEPROMWrite(data);            // port A
    else if (offset == 4 && m_driveBoard.present)
      DriveBoardWrite(data);        // port E: drive board command
    else if (offset == 4 && m_wheelBoard)
      m_wheelBoard->Write(data);    // port E: wheel drive board command
    // port E: billboard / drive board, port F: lamps and coin counters
    break;
  case 0x08:
    m_io.portConfig = data;
    break;
  case 0x0a:
    m_lightgunMux = data;           // serial channel 2 (MAME's lightgun_mux_w)
    break;
  case 0x0f:
    m_io.analogChannel = data & 7;
    break;
  default:
    break;
  }
}


/******************************************************************************
 Drive board (Sega Ski Super G's ski board, Manx TT DX's motion slider)

 Commands on port E, replies on port D. Both games' power-on checks wait for
 CE (the board's power-on sequence, as Model 3's boards: CF, CE, ...), then
 send a game setting and wait for "ready":
   Ski Super G (i960 88B10): sends 00, waits CE; sends 511166, waits FF;
     0x1DF frames, else "DRIVE BOARD TROUBLE".
   Manx TT (i960 5F530): sends 3D, waits CE; sends 5BBA12, waits CD; 0x34E
     and 0x234 frames, else "Drive Board ... Not Respond" (Bx: error codes).
 The setting can be anything, so the board is paced by the commands it gets
 instead of by time: CF to the first 10, CE to the next 10, then it is ready.
 The ski board then works like Model 3's Ski Champ board (CSkiBoard): Dx
 selects what reads return (0 status, 1 DIP switches, 4 foot sensors, all on).
 The motion slider answers CD. No motion or vibration.
******************************************************************************/

uint8_t CModel2::DriveBoardRead()
{
  if (!m_driveBoard.initialized)
    return m_driveBoard.initCommands < 10 ? 0xcf : 0xce;
  if (!m_driveBoard.ski)
    return 0xcd;
  switch (m_driveBoard.readMode)
  {
  case 0:  return 0xff;             // status: no errors
  case 1:  return 0xcf;             // DIP switches (Ski Champ's)
  case 4:  return 0xff;             // foot sensors: left F0, right 0F
  default: return 0xff;
  }
}

void CModel2::DriveBoardWrite(uint8_t cmd)
{
  if (!m_driveBoard.initialized && ++m_driveBoard.initCommands >= 20)
    m_driveBoard.initialized = true;
  if (m_driveBoard.ski && (cmd >> 4) == 0x0d)
    m_driveBoard.readMode = cmd & 7;
}


/******************************************************************************
 Palette
******************************************************************************/

void CModel2::UpdatePaletteEntry(unsigned i)
{
  uint16_t c = m_paletteRAM[i];
  uint8_t r = (uint8_t) m_colorXlat[0x0080 / 2 + ((c >> 0) & 0x1f) * 0x100];
  uint8_t g = (uint8_t) m_colorXlat[0x4080 / 2 + ((c >> 5) & 0x1f) * 0x100];
  uint8_t b = (uint8_t) m_colorXlat[0x8080 / 2 + ((c >> 10) & 0x1f) * 0x100];
  r = m_gamma[r];
  g = m_gamma[g];
  b = m_gamma[b];
  m_palette[i] = 0xFF000000 | (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
}

void CModel2::PaletteWrite(unsigned offset, uint16_t data, uint16_t mask)
{
  m_paletteRAM[offset] = (m_paletteRAM[offset] & ~mask) | (data & mask);
  UpdatePaletteEntry(offset);
}


/******************************************************************************
 i960 address space (model2_base_mem, model2_tgp_mem, model2a_crx_mem)
******************************************************************************/

bool CModel2::Burst(UINT32 addr)
{
  // MAME's regions with the BURST flag
  if (addr < 0x00240000) return true;                                   // ROM, RAM
  if (addr >= 0x00500000 && addr < 0x00600000) return true;             // work RAM
  if (addr >= 0x00804000 && addr < 0x00888000) return !m_model2b && !m_model2c && addr < 0x00884000;   // geo_prg, function port (2A)
  if (addr >= 0x00900000 && addr < 0x00980000) return true;             // buffer RAM
  if (addr >= 0x01000000 && addr < 0x01200000) return (addr & 0x60000) == 0 || (addr & 0x60000) == 0x20000 || (addr & 0xF80000) >= 0x80000;
  if (addr >= 0x01800000 && addr < 0x0181c000) return true;             // palette, colour tables
  if (addr >= 0x01d00000 && addr < 0x01d04000) return true;             // backup RAM
  if (addr >= 0x01a00000 && addr < 0x01a20000) return (addr & 0xffff) < 0x4000;   // comm board memory
  if (addr >= 0x02000000 && addr < 0x04000000) return true;             // data ROM
  if (addr >= 0x06000000 && addr < 0x07000000) return true;
  if (addr >= 0x11600000 && addr < 0x11700000) return true;             // framebuffer
  if (addr >= 0x12000000 && addr < 0x12820000) return !m_model2b && !m_model2c;   // texture RAM, luma RAM
  if (addr >= 0x11000000 && addr < 0x11410000) return m_model2b || m_model2c;     // 2B/2C texture RAM, luma RAM
  return false;
}

uint32_t CModel2::ReadDword(uint32_t addr, uint32_t mask)
{
  if (addr < 0x00200000)
    return Get32(m_program, addr);
  if (m_extra != Extra::None)
  {
    uint32_t data;
    if (ExtraRead(addr, mask, &data))
      return data;
  }
  if (addr < 0x00240000)
  {
    // original Model 2: 128 KB of RAM, then the second 128 KB of the program ROM
    if (m_original && addr >= 0x00220000)
      return Get32(m_program, addr - 0x00200000);
    return Get32(m_ram, addr - 0x00200000);
  }
  if (addr >= 0x00500000 && addr < 0x00600000)
    return Get32(m_workRAM, addr - 0x00500000);
  if (addr >= 0x00900000 && addr < 0x00980000)
    return m_bufferRAM[(addr & 0x1ffff) >> 2];
  if (addr >= 0x02000000 && addr < 0x04000000)
    return Get32(m_mainData, addr - 0x02000000);
  if (addr >= 0x06000000 && addr < 0x07000000)
    return Get32(m_mainData, addr - 0x06000000 + 0x1000000);

  if (addr >= 0x00800000 && addr < 0x00804000)
    return GeoRead((addr & 0x3fff) >> 2);
  if (addr >= 0x00804000 && addr < 0x00808000)
    return 0xffffffff;                                // geo_prg_r
  if (addr >= 0x00884000 && addr < 0x00888000)
    return FIFOPop(m_fifoOut, CPU_I960, CPU_COPRO);     // copro_fifo_r

  switch (addr)
  {
  case 0x00980000: return m_coproCtl;
  case 0x00980004: return m_fifoOut.values.empty() ? 1 : 0;   // fifo_control_r
  case 0x00980014:
    if (m_model2b || m_model2c)
      return m_coproCount == 0 ? 0xffffffff : 0;      // copro_status_r
    break;
  case 0x0098000c:
  {
    // the frame number latched at the last vblank (MAME's m_framenum): games pick
    // the display list buffer to build with it during the following frame
    uint8_t framenum = m_renderMode ? (m_vblankFrame & 1) << 2 : (m_vblankFrame & 2) << 1;
    return framenum | (m_videoControl & 3);
  }
  case 0x00e80000: return m_intReq;
  case 0x00e80004: return m_intEna;
  case 0x00f00000: case 0x00f00004: case 0x00f00008: case 0x00f0000c:
    return TimerRead((addr >> 2) & 3);
  default:
    break;
  }
  if (addr >= 0x00980030 && addr < 0x00980040)
  {
    // tgpid_r (Top Skater reads it and discards the result)
    static const uint8_t id[16] = { 0, 'T', 'A', 'H', 0, 'A', 'K', 'O', 0, 'Z', 'A', 'K', 0, 'M', 'T', 'K' };
    unsigned o = addr & 0xc;
    return id[o] | (uint32_t(id[o + 1]) << 8) | (uint32_t(id[o + 2]) << 16) | (uint32_t(id[o + 3]) << 24);
  }
  if (addr >= 0x00e00000 && addr < 0x00e00038)
    return Get32(m_waitStateRAM, addr - 0x00e00000);

  // Tilemaps (mirrored at +10000, +100000)
  if (addr >= 0x01000000 && addr < 0x01200000)
  {
    uint32_t a = addr & ~0x110000u;
    if (a < 0x01010000)
      return Read16x2(mask, [&](unsigned h) { return m_tiles.ReadTile(((a & 0xffff) >> 1) + h); });
    a = addr & ~0x100000u;
    if (a >= 0x01080000 && a < 0x01100000)
      return Read16x2(mask, [&](unsigned h) { return m_tiles.ReadChar(((a & 0x7ffff) >> 1) + h); });
    return 0;
  }
  if (addr >= 0x01800000 && addr < 0x01804000)
    return Read16x2(mask, [&](unsigned h) { return m_paletteRAM[((addr & 0x3fff) >> 1) + h]; });
  if (addr >= 0x01810000 && addr < 0x0181c000)
    return Read16x2(mask, [&](unsigned h) { return m_colorXlat[((addr - 0x01810000) >> 1) + h]; });

  // Original: the I/O board's dual port RAM (bytes 0 and 2 of each dword)
  if (m_original && addr >= 0x01c00000 && addr < 0x01c01000)
  {
    unsigned i = (addr & 0xfff) >> 1;
    uint32_t d = 0;
    if (mask & 0x000000ff)
      d |= m_dpram[i];
    if (mask & 0x00ff0000)
      d |= uint32_t(m_dpram[i + 1]) << 16;
    return d;
  }
  // 315-5649 I/O (8-bit registers in bytes 0 and 2 of each dword)
  if (addr >= 0x01c00000 && addr < 0x01c00020)
  {
    unsigned reg = (addr & 0x1f) >> 1;
    uint32_t d = 0;
    if (mask & 0x000000ff)
      d |= IORead(reg);
    if (mask & 0x00ff0000)
      d |= uint32_t(IORead(reg + 1)) << 16;
    return d;
  }
  // Model 2B's UART: its data and control registers in byte 0 of 2 words
  // (MAME's model2b_crx_mem, 0x009c0000-7 umask32 0x000000ff)
  if (m_model2b && (addr == 0x009c0000 || addr == 0x009c0004))
    return (mask & 0x000000ff) ? m_uart.Read((addr >> 2) & 1) : 0;
  // UART (bytes 0 and 2)
  if (addr == 0x01c80000)
  {
    uint32_t d = 0;
    if (mask & 0x000000ff)
      d |= m_uart.Read(0);
    if (mask & 0x00ff0000)
      d |= uint32_t(m_uart.Read(1)) << 16;
    return d;
  }
  if (addr >= 0x01d00000 && addr < 0x01d04000)
    return Get32(m_backupRAM, addr - 0x01d00000);

  // Communication board (MAME's m2comm, mirrored at +10000): its shared
  // memory, and the CN and FG flags
  if (addr >= 0x01a00000 && addr < 0x01a20000)
  {
    uint32_t a = addr & ~0x10000u;
    if (a < 0x01a04000)
    {
      unsigned o = a - 0x01a00000;
      return m_comm.ReadShared(o) | (uint32_t(m_comm.ReadShared(o + 1)) << 8) | (uint32_t(m_comm.ReadShared(o + 2)) << 16) | (uint32_t(m_comm.ReadShared(o + 3)) << 24);
    }
    if (a == 0x01a04000)
    {
      uint32_t d = 0;
      if (mask & 0x000000ff)
        d |= m_comm.ReadCN();
      if (mask & 0x00ff0000)
        d |= uint32_t(m_comm.ReadFG()) << 16;
      return d;
    }
    return 0;
  }

  if (addr >= 0x10000000 && addr < 0x10200000)
    return (uint32_t(m_renderUnk) << 14) | (uint32_t(m_renderMode) << 2) | uint32_t(m_renderTestMode);
  if (addr >= 0x10400000 && addr < 0x10600000)
    return m_video3d.PolygonCount();                  // polygon_count_r
  if (addr >= 0x11600000 && addr < 0x11700000)
  {
    std::vector<uint16_t> &fb = m_fbVRAM[(addr >> 19) & 1];
    return Read16x2(mask, [&](unsigned h) { return fb[((addr & 0x7ffff) >> 1) + h]; });
  }
  if ((m_model2b || m_model2c) && addr >= 0x11000000 && addr < 0x11400000)
    return m_textureRAM[(addr >> 21) & 1][(addr & (m_model2c ? 0x1fffff : 0xfffff)) >> 2];   // 2B: 1 MB each, mirrored; 2C: 2 MB
  if ((m_model2b || m_model2c) && addr >= 0x11400000 && addr < 0x11410000)
  {
    // luma RAM in bytes 0 and 2 of each dword
    unsigned i = (addr & 0xfffc) >> 1;
    return m_lumaRAM[i] | (uint32_t(m_lumaRAM[i + 1]) << 16);
  }
  if (!m_model2b && !m_model2c && addr >= 0x12000000 && addr < 0x12800000)
  {
    std::vector<uint32_t> &tex = m_textureRAM[(addr >> 22) & 1];
    return tex[(addr & 0x1fffff) >> 2];
  }
  if (!m_model2b && !m_model2c && addr >= 0x12800000 && addr < 0x12820000)
    return m_lumaRAM[(addr & 0x1ffff) >> 2];

  if (m_logIO)
    DebugLog("Model 2: unmapped read %08X (PC %08X)\n", addr, m_i960.GetPIP());
  return 0;
}

void CModel2::WriteDword(uint32_t addr, uint32_t data, uint32_t mask)
{
  if (addr < 0x00200000)
    return;                                           // ROM
  if (m_extra != Extra::None && ExtraWrite(addr, data, mask))
    return;
  if (addr < 0x00240000)
  {
    if (!(m_original && addr >= 0x00220000))
      Put32(m_ram, addr - 0x00200000, data, mask);
    return;
  }
  if (addr >= 0x00500000 && addr < 0x00600000)
  {
    Put32(m_workRAM, addr - 0x00500000, data, mask);
    return;
  }
  if (addr >= 0x00900000 && addr < 0x00980000)
  {
    uint32_t &d = m_bufferRAM[(addr & 0x1ffff) >> 2];
    d = Combine32(d, data, mask);
    return;
  }

  if (addr >= 0x00800000 && addr < 0x00804000)
  {
    GeoWrite((addr & 0x3fff) >> 2, data);
    return;
  }
  if (addr >= 0x00804000 && addr < 0x00808000)
  {
    // geo_prg_w: the program upload (kept on 2B, not run: the geometrizer is
    // simulated) or data
    if (m_geoCtl & 0x80000000)
    {
      if (m_model2b && m_geoCount < 0x100000)
        m_geoProgram.push_back(data);
      if (m_geoLLE)
        m_geoSharc.ExternalDMAWrite(m_geoCount, data & 0xffff);   // host boot, as the copro
      m_geoCount++;
    }
    else
      GeoPush(data);
    return;
  }
  if (addr >= 0x00880000 && addr < 0x00884000)
  {
    // copro_function_port_w
    uint32_t d = (data & 0x800fffff) | ((((addr & 0x3fff) >> 4) & 0xff) << 23);
    FIFOPush(m_fifoIn, d, CPU_I960, CPU_COPRO);
    return;
  }
  if (addr >= 0x00884000 && addr < 0x00888000)
  {
    CoproFIFOWrite(data);
    return;
  }
  if (m_model2b && addr >= 0x008c0000 && addr < 0x008c1000)
  {
    m_sharc.ExternalIOPWrite((addr & 0xfff) >> 2, data);
    return;
  }
  if (m_model2b && addr >= 0x00840000 && addr < 0x00841000)
  {
    // the geometrizer SHARC's IOP registers (boot setup; nothing to do when simulated)
    if (m_geoLLE)
      m_geoSharc.ExternalIOPWrite((addr & 0xfff) >> 2, data);
    return;
  }

  switch (addr)
  {
  case 0x00980000:
    CoproCtlWrite(data, mask);
    return;
  case 0x00980008:
    // geo_ctl1_w
    if ((data ^ m_geoCtl) == 0x80000000)
    {
      if (data & 0x80000000)
      {
        m_geoCount = 0;
        m_geoProgram.clear();
        if (m_geoLLE)
        {
          m_geoSharc.Reset();
          m_geoSharc.SetHalt(true);
          m_geoSharc.SetFlagInput(0, false);
        }
      }
      else
      {
        DebugLog("Model 2: boot geo, %u dwords\n", m_geoCount);
        if (m_geoLLE)
        {
          m_geoSharc.SetHalt(false);
          // Debugging aid: "start,end" (hex) of the geometrizer's program disassembled to the log
          unsigned start = 0, end = 0;
          if (const char *range = getenv("SUPERMODEL_MODEL2_GEO_DISASM"))
            if (sscanf(range, "%x,%x", &start, &end) == 2)
              for (UINT32 pc = start; pc < end; pc++)
                InfoLog("GEO %05X: %012llX %s\n", pc, (unsigned long long) m_geoSharc.ReadProgramWord(pc), m_geoSharc.Disassemble(pc).c_str());
        }
        // Debugging aid: the uploaded geometrizer program to a file
        if (const char *file = getenv("SUPERMODEL_MODEL2_GEOPROGRAM"))
          if (FILE *fp = fopen(file, "wb"))
          {
            fwrite(m_geoProgram.data(), 4, m_geoProgram.size(), fp);
            fclose(fp);
          }
      }
    }
    m_geoCtl = data;
    return;
  case 0x0098000c:
    m_videoControl = Combine32(m_videoControl, data, mask);
    return;
  case 0x00980020:
    if (m_model2b)
      return;                                         // bank control (all games write 0)
    break;
  case 0x00e80000:
  {
    // irq_ack_w. The i960's lines are level sensitive: a line shared by
    // several requests (the timers share line 2) stays asserted when only
    // some of them are acknowledged, and the CPU takes it again once the
    // handler returns. The core reacts to edges (as MAME's): the line is
    // signalled again so it is held and taken. Over Rev acknowledges one of
    // two pending timers; the other was never taken, its timer never
    // restarted and the game's sound commands stopped.
    static const uint32_t lineMask[4] = { 0x001, 0x002, 0x3fc, 0xc00 };
    uint32_t before = m_intReq;
    m_intReq &= data;
    for (int line = 0; line < 4; line++)
    {
      if ((before & ~m_intReq & lineMask[line]) && (m_intReq & lineMask[line]))
        m_i960.SetIRQLine(line, false);
    }
    IRQUpdate();
    return;
  }
  case 0x00e80004:
    IRQEnableWrite(data, mask);
    return;
  case 0x00f00000: case 0x00f00004: case 0x00f00008: case 0x00f0000c:
    TimerWrite((addr >> 2) & 3, data, mask);
    return;
  case 0x0181c000:
    m_video3d.ZClipWrite(data);                       // model2_3d_zclip_w
    return;
  default:
    break;
  }
  if (addr >= 0x00e00000 && addr < 0x00e00038)
  {
    Put32(m_waitStateRAM, addr - 0x00e00000, data, mask);
    return;
  }

  if (addr >= 0x01000000 && addr < 0x01200000)
  {
    uint32_t a = addr & ~0x110000u;
    if (a < 0x01010000)
    {
      Write16x2(data, mask, [&](unsigned h, uint16_t d, uint16_t m) { m_tiles.WriteTile(((a & 0xffff) >> 1) + h, d, m); });
      return;
    }
    a = addr & ~0x100000u;
    if (a >= 0x01080000 && a < 0x01100000)
      Write16x2(data, mask, [&](unsigned h, uint16_t d, uint16_t m) { m_tiles.WriteChar(((a & 0x7ffff) >> 1) + h, d, m); });
    // The tilemap chip's sync registers also position the 3D (MAME's
    // xhout/xvout callbacks); 01020000 ABSEL and 01070000 sync switch ignored
    else if (a == 0x01040000 && (mask & 0xffff))
      m_video3d.HorizontalSync(uint16_t(data));
    else if (a == 0x01060000 && (mask & 0xffff))
      m_video3d.VerticalSync(uint16_t(data));
    return;
  }
  if (addr >= 0x01800000 && addr < 0x01804000)
  {
    Write16x2(data, mask, [&](unsigned h, uint16_t d, uint16_t m) { PaletteWrite(((addr & 0x3fff) >> 1) + h, d, m); });
    return;
  }
  if (addr >= 0x01810000 && addr < 0x0181c000)
  {
    Write16x2(data, mask, [&](unsigned h, uint16_t d, uint16_t m)
    {
      unsigned offset = ((addr - 0x01810000) >> 1) + h;
      m_colorXlat[offset] = (m_colorXlat[offset] & ~m) | (d & m);
      // writing the scroll colour table changes the palette
      if ((offset & 0xff) == 0x80 / 2)
        m_paletteDirty = true;
    });
    return;
  }
  if (m_original && addr >= 0x01c00000 && addr < 0x01c01000)
  {
    unsigned i = (addr & 0xfff) >> 1;
    if (mask & 0x000000ff)
      m_dpram[i] = uint8_t(data);
    if (mask & 0x00ff0000)
      m_dpram[i + 1] = uint8_t(data >> 16);
    // byte 11: the drive board command, which the I/O board passes on (its port E)
    if (m_wheelBoard && ((i == 0x11 && (mask & 0x000000ff)) || (i + 1 == 0x11 && (mask & 0x00ff0000))))
      m_wheelBoard->Write(m_dpram[0x11]);
    return;
  }
  if (addr >= 0x01c00000 && addr < 0x01c00020)
  {
    unsigned reg = (addr & 0x1f) >> 1;
    if (mask & 0x000000ff)
      IOWrite(reg, uint8_t(data));
    if (mask & 0x00ff0000)
      IOWrite(reg + 1, uint8_t(data >> 16));
    return;
  }
  if (m_model2b && (addr == 0x009c0000 || addr == 0x009c0004))
  {
    if (mask & 0x000000ff)
      m_uart.Write((addr >> 2) & 1, uint8_t(data));
    return;
  }
  if (addr == 0x01c80000)
  {
    if (mask & 0x000000ff)
      m_uart.Write(0, uint8_t(data));
    if (mask & 0x00ff0000)
      m_uart.Write(1, uint8_t(data >> 16));
    return;
  }
  if (addr >= 0x01d00000 && addr < 0x01d04000)
  {
    Put32(m_backupRAM, addr - 0x01d00000, data, mask);
    return;
  }
  if (addr >= 0x01a00000 && addr < 0x01a20000)
  {
    uint32_t a = addr & ~0x10000u;
    if (a < 0x01a04000)
    {
      unsigned o = a - 0x01a00000;
      for (unsigned b = 0; b < 4; b++)
        if (mask & (0xffu << (b * 8)))
          m_comm.WriteShared(o + b, uint8_t(data >> (b * 8)));
    }
    else if (a == 0x01a04000)
    {
      if (mask & 0x000000ff)
        m_comm.WriteCN(uint8_t(data));
      if (mask & 0x00ff0000)
        m_comm.WriteFG(uint8_t(data >> 16));
    }
    return;
  }

  if (addr >= 0x10000000 && addr < 0x10200000)
  {
    // render_mode_w: bit 0 test mode (host framebuffer access), bit 2 60 Hz, bit 14 unknown
    m_renderTestMode = (data & 1) != 0;
    m_renderMode = (data & 4) != 0;
    m_renderUnk = (data & 0x4000) != 0;
    return;
  }
  if (addr >= 0x11600000 && addr < 0x11700000)
  {
    std::vector<uint16_t> &fb = m_fbVRAM[(addr >> 19) & 1];
    Write16x2(data, mask, [&](unsigned h, uint16_t d, uint16_t m)
    {
      uint16_t &w = fb[((addr & 0x7ffff) >> 1) + h];
      w = (w & ~m) | (d & m);
    });
    return;
  }
  if ((m_model2b || m_model2c) && addr >= 0x11000000 && addr < 0x11400000)
  {
    uint32_t &d = m_textureRAM[(addr >> 21) & 1][(addr & (m_model2c ? 0x1fffff : 0xfffff)) >> 2];
    d = Combine32(d, data, mask);
    return;
  }
  if ((m_model2b || m_model2c) && addr >= 0x11400000 && addr < 0x11410000)
  {
    unsigned i = (addr & 0xfffc) >> 1;
    if (mask & 0x000000ff)
      m_lumaRAM[i] = uint8_t(data);
    if (mask & 0x00ff0000)
      m_lumaRAM[i + 1] = uint8_t(data >> 16);
    return;
  }
  if (!m_model2b && !m_model2c && addr >= 0x12000000 && addr < 0x12800000)
  {
    // tex0_w/tex1_w: each dword written carries 16 bits, into alternate halves
    std::vector<uint32_t> &tex = m_textureRAM[(addr >> 22) & 1];
    uint32_t offset = (addr & 0x1fffff) >> 2;
    uint32_t &d = tex[(offset >> 1) & 0x7ffff];
    if (offset & 1)
      d = (d & 0x0000ffff) | ((data & 0xffff) << 16);
    else
      d = (d & 0xffff0000) | (data & 0xffff);
    return;
  }
  if (!m_model2b && !m_model2c && addr >= 0x12800000 && addr < 0x12820000)
  {
    if (mask & 0xff)
      m_lumaRAM[(addr & 0x1ffff) >> 2] = uint8_t(data);
    return;
  }

  if (m_logIO)
    DebugLog("Model 2: unmapped write %08X = %08X & %08X (PC %08X)\n", addr, data, mask, m_i960.GetPIP());
}

// The board's wait states: the program writes Sega's area wait table (one word
// per memory area at 0xE00000: 01/02/04/08/10 = 0-4 waits, 20 = the device
// signals ready itself, bit 6 = DRAM) and each i960 data access to an area is
// stretched by its waits. Opcode fetches aren't charged (the i960KB's
// instruction cache), nor DRAM (page mode hits). The areas as sm2-emu's
// model2_area_timing.h, from Sega's table.
int CModel2::AreaWaits(uint32_t addr) const
{
  unsigned offset;
  if (addr < 0x00200000)                                offset = 0x00;   // program ROM
  else if (addr >= 0x00500000 && addr < 0x00600000)     offset = 0x08;   // work RAM
  else if (addr >= 0x00900000 && addr < 0x00980000)     offset = 0x04;   // buffer RAM
  else if (addr >= 0x01800000 && addr < 0x0181c000)     offset = 0x1c;   // palette, colour tables
  else if (addr >= 0x01a00000 && addr < 0x01a20000)     offset = 0x20;   // comm board
  else if (addr >= 0x01c00000 && addr < 0x01c80000)     offset = 0x24;   // I/O
  else if (addr >= 0x01c80000 && addr < 0x01d00000)     offset = 0x28;   // sound
  else if (addr >= 0x01d00000 && addr < 0x01d80000)     offset = 0x2c;   // backup RAM
  else if (addr >= 0x01d80000 && addr < 0x01e00000)     offset = 0x30;   // security
  else if ((addr >= 0x02000000 && addr < 0x04000000) || (addr >= 0x06000000 && addr < 0x07000000))
    offset = 0x34;                                                        // data ROM
  else
    return 0;
  uint8_t setting = m_waitStateRAM[offset];               // little endian: the word's low byte
  if (setting & 0x40)
    return 0;                                             // DRAM
  switch (setting & 0x3f)
  {
  case 0x02: return 1;
  case 0x04: return 2;
  case 0x08: return 3;
  case 0x10: return 4;
  default:   return 0;
  }
}

UINT8 CModel2::Read8(UINT32 addr)
{
  ChargeWaits(addr);
  unsigned shift = (addr & 3) * 8;
  return uint8_t(ReadDword(addr & ~3u, 0xffu << shift) >> shift);
}

UINT16 CModel2::Read16(UINT32 addr)
{
  ChargeWaits(addr);
  unsigned shift = (addr & 2) * 8;
  return uint16_t(ReadDword(addr & ~3u, 0xffffu << shift) >> shift);
}

UINT32 CModel2::Read32(UINT32 addr)
{
  ChargeWaits(addr);
  return ReadDword(addr & ~3u, 0xffffffff);
}

void CModel2::Write8(UINT32 addr, UINT8 data)
{
  ChargeWaits(addr);
  unsigned shift = (addr & 3) * 8;
  WriteDword(addr & ~3u, uint32_t(data) << shift, 0xffu << shift);
}

void CModel2::Write16(UINT32 addr, UINT16 data)
{
  ChargeWaits(addr);
  unsigned shift = (addr & 2) * 8;
  WriteDword(addr & ~3u, uint32_t(data) << shift, 0xffffu << shift);
}

void CModel2::Write32(UINT32 addr, UINT32 data)
{
  ChargeWaits(addr);
  WriteDword(addr & ~3u, data, 0xffffffff);
}


/******************************************************************************
 Inputs
******************************************************************************/

// Each game's controls, as MAME's input port sets (generated from MAME's
// model2.cpp by games_xml_model2_all.py: game -> its port set's layout)
static const struct { const char *game; const char *layout; } s_controlLayouts[] =
{
  { "daytona", "daytona" },
  { "daytonase", "daytona" },
  { "daytona93", "daytona" },
  { "daytonas", "daytona" },
  { "daytonat", "daytona" },
  { "daytonata", "daytona" },
  { "daytonam", "daytona" },
  { "daytonagtx", "daytona" },
  { "desert", "desert" },
  { "vcop", "gun" },
  { "vcopa", "gun" },
  { "vf2", "fighting" },
  { "vf2b", "fighting" },
  { "vf2a", "fighting" },
  { "vf2o", "fighting" },
  { "manxtt", "bike" },
  { "manxttc", "bike" },
  { "manxttdx", "bike" },
  { "srallyc", "srallyc" },
  { "srallycb", "srallyc" },
  { "srallycc", "srallyc" },
  { "srallycdx", "srallyc" },
  { "srallycdxa", "srallyc" },
  { "vcop2", "gun" },
  { "skytargt", "skytargt" },
  { "doaa", "fighting" },
  { "doaab", "fighting" },
  { "doaae", "fighting" },
  { "zeroguna", "fighting" },
  { "zerogunaj", "fighting" },
  { "motoraid", "bike" },
  { "motoraiddx", "bike" },
  { "airwlkrs", "fighting" },
  { "dynamcop", "fighting" },
  { "dyndeka2", "fighting" },
  { "pltkidsa", "fighting" },
  { "hpyagu98", "fighting" },
  { "rchase2", "analoggun" },
  { "rchase2a", "analoggun" },
  { "vstriker", "soccer" },
  { "vstrikero", "soccer" },
  { "fvipers", "fighting" },
  { "fvipersb", "fighting" },
  { "fvipersa", "fighting" },
  { "gunblade", "analoggun" },
  { "indy500", "indy500" },
  { "indy500d", "indy500" },
  { "indy500to", "indy500" },
  { "von", "von" },
  { "vonu", "von" },
  { "vonj", "von" },
  { "vonr", "von" },
  { "schamp", "fighting" },
  { "sfight", "fighting" },
  { "lastbrnx", "fighting" },
  { "lastbrnxu", "fighting" },
  { "lastbrnxj", "fighting" },
  { "doa", "fighting" },
  { "doab", "fighting" },
  { "sgt24h", "indy500" },
  { "powsled", "powsled" },
  { "powsledr", "powsled" },
  { "powsledm", "powsled" },
  { "dynabb", "fighting" },
  { "dynabb97", "fighting" },
  { "overrevb", "indy500" },
  { "overrevba", "indy500" },
  { "zerogun", "fighting" },
  { "zerogunj", "fighting" },
  { "dynamcopb", "fighting" },
  { "dyndeka2b", "fighting" },
  { "pltkids", "fighting" },
  { "skisuprg", "ski" },
  { "stcc", "indy500" },
  { "stccb", "indy500" },
  { "stcca", "indy500" },
  { "stcco", "indy500" },
  { "waverunr", "waverunr" },
  { "bel", "analoggun" },
  { "hotd", "gun" },
  { "hotdo", "gun" },
  { "hotdp", "gun" },
  { "overrev", "indy500" },
  { "rascot2", "fighting" },
  { "segawski", "segawski" },
  { "topskatr", "topskatr" },
  { "topskatru", "topskatr" },
  { "topskatruo", "topskatr" },
  { "topskatrj", "topskatr" },
  { "dynamcopc", "fighting" },
};

CModel2::Layout CModel2::LayoutOf(const std::string &game, const std::string &parent)
{
  static const std::map<std::string, Layout> names =
  {
    { "daytona", Layout::Daytona }, { "desert", Layout::Desert }, { "gun", Layout::Gun }, { "fighting", Layout::Fighting },
    { "soccer", Layout::Soccer }, { "bike", Layout::Bike }, { "srallyc", Layout::SegaRally }, { "indy500", Layout::Indy500 },
    { "skytargt", Layout::SkyTarget }, { "analoggun", Layout::AnalogGun }, { "von", Layout::VirtualOn }, { "powsled", Layout::PowerSled },
    { "ski", Layout::Ski }, { "waverunr", Layout::WaveRunner }, { "segawski", Layout::WaterSki }, { "topskatr", Layout::TopSkater },
  };
  for (const std::string &name : { game, parent })
    for (const auto &e : s_controlLayouts)
      if (name == e.game)
      {
        auto it = names.find(e.layout);
        if (it != names.end())
          return it->second;
      }
  return Layout::Fighting;   // MAME's model2/model2crx: two joysticks with 4 buttons each
}

void CModel2::UpdateInputs(void)
{
  if (!m_inputs)
    return;

  auto sw = [](const std::shared_ptr<CSwitchInput> &in) -> bool { return in && in->value; };
  auto axis = [](const std::shared_ptr<CAxisInput> &in, uint8_t center = 0x80) -> uint8_t { return in ? (uint8_t) in->value : center; };
  // a value 0-255 scaled to [lo, hi] (reversed: hi to lo)
  auto range = [](uint8_t v, int lo, int hi, bool reverse = false) -> uint8_t
  {
    int r = lo + (int(v) * (hi - lo) + 127) / 255;
    return (uint8_t) (reverse ? hi + lo - r : r);
  };
  uint8_t in0 = 0, in1 = 0, in2 = 0;
  const uint32_t inputs = m_game.inputs;
  const std::string &name = m_game.name;
  const std::string &parent = m_game.parent;
  auto is = [&](const char *n) { return name == n || parent == n; };

  // MAME's model2 port set: coins, test, service, start 1 and 2
  in0 |= sw(m_inputs->coin[0]) ? 0x01 : 0;
  in0 |= sw(m_inputs->coin[1]) ? 0x02 : 0;
  in0 |= sw(m_inputs->test[0]) ? 0x04 : 0;
  in0 |= sw(m_inputs->service[0]) ? 0x08 : 0;
  in0 |= sw(m_inputs->start[0]) ? 0x10 : 0;

  // a joystick and buttons per player in IN1 (P1) and IN2 (P2): buttons
  // 1-4 in bits 0-3, down, up, right, left in bits 4-7
  auto joystick = [&](int p, uint8_t &v)
  {
    v |= sw(m_inputs->down[p]) ? 0x10 : 0;
    v |= sw(m_inputs->up[p]) ? 0x20 : 0;
    v |= sw(m_inputs->right[p]) ? 0x40 : 0;
    v |= sw(m_inputs->left[p]) ? 0x80 : 0;
  };

  uint8_t steer = m_inputs->steering ? (uint8_t) m_inputs->steering->value : 0x80;
  uint8_t accel = m_inputs->accelerator ? (uint8_t) m_inputs->accelerator->value : 0;
  uint8_t brake = m_inputs->brake ? (uint8_t) m_inputs->brake->value : 0;

  switch (m_layout)
  {
  case Layout::Fighting:
  default:
  {
    // Virtua Fighter 2, Fighting Vipers, Last Bronx (punch, kick, guard),
    // Dead or Alive (hold, punch, kick), Sonic Championship (punch, kick,
    // barrier), Dynamite Cop (punch, kick, jump), Zero Gunner, Pilot Kids,
    // Air Walkers, Dynamite Baseball...: MAME's model2crx port set
    in0 |= sw(m_inputs->start[1]) ? 0x20 : 0;
    uint8_t *port[2] = { &in1, &in2 };
    for (int p = 0; p < 2; p++)
    {
      uint8_t &v = *port[p];
      v |= sw(m_inputs->punch[p]) ? 0x01 : 0;
      v |= sw(m_inputs->kick[p]) ? 0x02 : 0;
      v |= sw(m_inputs->guard[p]) ? 0x04 : 0;
      v |= sw(m_inputs->escape[p]) ? 0x08 : 0;
      joystick(p, v);
    }
    // Dynamite Baseball: the bat swing is analog (channels 0 and 1); a
    // button swings fully
    if (is("dynabb") || is("dynabb97"))
    {
      m_analog[0] = sw(m_inputs->guard[0]) ? 0xFF : 0x00;
      m_analog[1] = sw(m_inputs->guard[1]) ? 0xFF : 0x00;
    }
    break;
  }

  case Layout::Soccer:
    // Virtua Striker: long pass, shoot, short pass in bits 0-2
    in0 |= sw(m_inputs->start[1]) ? 0x20 : 0;
    for (int p = 0; p < 2; p++)
    {
      uint8_t &v = p ? in2 : in1;
      v |= sw(m_inputs->longPass[p]) ? 0x01 : 0;
      v |= sw(m_inputs->shoot[p]) ? 0x02 : 0;
      v |= sw(m_inputs->shortPass[p]) ? 0x04 : 0;
      joystick(p, v);
    }
    break;

  case Layout::VirtualOn:
    // Cyber Troopers Virtual-On: the left stick and its shot/dash buttons in
    // IN1, the right ones in IN2
    in1 |= sw(m_inputs->twinJoyShot1) ? 0x01 : 0;
    in1 |= sw(m_inputs->twinJoyTurbo1) ? 0x02 : 0;
    in1 |= sw(m_inputs->twinJoyDown1) ? 0x10 : 0;
    in1 |= sw(m_inputs->twinJoyUp1) ? 0x20 : 0;
    in1 |= sw(m_inputs->twinJoyRight1) ? 0x40 : 0;
    in1 |= sw(m_inputs->twinJoyLeft1) ? 0x80 : 0;
    in2 |= sw(m_inputs->twinJoyShot2) ? 0x01 : 0;
    in2 |= sw(m_inputs->twinJoyTurbo2) ? 0x02 : 0;
    in2 |= sw(m_inputs->twinJoyDown2) ? 0x10 : 0;
    in2 |= sw(m_inputs->twinJoyUp2) ? 0x20 : 0;
    in2 |= sw(m_inputs->twinJoyRight2) ? 0x40 : 0;
    in2 |= sw(m_inputs->twinJoyLeft2) ? 0x80 : 0;
    break;

  case Layout::Daytona:
    // Daytona USA (I/O board): wheel and pedals 20-E0 (MAME's ports); the
    // unconnected channels read FF. VR buttons (red, blue, yellow, green)
    for (int i = 3; i < 8; i++)
      m_analog[i] = 0xFF;
    m_analog[0] = (uint8_t) (0x20 + steer * 0xC0 / 255);
    m_analog[1] = (uint8_t) (0x20 + accel * 0xC0 / 255);
    m_analog[2] = (uint8_t) (0x20 + brake * 0xC0 / 255);
    in0 |= sw(m_inputs->vr[0]) ? 0x20 : 0;
    in0 |= sw(m_inputs->vr[1]) ? 0x40 : 0;
    in0 |= sw(m_inputs->vr[2]) ? 0x80 : 0;
    in1 |= sw(m_inputs->vr[3]) ? 0x01 : 0;
    break;

  case Layout::Desert:
  {
    // Desert Tank (I/O board): steering, accelerator, brake on channels 0-2;
    // VR 1-3 in IN0 bits 5-7; shift (toggle) IN1 bit 0, machine gun bit 4,
    // cannon bit 5
    for (int i = 3; i < 8; i++)
      m_analog[i] = 0xFF;
    m_analog[0] = steer;
    m_analog[1] = accel;
    m_analog[2] = brake;
    in0 |= sw(m_inputs->vr[0]) ? 0x20 : 0;
    in0 |= sw(m_inputs->vr[1]) ? 0x40 : 0;
    in0 |= sw(m_inputs->vr[2]) ? 0x80 : 0;
    if (m_inputs->gearShiftUp && m_inputs->gearShiftUp->Pressed())
      m_gearSel ^= 1;
    in1 |= (m_gearSel & 1) ? 0x01 : 0;
    in1 |= sw(m_inputs->analogJoyTrigger1) ? 0x10 : 0;
    in1 |= sw(m_inputs->analogJoyTrigger2) ? 0x20 : 0;
    break;
  }

  case Layout::SegaRally:
    // Sega Rally: wheel, accelerator, brake on analog channels 0-2; start is
    // bit 6 and the VR button bit 5
    m_analog[0] = steer;
    m_analog[1] = accel;
    m_analog[2] = brake;
    if (in0 & 0x10)
      in0 = (in0 & ~0x10) | 0x40;
    in0 |= sw(m_inputs->vr[0]) ? 0x20 : 0;
    break;

  case Layout::Bike:
    // The bikes (Manx TT, Motor Raid; MAME's manxtt ports): throttle, brake,
    // bank (lean, the wheel, reversed) on analog channels 0-2; start is bit
    // 6, shift up/down IN1 bits 4/5 (Motor Raid: punch, kick)
    m_analog[0] = accel;
    m_analog[1] = brake;
    m_analog[2] = (uint8_t) (0xFF - steer);
    if (in0 & 0x10)
      in0 = (in0 & ~0x10) | 0x40;
    if (is("motoraid"))
    {
      in1 |= sw(m_inputs->punch[0]) ? 0x10 : 0;
      in1 |= sw(m_inputs->kick[0]) ? 0x20 : 0;
    }
    else
    {
      in1 |= sw(m_inputs->gearShiftUp) ? 0x10 : 0;
      in1 |= sw(m_inputs->gearShiftDown) ? 0x20 : 0;
    }
    break;

  case Layout::Indy500:
  {
    // Indy 500, Super GT 24h, Over Rev: wheel, gas, brake on channels 0-2
    // (Super GT and Over Rev: the pedals reversed); start is bit 6; view 1/2
    // IN1 bits 0/1, shift up/down bits 4/5
    const bool reversed = is("sgt24h") || is("overrev");
    m_analog[0] = steer;
    m_analog[1] = reversed ? (uint8_t) (0xFF - accel) : accel;
    m_analog[2] = reversed ? (uint8_t) (0xFF - brake) : brake;
    if (in0 & 0x10)
      in0 = (in0 & ~0x10) | 0x40;
    in1 |= sw(m_inputs->vr[0]) ? 0x01 : 0;
    in1 |= sw(m_inputs->vr[1]) ? 0x02 : 0;
    in1 |= sw(m_inputs->gearShiftUp) ? 0x10 : 0;
    in1 |= sw(m_inputs->gearShiftDown) ? 0x20 : 0;
    break;
  }

  case Layout::SkyTarget:
    // Sky Target: the stick's Y on channel 0, X (reversed) on 2; start is
    // bit 6, view IN0 bit 5; machine gun, missile IN1 bits 4/5
    m_analog[0] = axis(m_inputs->analogJoyY);
    m_analog[2] = (uint8_t) (0xFF - axis(m_inputs->analogJoyX));
    if (in0 & 0x10)
      in0 = (in0 & ~0x10) | 0x40;
    in0 |= sw(m_inputs->viewChange) ? 0x20 : 0;
    in1 |= sw(m_inputs->analogJoyTrigger1) ? 0x10 : 0;
    in1 |= sw(m_inputs->analogJoyTrigger2) ? 0x20 : 0;
    break;

  case Layout::AnalogGun:
  {
    // Rail Chase 2, Gunblade NY, Behind Enemy Lines: a gun on a stick per
    // player, its position on the analog channels (MAME's ranges); triggers
    // IN1 bits 0/1, missiles (Behind Enemy Lines) bits 4/5
    in0 |= sw(m_inputs->start[1]) ? 0x20 : 0;
    uint8_t x[2], y[2];
    for (int p = 0; p < 2; p++)
    {
      x[p] = axis(m_inputs->analogGunX[p]);
      y[p] = axis(m_inputs->analogGunY[p]);
      in1 |= sw(m_inputs->analogTriggerLeft[p]) ? (1 << p) : 0;
      in1 |= sw(m_inputs->analogTriggerRight[p]) ? (0x10 << p) : 0;
    }
    if (is("rchase2a"))
    {
      m_analog[0] = x[1]; m_analog[1] = x[0]; m_analog[2] = y[1]; m_analog[3] = y[0];
    }
    else if (is("rchase2"))
    {
      m_analog[0] = range(x[1], 0x34, 0xC7, true);
      m_analog[1] = range(x[0], 0x3A, 0xCA, true);
      m_analog[2] = range(y[1], 0x1C, 0xCB, true);
      m_analog[3] = range(y[0], 0x1C, 0xCB, true);
    }
    else
    {
      m_analog[0] = range(x[0], 0x69, 0xFF);
      m_analog[1] = range(x[1], 0x00, 0x96);
      m_analog[2] = range(y[0], 0x11, 0xAE);
      m_analog[3] = range(y[1], 0x11, 0xAE);
    }
    break;
  }

  case Layout::PowerSled:
    // Power Sled: the right and left pedals of each sled on channels 1/3
    // (P1) and 5/7 (P2); entry and call IN1 bits 0/1 (P1), 2/3 (P2)
    m_analog[1] = accel;
    m_analog[3] = brake;
    in1 |= sw(m_inputs->punch[0]) ? 0x01 : 0;
    in1 |= sw(m_inputs->kick[0]) ? 0x02 : 0;
    in1 |= sw(m_inputs->punch[1]) ? 0x04 : 0;
    in1 |= sw(m_inputs->kick[1]) ? 0x08 : 0;
    in0 |= sw(m_inputs->escape[0]) ? 0x80 : 0;   // cancel error
    break;

  case Layout::Ski:
    // Ski controls (Sega Ski Super G, MAME's skisuprg ports): swing on analog
    // channel 0, inclining on 1 (as Model 3's Ski Champ: Y then X); select
    // 1/2/3 in IN0 bits 6/7/4 (no start button: bit 4 is select 3), zoom in
    // (pole right) IN0 bit 5, zoom out (pole left) IN1 bit 0. The foot sensors
    // come from the drive board (DriveBoardRead).
    m_analog[0] = m_inputs->skiY ? (uint8_t) m_inputs->skiY->value : 0x80;
    m_analog[1] = m_inputs->skiX ? (uint8_t) m_inputs->skiX->value : 0x80;
    in0 &= ~0x10;
    in0 |= sw(m_inputs->skiSelect1) ? 0x40 : 0;
    in0 |= sw(m_inputs->skiSelect2) ? 0x80 : 0;
    in0 |= sw(m_inputs->skiSelect3) ? 0x10 : 0;
    in0 |= sw(m_inputs->skiPollRight) ? 0x20 : 0;
    in1 |= sw(m_inputs->skiPollLeft) ? 0x01 : 0;
    break;

  case Layout::WaveRunner:
    // Wave Runner (MAME's waverunr ports): handle bar (the wheel), roll
    // (joystick X), throttle (accelerator) and pitch (joystick Y) on analog
    // channels 0-3, throttle and pitch reversed; start is bit 6, view IN1
    // bit 0. IN2 bit 3 (MAME: active high, a safety sensor) reads 0.
    m_analog[0] = steer;
    m_analog[1] = axis(m_inputs->analogJoyX);
    m_analog[2] = (uint8_t) (0xFF - accel);
    m_analog[3] = (uint8_t) (0xFF - axis(m_inputs->analogJoyY));
    if (in0 & 0x10)
      in0 = (in0 & ~0x10) | 0x40;
    in1 |= sw(m_inputs->viewChange) ? 0x01 : 0;
    in2 |= 0x08;
    break;

  case Layout::WaterSki:
    // Sega Water Ski: slide on channel 0 (joystick X); no start button: set
    // IN1 bit 0 (guard), select up IN1 bit 1, select down IN0 bit 6, pitch
    // left/right IN1 bits 2/3 (punch, kick)
    m_analog[0] = axis(m_inputs->analogJoyX);
    in0 &= ~0x10;
    in0 |= sw(m_inputs->down[0]) ? 0x40 : 0;
    in1 |= sw(m_inputs->guard[0]) || sw(m_inputs->start[0]) ? 0x01 : 0;
    in1 |= sw(m_inputs->up[0]) ? 0x02 : 0;
    in1 |= sw(m_inputs->punch[0]) ? 0x04 : 0;
    in1 |= sw(m_inputs->kick[0]) ? 0x08 : 0;
    break;

  case Layout::TopSkater:
    // Top Skater: curving (joystick X) and slide (joystick Y) on channels
    // 0/1; select right/left IN0 bits 4/7, jump front IN0 bit 5 (punch), start
    // bit 6, jump tail IN1 bit 0 (kick)
    m_analog[0] = axis(m_inputs->analogJoyX);
    m_analog[1] = axis(m_inputs->analogJoyY);
    in0 &= ~0x10;
    in0 |= sw(m_inputs->start[0]) ? 0x40 : 0;
    in0 |= sw(m_inputs->right[0]) ? 0x10 : 0;
    in0 |= sw(m_inputs->left[0]) ? 0x80 : 0;
    in0 |= sw(m_inputs->punch[0]) ? 0x20 : 0;
    in1 |= sw(m_inputs->kick[0]) ? 0x01 : 0;
    break;

  case Layout::Gun:
  {
    // Light guns (The House of the Dead, Virtua Cop, Virtua Cop 2): triggers
    // in IN1 bits 0-1, the second start button in IN0 bit 5; Supermodel's gun
    // range ([150,651] x [80,465]) scaled to MAME's port ranges. Shooting off
    // screen (the inputs' off-screen trigger) reloads. Virtua Cop's I/O board
    // passes them on in its DPRAM (UpdateIOBoard).
    const bool vcop2 = is("vcop2");
    const bool vcop = is("vcop");
    for (int g = 0; g < 2; g++)
    {
      if (!(inputs & (g == 0 ? Game::INPUT_GUN1 : Game::INPUT_GUN2)) || !m_inputs->gunX[g] || !m_inputs->gunY[g] || !m_inputs->trigger[g])
        continue;
      const int xMin = vcop2 ? (g == 0 ? 137 : 134) : vcop ? (g == 0 ? 131 : 128) : (g == 0 ? 173 : 163);
      const int xMax = vcop2 ? (g == 0 ? 630 : 627) : vcop ? (g == 0 ? 630 : 627) : 596;
      const int yMin = vcop2 ? 36 : vcop ? (g == 0 ? 36 : 39) : 87, yMax = vcop2 || vcop ? 425 : 380;
      int x = std::clamp(int(m_inputs->gunX[g]->value), 150, 651);
      int y = std::clamp(int(m_inputs->gunY[g]->value), 80, 465);
      m_gun[g * 2 + 1] = (uint16_t) (xMin + (x - 150) * (xMax - xMin) / (651 - 150));
      m_gun[g * 2 + 0] = (uint16_t) (yMin + (y - 80) * (yMax - yMin) / (465 - 80));
      // the off-screen input alone reloads: it also pulls the trigger
      in1 |= (m_inputs->trigger[g]->value || m_inputs->trigger[g]->offscreenValue) ? (1 << g) : 0;
      // off screen: the off-screen input, or (as MAME does) the gun within 5% of
      // an edge of its range, e.g. a mouse moved out of the window
      const int borderX = (651 - 150) / 20, borderY = (465 - 80) / 20;
      bool offscreen = m_inputs->trigger[g]->offscreenValue != 0 ||
        x <= 150 + borderX || x >= 651 - borderX || y <= 80 + borderY || y >= 465 - borderY;
      m_gunOffscreen = (uint8_t) ((m_gunOffscreen & ~(1 << g)) | ((offscreen ? 1 : 0) << g));
      if (g == 1)
        in0 |= sw(m_inputs->start[1]) ? 0x20 : 0;
    }
    break;
  }
  }

  // 4-speed shifter (Daytona, Sega Rally; MAME's daytona_gearbox_r): the gear
  // stays selected, IN1 bits 4-6 active high
  if ((inputs & Game::INPUT_SHIFT4) && m_inputs->gearShift4)
  {
    static const uint8_t gearValue[5] = { 0, 2, 1, 6, 5 };
    int gear = m_inputs->gearShift4->value;
    if (gear >= 0 && gear <= 4)
      m_gearSel = (uint8_t) gear;
    in1 |= gearValue[m_gearSel] << 4;
  }

  if (m_original)
  {
    UpdateIOBoard(m_analog, in0, in1, in2);
    return;
  }

  m_in[0] = ~in0;
  m_in[1] = (inputs & Game::INPUT_SHIFT4) ? (uint8_t) ((~in1 & 0x8f) | (in1 & 0x70)) : (uint8_t) ~in1;   // gear bits are active high
  m_in[2] = ~in2;
}

/******************************************************************************
 Protection and per-game extras (MAME's model2.cpp)

 - 315-5881 (Dynamite Cop, Zero Gunner, Pilot Kids; key in Games.xml):
   MAME's model2_5881_mem. 64 KB of RAM at 1D80000 that the chip decrypts
   from (byte-swapped words), its registers at 1D90000 (iomap_le): 0 ready,
   8 address low, A address high, C sub-key, E decrypted data (byte-swapped;
   the first read after an address write is a dummy 0). Model 3's CCrypto.
 - 317-0229 (Dead or Alive): MAME's model2_0229_mem. 32 KB of source RAM at
   1D80000, overlaid at 1D87FF0 (source address), 1D87FF4 (tables), 1D87FF8
   (data; the first read after a source write is a dummy 0); 1D8400C a
   status that toggles.
 - Daytona USA To The MAXX: reads at 240000-24FFFF (maxx_r).
 - Daytona USA GTX 2004: a banked protection ROM at 2C00000 (gtx_r).
 - Program patches (MAME's driver inits): Zero Gunner and Pilot Kids
   destroy their interrupt table, Power Sled (Main) is the relay set
   switched to main.
 - Last Bronx: the simulated geometrizer's mesh commands.
******************************************************************************/

void CModel2::InitExtras(const Game &game, const ROMSet &rom_set)
{
  auto is = [&](const char *n) { return game.name == n || game.parent == n; };
  auto patch32 = [&](uint32_t addr, uint32_t value)
  {
    if (addr + 4 <= m_program.size())
      for (int i = 0; i < 4; i++)
        m_program[addr + i] = uint8_t(value >> (8 * i));
  };

  m_extra = Extra::None;
  if (game.encryption_key)
  {
    m_extra = Extra::Crypt5881;
    m_protRAM.assign(0x10000, 0);
    m_crypt5881.Init(game.encryption_key, [this](uint32_t a) -> uint16_t
    {
      // MAME's crypt_read_callback: the word at 1D80000 + 2a, byte-swapped
      size_t o = (size_t(a) * 2) & 0xfffe;
      return uint16_t((m_protRAM[o] << 8) | m_protRAM[o + 1]);
    });
  }
  else if (is("doa"))
  {
    m_extra = Extra::Comp0229;
    m_protRAM.assign(0x8000, 0);
    m_comp0229.Init(Model2::C315_5838::Variant::SEGA_317_0229, [this](uint32_t a) -> uint16_t
    {
      // a 32 KB window of little-endian i960 RAM
      size_t o = (size_t(a) & 0x3fff) * 2;
      return uint16_t(m_protRAM[o] | (m_protRAM[o + 1] << 8));
    });
  }
  else if (game.name == "daytonam")
    m_extra = Extra::DaytonaMaxx;
  else if (game.name == "daytonagtx")
  {
    m_extra = Extra::DaytonaGtx;
    ROM prot = rom_set.get_rom("prot_data");
    m_protData.assign(0x200000, 0xFF);
    if (prot.size)
      prot.CopyTo(m_protData.data(), std::min<size_t>(prot.size, m_protData.size()));
  }

  if (is("zerogun"))
    patch32(0x700, 0x08000004);
  if (is("pltkids"))
    patch32(0x730, 0x08000004);
  if (game.name == "powsledm" && m_program.size() > 0x1585d)
  {
    m_program[0x1571c] = 0x01;   // main mode
    m_program[0x1584c] = 0x89;   // node ID 0x200 = main
    m_program[0x1585d] = 0xfd;   // inverted node ID
  }
  m_video3d.SetGeoMesh(is("lastbrnx"));
  m_video3d.SetGeoTopSkater(is("topskatr"));
}

bool CModel2::ExtraRead(uint32_t addr, uint32_t mask, uint32_t *data)
{
  switch (m_extra)
  {
  case Extra::Crypt5881:
    if (addr >= 0x01d80000 && addr < 0x01d90000)
    {
      *data = Get32(m_protRAM, addr - 0x01d80000);
      return true;
    }
    if (addr >= 0x01d90000 && addr < 0x01da0000)
    {
      uint32_t d = 0;
      if ((addr & 0xffff) == 0x000c && (mask & 0xffff0000))
      {
        // decrypt_le_r
        uint16_t v = 0;
        if (m_cryptFirstRead)
          m_cryptFirstRead = false;
        else
        {
          uint8_t *base = nullptr;
          v = m_crypt5881.Decrypt(&base);
          v = uint16_t((v >> 8) | (v << 8));
        }
        d |= uint32_t(v) << 16;
      }
      *data = d;   // ready (0: not busy) and the write-only registers read 0
      return true;
    }
    return false;

  case Extra::Comp0229:
    if (addr >= 0x01d80000 && addr < 0x01d88000)
    {
      if (addr == 0x01d87ff8)
      {
        // doa_prot_r, in each 16-bit lane read
        uint32_t d = 0;
        for (int lane = 0; lane < 2; lane++)
        {
          if (!(mask & (0xffffu << (16 * lane))))
            continue;
          uint16_t v = 0;
          if (m_doaDummy)
            m_doaDummy = false;
          else
            v = m_comp0229.DataRead();
          d |= uint32_t(v) << (16 * lane);
        }
        *data = d;
        return true;
      }
      if (addr == 0x01d8400c)
      {
        m_doaStatus = !m_doaStatus;   // doa_unk_r
        *data = m_doaStatus ? 0xffff : 0xfff0;
        return true;
      }
      *data = Get32(m_protRAM, addr - 0x01d80000);
      return true;
    }
    return false;

  case Extra::DaytonaMaxx:
    if (addr >= 0x00240000 && addr < 0x00250000)
    {
      uint32_t offset = (addr - 0x00240000) >> 2;
      if (offset <= 0x1f / 4)
      {
        if (mask == 0xffff0000)
        {
          // 16-bit protection reads
          m_maxxState = (m_maxxState + 1) & 0x0f;
          *data = !m_maxxState ? 0x00070000 : (m_maxxState & 0x2) ? 0 : 0x00040000;
          return true;
        }
        if (mask == 0xffffffff && offset == 0x22 / 4)
        {
          *data = 0x00ff0000;
          return true;
        }
      }
      *data = Get32(m_program, 0x040000 + (addr - 0x00240000));
      return true;
    }
    return false;

  case Extra::DaytonaGtx:
    if (addr >= 0x02c00000 && addr < 0x02d00000)
    {
      uint32_t d = 0;
      for (int i = 0; i < 4; i++)
      {
        if (!(mask & (0xffu << (8 * i))))
          continue;
        uint32_t offset = addr - 0x02c00000 + i;
        if (offset == 0xffffc)
          m_gtxState = 2;            // protection ROM overlay off
        else if (offset == 0xff00c || offset == 0xf0003)
          m_gtxState = 0;            // bank 0
        else if (offset == 0xff000)
          m_gtxState = 1;            // bank 1
        size_t o = size_t(m_gtxState) * 0x100000 + offset;
        uint8_t b = o < m_protData.size() ? m_protData[o] : 0xFF;
        d |= uint32_t(b) << (8 * i);
      }
      *data = d;
      return true;
    }
    return false;

  default:
    return false;
  }
}

bool CModel2::ExtraWrite(uint32_t addr, uint32_t data, uint32_t mask)
{
  switch (m_extra)
  {
  case Extra::Crypt5881:
    if (addr >= 0x01d80000 && addr < 0x01d90000)
    {
      uint32_t o = addr - 0x01d80000;
      uint32_t v = Get32(m_protRAM, o);
      v = (v & ~mask) | (data & mask);
      for (int i = 0; i < 4; i++)
        m_protRAM[o + i] = uint8_t(v >> (8 * i));
      return true;
    }
    if (addr >= 0x01d90000 && addr < 0x01da0000)
    {
      switch (addr & 0xffff)
      {
      case 0x0008:
        if (mask & 0x0000ffff)
        {
          m_crypt5881.SetAddressLow(uint16_t(data));   // addrlo_w
          m_cryptFirstRead = true;
        }
        if (mask & 0xffff0000)
        {
          m_crypt5881.SetAddressHigh(0);               // addrhi_w
          m_cryptFirstRead = true;
        }
        break;
      case 0x000c:
        if (mask & 0x0000ffff)
          m_crypt5881.SetSubKey(uint16_t(data));       // subkey_le_w
        break;
      default:
        break;
      }
      return true;
    }
    return false;

  case Extra::Comp0229:
    if (addr >= 0x01d80000 && addr < 0x01d88000)
    {
      if (addr == 0x01d87ff0)
      {
        m_comp0229.SourceWrite(data, mask);   // doa_source_w
        m_doaDummy = true;
        return true;
      }
      if (addr == 0x01d87ff4)
      {
        // table_w: a 16-bit handler, offset 0 in the low lane, 1 in the high
        if (mask & 0x0000ffff)
          m_comp0229.TableWrite(0, uint16_t(data));
        if (mask & 0xffff0000)
          m_comp0229.TableWrite(1, uint16_t(data >> 16));
        return true;
      }
      uint32_t o = addr - 0x01d80000;
      uint32_t v = Get32(m_protRAM, o);
      v = (v & ~mask) | (data & mask);
      for (int i = 0; i < 4; i++)
        m_protRAM[o + i] = uint8_t(v >> (8 * i));
      return true;
    }
    return false;

  default:
    return false;
  }
}

void CModel2::UpdateIOBoard(const uint8_t (&an)[8], uint8_t in0, uint8_t in1, uint8_t in2)
{
  for (int i = 0; i < 8; i++)
    m_dpram[i] = an[i];
  // the gear bits (Daytona's 4-speed shifter) are active high; the other
  // games' IN1 bits 4-6 are buttons, active low (Desert Tank: machine gun,
  // cannon)
  m_dpram[8] = (uint8_t) ~in0;
  m_dpram[9] = (m_game.inputs & Game::INPUT_SHIFT4) ? (uint8_t) ((~in1 & 0x8f) | (in1 & 0x70)) : (uint8_t) ~in1;
  m_dpram[10] = (uint8_t) ~in2;
  // the board's DIP switches (MAME: three banks, all off)
  m_dpram[0x0B] = m_dpram[0x0C] = m_dpram[0x0D] = 0xFF;
  // a fourth input port (Desert Tank reads it at boot; 0 there keeps it in
  // test mode): nothing pressed
  m_dpram[0x0E] = m_dpram[0x0F] = 0xFF;
  // Light guns (Virtua Cop's model1io2 board; MAME's fpga_r 0-8, which its
  // BIOS copies to 80-88): P1 Y, P1 X, P2 Y, P2 X (16 bits, little endian),
  // then the off-screen bits (0: P1, 1: P2)
  if (m_game.inputs & (Game::INPUT_GUN1 | Game::INPUT_GUN2))
  {
    for (int i = 0; i < 4; i++)
    {
      m_dpram[0x80 + i * 2] = uint8_t(m_gun[i]);
      m_dpram[0x81 + i * 2] = uint8_t(m_gun[i] >> 8);
    }
    m_dpram[0x88] = 0xfc | m_gunOffscreen;
  }

  if (m_ioBoardBootFrames > 0)
  {
    m_ioBoardBootFrames--;
    m_dpram[0x21] = 0;
    return;
  }
  if (m_dpram[0x20])
  {
    if (m_logIO)
      DebugLog("I/O board command %02X (%02X %02X %02X %02X)\n", m_dpram[0x20], m_dpram[0x1A], m_dpram[0x1B], m_dpram[0x1C], m_dpram[0x1D]);
    m_dpram[0x20] = 0;
  }
  m_dpram[0x21] = 0x40;
}


/******************************************************************************
 Running
******************************************************************************/

void CModel2::RunLine(void)
{
  m_lineEnd += LINE_TIME;

  for (int guard = 0; guard < 100000; guard++)
  {
    bool ready[2];
    ready[CPU_I960] = !m_halted[CPU_I960] && m_time[CPU_I960] < m_lineEnd;
    ready[CPU_COPRO] = !m_halted[CPU_COPRO] && !m_coproHeld && m_time[CPU_COPRO] < m_lineEnd;
    if (!ready[CPU_I960] && !ready[CPU_COPRO])
      break;

    CPUID cpu = (ready[CPU_I960] && (!ready[CPU_COPRO] || m_time[CPU_I960] <= m_time[CPU_COPRO])) ? CPU_I960 : CPU_COPRO;
    CPUID other = cpu == CPU_I960 ? CPU_COPRO : CPU_I960;
    int64_t limit = m_lineEnd;
    if (ready[other])
      limit = std::min(limit, m_time[other] + QUANTUM);

    m_running = cpu;
    if (cpu == CPU_I960)
    {
      int cycles = (int) std::max<int64_t>(1, (limit - m_time[cpu] + I960_TIME - 1) / I960_TIME);
      int done = m_i960.Run(cycles);
      m_time[cpu] += int64_t(std::max(done, 1)) * I960_TIME;
      RunTimers();
    }
    else
    {
      const int unit = m_model2c ? TGPX4_TIME : m_model2b ? SHARC_TIME : TGP_TIME;
      int cycles = (int) std::max<int64_t>(1, (limit - m_time[cpu] + unit - 1) / unit);
      int done = m_model2c ? m_tgpx4.Run(cycles) : m_model2b ? m_sharc.Run(cycles) : m_tgp.Run(cycles);
      m_time[cpu] += int64_t(std::max(done, 1)) * unit;
    }
    m_running = CPU_NONE;
  }

  // Halted or held CPUs idle until the end of the line
  for (int i = 0; i < 2; i++)
    if (m_halted[i] || (i == CPU_COPRO && m_coproHeld))
      m_time[i] = std::max(m_time[i], m_lineEnd);
  RunTimers();

  // UART clock
  m_uartClockFrac += LINE_TIME;
  unsigned clocks = m_uartClockFrac / UART_TIME;
  m_uartClockFrac %= UART_TIME;
  if (clocks)
    m_uart.Clock(clocks);
}

void CModel2::BuildVideoFrame(void)
{
  const size_t pixels = size_t(SCREEN_WIDTH) * SCREEN_HEIGHT;
  m_video.valid = true;
  m_video.software = m_softwareRender;
  m_video.geometry.Clear();
  m_video.model2 = nullptr;
  m_video.model2Memory = nullptr;

  // If the scroll colour table was written, the whole palette changes
  if (m_paletteDirty)
  {
    for (unsigned i = 0; i < 0x1000; i++)
      UpdatePaletteEntry(i);
    m_paletteDirty = false;
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

  if (!m_softwareRender)
  {
    // The GPU draws the 3D between the layers
    if (!m_renderTestMode)
    {
      // Wide screen: the renderer draws beyond the 4:3 picture, so viewports
      // at its left and right edges clip that much further out (from the
      // next frame's polygons on)
      m_video3d.SetWideExtension(m_render ? (int) std::ceil(m_render->WideExtension()) : 0);
      m_video.model2 = &m_video3d.BuildGPU();
      m_video.model2Quads = m_video3d.GPUQuads();
      m_video.model2Memory = &m_videoMemory;
    }
    return;
  }

  // MAME's screen_update(): tilemaps below, the 3D, tilemaps above
  std::vector<uint32_t> &img = m_video.softwareImage;
  img = m_video.back;
  if (!m_renderTestMode)
    m_video3d.Render(img.data(), SCREEN_WIDTH, SCREEN_HEIGHT);
  for (size_t i = 0; i < pixels; i++)
    if (m_video.hud[i] & 0xFF000000)
      img[i] = m_video.hud[i];
  for (auto &c : img)
    c |= 0xFF000000;
}

void CModel2::RunFrame(void)
{
  UpdateInputs();

  for (m_line = 0; m_line < TOTAL_LINES; m_line++)
  {
    if (m_line == VBLANK_LINE)
    {
      // The communication board's vblank tick (MAME's comm_tick)
      m_comm.Tick();
      // Debugging aid: "frame:prefix" writes texture RAM 0/1 and luma RAM to prefix_tex0.bin etc.
      if (const char *dump = getenv("SUPERMODEL_MODEL2_TEXDUMP"))
      {
        unsigned frame = 0;
        char prefix[1000] = { 0 };
        if (sscanf(dump, "%u:%999s", &frame, prefix) == 2 && frame == (unsigned) m_frameNumber)
        {
          const struct { const char *name; const void *data; size_t size; } parts[] = {
            { "tex0", m_textureRAM[0].data(), m_textureRAM[0].size() * 4 }, { "tex1", m_textureRAM[1].data(), m_textureRAM[1].size() * 4 },
            { "luma", m_lumaRAM.data(), m_lumaRAM.size() } };
          for (const auto &part : parts)
          {
            char file[1024];
            snprintf(file, sizeof(file), "%s_%s.bin", prefix, part.name);
            if (FILE *f = fopen(file, "wb"))
            {
              fwrite(part.data, 1, part.size, f);
              fclose(f);
            }
          }
        }
      }
      // Debugging aid: "frame:file" writes buffer RAM (the display list) at this vblank
      if (const char *dump = getenv("SUPERMODEL_MODEL2_BUFDUMP"))
      {
        unsigned frame = 0;
        char file[1024] = { 0 };
        if (sscanf(dump, "%u:%1023s", &frame, file) == 2 && frame == (unsigned) m_frameNumber)
          if (FILE *f = fopen(file, "wb"))
          {
            fwrite(m_bufferRAM.data(), 4, m_bufferRAM.size(), f);
            fclose(f);
          }
      }
      // The geometrizer starts a frame: in 60 Hz mode, or on even frames
      m_vblankFrame = (uint32_t) m_frameNumber;
      if ((m_videoControl & 1) == 0 || (m_vblankFrame & 1) == 0)
      {
        if (m_geoLLE)
          RunGeometrizer();
        else
          m_video3d.GeoParse(m_geoReadAddress);
      }
      if (m_intEna & 1)
      {
        m_intReq |= 1;
        IRQUpdate();
      }
    }
    RunLine();
  }

  if (m_wheelBoard)
    m_wheelBoard->RunFrame();

  BuildVideoFrame();
  m_frameNumber++;

  if (m_logIO && (m_frameNumber % 60) == 0)
    DebugLog("Model 2 frame %llu: i960 IP=%08X%s%s copro PC=%05X%s%s FIFO in=%u out=%u IRQ req=%03X ena=%03X video=%X geo read=%05X write=%05X\n",
      (unsigned long long) m_frameNumber, m_i960.GetIP(), m_halted[CPU_I960] ? " (halted)" : "", m_i960.IsFatalHalted() ? " (FATAL)" : "",
      m_model2c ? (unsigned) m_tgpx4.GetPC() : m_model2b ? (unsigned) m_sharc.GetPC() : (unsigned) m_tgp.GetPC(), m_halted[CPU_COPRO] ? " (halted)" : "", m_coproHeld ? " (held)" : "",
      (unsigned) m_fifoIn.values.size(), (unsigned) m_fifoOut.values.size(), m_intReq, m_intEna, m_videoControl, m_geoReadAddress, m_geoWriteAddress);

  if (m_soundBoard)
    m_soundBoard->RunFrame();
  if (m_m1SoundBoard && !m_m1SoundMixer)
    m_m1SoundBoard->RunFrame();     // (mixed in by the SCSP board otherwise)

  // Debugging aid: "frame:file" writes that frame to a BMP file
  if (const char *dump = getenv("SUPERMODEL_MODEL2_DUMP"))
  {
    unsigned frame = 0;
    char file[1024] = { 0 };
    if (m_render && sscanf(dump, "%u:%1023s", &frame, file) == 2 && frame == (unsigned) m_frameNumber)
      m_render->RequestDump(file);
  }
  // Debugging aid: "n:prefix" writes every n-th frame to prefix_<frame>.bmp
  if (const char *every = getenv("SUPERMODEL_MODEL2_DUMP_EVERY"))
  {
    unsigned n = 0;
    char prefix[1000] = { 0 };
    if (m_render && sscanf(every, "%u:%999s", &n, prefix) == 2 && n && (m_frameNumber % n) == 0)
    {
      char file[1024];
      snprintf(file, sizeof(file), "%s_%05u.bmp", prefix, (unsigned) m_frameNumber);
      m_render->RequestDump(file);
    }
  }

  RenderFrame();
}

void CModel2::RenderFrame(void)
{
  if (BeginFrameVideo() && m_render)
  {
    m_render->DrawFrame(m_video);
    if (m_superAA)
      m_superAA->Draw();
  }
  EndFrameVideo();
}

void CModel2::Reset(void)
{
  if (m_extra == Extra::Crypt5881)
    m_crypt5881.Reset();
  if (m_extra == Extra::Comp0229)
    m_comp0229.Reset();
  m_cryptFirstRead = true;
  m_doaDummy = m_doaStatus = false;
  m_maxxState = m_gtxState = 0;
  m_intReq = 0;
  m_intEna = 0;
  m_coproCtl = 0;
  m_coproCount = 0;
  m_geoCtl = 0;
  m_geoCount = 0;
  m_ctrlMode = 0;
  for (int i = 0; i < 4; i++)
  {
    m_timerVal[i] = 0xfffff;
    m_timerOrig[i] = 0;
    m_timerRun[i] = false;
  }
  m_uart.Reset();
  m_uart.WriteCTS(0);
  m_io = IOChip();
  m_eeprom.Reset();
  {
    DriveBoard board;
    board.present = m_driveBoard.present;
    board.ski = m_driveBoard.ski;
    m_driveBoard = board;
  }

  // MAME: "initialize bufferram to a sane default"
  std::fill(m_bufferRAM.begin(), m_bufferRAM.end(), 0x07800f0f);

  m_fifoIn.values.clear();
  m_fifoIn.extra.clear();
  m_fifoOut.values.clear();
  m_fifoOut.extra.clear();
  for (int i = 0; i < 2; i++)
  {
    m_halted[i] = false;
    m_emptyTriggered[i] = false;
    m_fullTriggered[i] = false;
    m_time[i] = 0;
  }
  m_geoWriteAddress = 0;
  m_geoReadAddress = 0;
  m_coproBank = 0;
  m_coproSinCosBase = m_coproInvBase = m_coproISqrtBase = 0;
  std::fill(std::begin(m_coproAtanBase), std::end(m_coproAtanBase), 0);
  m_coproHeld = true;   // until there's a program
  m_geoProgram.clear();
  m_lineEnd = 0;
  m_uartClockFrac = 0;

  m_i960.Reset();
  m_tgp.Reset();
  m_tgpx4.Reset();          // held (m_coproHeld) until the i960 boots it
  if (m_model2b)
  {
    // MAME: halted, FIFO in empty, FIFO out not full
    m_sharc.Reset();
    m_sharc.SetHalt(true);
    UpdateSHARCFlags();
    m_geoSharc.Reset();
    m_geoSharc.SetHalt(true);
    std::fill(m_geoPolyRAM.begin(), m_geoPolyRAM.end(), 0);
    m_geoROMAddress = 0;
  }
  m_tiles.Reset();
  m_video3d.Reset();
  if (m_soundBoard)
    m_soundBoard->Reset();
  if (m_m1SoundBoard)
    m_m1SoundBoard->Reset();
  m_ioBoardBootFrames = IO_BOARD_BOOT_FRAMES;
  m_gearSel = 0;
  m_comm.Reset();
  if (m_wheelBoard)
    m_wheelBoard->Reset();
  IRQUpdate();

  DebugLog("Model 2 reset\n");
}


/******************************************************************************
 Save states and NVRAM
******************************************************************************/

void CModel2::SaveState(CBlockFile *SaveState)
{
  SaveState->NewBlock("Model 2", __FILE__);
  WriteVector(SaveState, m_ram);
  WriteVector(SaveState, m_workRAM);
  WriteVector(SaveState, m_bufferRAM);
  WriteVector(SaveState, m_backupRAM);
  WriteVector(SaveState, m_paletteRAM);
  WriteVector(SaveState, m_colorXlat);
  WriteVector(SaveState, m_textureRAM[0]);
  WriteVector(SaveState, m_textureRAM[1]);
  WriteVector(SaveState, m_lumaRAM);
  WriteVector(SaveState, m_fbVRAM[0]);
  WriteVector(SaveState, m_fbVRAM[1]);
  WriteVector(SaveState, m_waitStateRAM);
  WriteVector(SaveState, m_tgpProgram);
  WriteVector(SaveState, m_dpram);

  auto saveFIFO = [&](const CFIFO &f)
  {
    uint32_t n = (uint32_t) f.values.size(), e = (uint32_t) f.extra.size();
    SaveState->Write(n);
    for (uint32_t v : f.values)
      SaveState->Write(v);
    SaveState->Write(e);
    for (uint32_t v : f.extra)
      SaveState->Write(v);
  };
  saveFIFO(m_fifoIn);
  saveFIFO(m_fifoOut);
  SaveState->Write(m_halted);
  SaveState->Write(m_emptyTriggered);
  SaveState->Write(m_fullTriggered);
  SaveState->Write(m_coproHeld);
  SaveState->Write(m_coproCtl);
  SaveState->Write(m_coproCount);
  SaveState->Write(m_coproBank);
  SaveState->Write(m_coproSinCosBase);
  SaveState->Write(m_coproInvBase);
  SaveState->Write(m_coproISqrtBase);
  SaveState->Write(m_coproAtanBase);
  SaveState->Write(m_geoCtl);
  SaveState->Write(m_geoCount);
  SaveState->Write(m_geoWriteAddress);
  SaveState->Write(m_geoReadAddress);
  SaveState->Write(m_videoControl);
  SaveState->Write(m_vblankFrame);
  SaveState->Write(m_renderTestMode);
  SaveState->Write(m_renderMode);
  SaveState->Write(m_renderUnk);
  SaveState->Write(m_intReq);
  SaveState->Write(m_intEna);
  SaveState->Write(m_timerVal);
  SaveState->Write(m_timerOrig);
  SaveState->Write(m_timerRun);
  SaveState->Write(m_timerStart);
  SaveState->Write(m_io);
  SaveState->Write(m_ctrlMode);
  SaveState->Write(m_time);
  SaveState->Write(m_lineEnd);
  SaveState->Write(m_uartClockFrac);
  SaveState->Write(m_frameNumber);
  SaveState->Write(m_gearSel);
  SaveState->Write(m_lightgunMux);

  m_i960.SaveState(SaveState, "Model 2 i960");
  m_tgp.SaveState(SaveState, "Model 2 TGP");
  if (m_model2c)
    m_tgpx4.SaveState(SaveState, "Model 2 TGPx4");
  if (m_model2b)
  {
    m_sharc.SaveState(SaveState, "Model 2 SHARC");
    m_geoSharc.SaveState(SaveState, "Model 2 geometrizer SHARC");
    SaveState->NewBlock("Model 2 geometrizer", __FILE__);
    WriteVector(SaveState, m_geoPolyRAM);
    SaveState->Write(m_geoROMAddress);
    // Which geometrizer made the state: the program's SHARC state only exists
    // with the low level one, so the two can't load each other's states
    SaveState->NewBlock("Model 2 geometrizer mode", __FILE__);
    SaveState->Write(uint8_t(m_geoLLE ? 1 : 0));
  }
  m_tiles.SaveState(SaveState);
  m_video3d.SaveState(SaveState);
  m_uart.SaveState(SaveState, "Model 2 UART");
  m_comm.SaveState(SaveState);
  m_eeprom.SaveState(SaveState);
  if (m_extra != Extra::None)
  {
    SaveState->NewBlock("Model 2 protection", __FILE__);
    WriteVector(SaveState, m_protRAM);
    SaveState->Write(m_cryptFirstRead);
    SaveState->Write(m_doaDummy);
    SaveState->Write(m_doaStatus);
    SaveState->Write(m_maxxState);
    SaveState->Write(m_gtxState);
    if (m_extra == Extra::Crypt5881)
      m_crypt5881.SaveState(SaveState);
    if (m_extra == Extra::Comp0229)
      m_comp0229.SaveState(SaveState, "Model 2 317-0229");
  }
  if (m_soundBoard)
    m_soundBoard->SaveState(SaveState);
  if (m_m1SoundBoard)
    m_m1SoundBoard->SaveState(SaveState);
  if (m_driveBoard.present)
  {
    SaveState->NewBlock("Model 2 drive board", __FILE__);
    SaveState->Write(m_driveBoard.initialized);
    SaveState->Write(m_driveBoard.initCommands);
    SaveState->Write(m_driveBoard.readMode);
  }
  if (m_wheelBoard)
    m_wheelBoard->SaveState(SaveState);
}

void CModel2::LoadState(CBlockFile *SaveState)
{
  // 2B: the state must come from the same geometrizer (low level or
  // simulation); states without the block were made by the simulation
  if (m_model2b)
  {
    uint8_t lle = 0;
    if (Result::OKAY == SaveState->FindBlock("Model 2 geometrizer mode"))
      SaveState->Read(lle);
    if ((lle != 0) != m_geoLLE)
    {
      ErrorLog("Unable to load Model 2 state: it was saved with the %s geometrizer, but the %s geometrizer is running "
        "(HighLevelGeometrizer=%d). Set HighLevelGeometrizer=%d to load it.",
        lle ? "low level" : "simulated", m_geoLLE ? "low level" : "simulated", m_geoLLE ? 0 : 1, lle ? 0 : 1);
      return;
    }
  }
  if (Result::OKAY != SaveState->FindBlock("Model 2"))
  {
    ErrorLog("Unable to load Model 2 state. Save state file is corrupt.");
    return;
  }
  ReadVector(SaveState, m_ram);
  ReadVector(SaveState, m_workRAM);
  ReadVector(SaveState, m_bufferRAM);
  ReadVector(SaveState, m_backupRAM);
  ReadVector(SaveState, m_paletteRAM);
  ReadVector(SaveState, m_colorXlat);
  ReadVector(SaveState, m_textureRAM[0]);
  ReadVector(SaveState, m_textureRAM[1]);
  ReadVector(SaveState, m_lumaRAM);
  ReadVector(SaveState, m_fbVRAM[0]);
  ReadVector(SaveState, m_fbVRAM[1]);
  ReadVector(SaveState, m_waitStateRAM);
  ReadVector(SaveState, m_tgpProgram);
  ReadVector(SaveState, m_dpram);
  m_ioBoardBootFrames = 0;

  auto loadFIFO = [&](CFIFO &f)
  {
    uint32_t n = 0, v = 0;
    f.values.clear();
    f.extra.clear();
    SaveState->Read(n);
    for (uint32_t i = 0; i < n; i++)
    {
      SaveState->Read(v);
      f.values.push_back(v);
    }
    SaveState->Read(n);
    for (uint32_t i = 0; i < n; i++)
    {
      SaveState->Read(v);
      f.extra.push_back(v);
    }
  };
  loadFIFO(m_fifoIn);
  loadFIFO(m_fifoOut);
  SaveState->Read(m_halted);
  SaveState->Read(m_emptyTriggered);
  SaveState->Read(m_fullTriggered);
  SaveState->Read(m_coproHeld);
  SaveState->Read(m_coproCtl);
  SaveState->Read(m_coproCount);
  SaveState->Read(m_coproBank);
  SaveState->Read(m_coproSinCosBase);
  SaveState->Read(m_coproInvBase);
  SaveState->Read(m_coproISqrtBase);
  SaveState->Read(m_coproAtanBase);
  SaveState->Read(m_geoCtl);
  SaveState->Read(m_geoCount);
  SaveState->Read(m_geoWriteAddress);
  SaveState->Read(m_geoReadAddress);
  SaveState->Read(m_videoControl);
  SaveState->Read(m_vblankFrame);
  SaveState->Read(m_renderTestMode);
  SaveState->Read(m_renderMode);
  SaveState->Read(m_renderUnk);
  SaveState->Read(m_intReq);
  SaveState->Read(m_intEna);
  SaveState->Read(m_timerVal);
  SaveState->Read(m_timerOrig);
  SaveState->Read(m_timerRun);
  SaveState->Read(m_timerStart);
  SaveState->Read(m_io);
  SaveState->Read(m_ctrlMode);
  SaveState->Read(m_time);
  SaveState->Read(m_lineEnd);
  SaveState->Read(m_uartClockFrac);
  SaveState->Read(m_frameNumber);
  SaveState->Read(m_gearSel);
  SaveState->Read(m_lightgunMux);

  m_i960.LoadState(SaveState, "Model 2 i960");
  m_tgp.LoadState(SaveState, "Model 2 TGP");
  if (m_model2c)
    m_tgpx4.LoadState(SaveState, "Model 2 TGPx4");
  if (m_model2b)
  {
    m_sharc.LoadState(SaveState, "Model 2 SHARC");
    m_geoSharc.LoadState(SaveState, "Model 2 geometrizer SHARC");
    if (Result::OKAY == SaveState->FindBlock("Model 2 geometrizer"))
    {
      ReadVector(SaveState, m_geoPolyRAM);
      SaveState->Read(m_geoROMAddress);
    }
  }
  m_tiles.LoadState(SaveState);
  m_video3d.LoadState(SaveState);
  m_uart.LoadState(SaveState, "Model 2 UART");
  m_comm.LoadState(SaveState);
  m_eeprom.LoadState(SaveState);
  if (m_extra != Extra::None && Result::OKAY == SaveState->FindBlock("Model 2 protection"))
  {
    ReadVector(SaveState, m_protRAM);
    SaveState->Read(m_cryptFirstRead);
    SaveState->Read(m_doaDummy);
    SaveState->Read(m_doaStatus);
    SaveState->Read(m_maxxState);
    SaveState->Read(m_gtxState);
    if (m_extra == Extra::Crypt5881)
      m_crypt5881.LoadState(SaveState);
    if (m_extra == Extra::Comp0229)
      m_comp0229.LoadState(SaveState, "Model 2 317-0229");
  }
  if (m_soundBoard)
    m_soundBoard->LoadState(SaveState);
  if (m_m1SoundBoard)
    m_m1SoundBoard->LoadState(SaveState);
  if (m_driveBoard.present && Result::OKAY == SaveState->FindBlock("Model 2 drive board"))
  {
    SaveState->Read(m_driveBoard.initialized);
    SaveState->Read(m_driveBoard.initCommands);
    SaveState->Read(m_driveBoard.readMode);
  }
  // (a state made without the board leaves it running as it is)
  if (m_wheelBoard && Result::OKAY == SaveState->FindBlock("DriveBoard.2"))
    m_wheelBoard->LoadState(SaveState);

  for (unsigned i = 0; i < m_paletteRAM.size(); i++)
    UpdatePaletteEntry(i);
  m_paletteDirty = false;

  // The frame shown: when paused, the main loop only draws it again
  BuildVideoFrame();
}

// NVRAM: the backup RAM and the EEPROM
void CModel2::SaveNVRAM(CBlockFile *NVRAM)
{
  NVRAM->NewBlock("Model 2 NVRAM", __FILE__);
  WriteVector(NVRAM, m_backupRAM);
  m_eeprom.SaveState(NVRAM);
  NVRAM->Write(&m_dpram[0x100], 0x80);
}

void CModel2::LoadNVRAM(CBlockFile *NVRAM)
{
  if (Result::OKAY != NVRAM->FindBlock("Model 2 NVRAM"))
  {
    ErrorLog("Unable to load Model 2 NVRAM. Save file is corrupt.");
    return;
  }
  ReadVector(NVRAM, m_backupRAM);
  m_eeprom.LoadState(NVRAM);
  NVRAM->Read(&m_dpram[0x100], 0x80);
}

void CModel2::ClearNVRAM(void)
{
  // MAME: nvram_device::DEFAULT_ALL_1
  std::fill(m_backupRAM.begin(), m_backupRAM.end(), 0xFF);
  m_eeprom.Clear();
  std::fill(m_dpram.begin() + 0x100, m_dpram.begin() + 0x180, 0xFF);
}


/******************************************************************************
 Initialization
******************************************************************************/

void CModel2::AttachRenderers(IRender2D *Render2DPtr, IRender3D *Render3DPtr, SuperAA *superAA)
{
  m_render = dynamic_cast<CModel1Render *>(Render2DPtr);
  m_superAA = superAA;
  if (!m_render)
    ErrorLog("Model 2: the 2D renderer is not a Model 1/2 renderer. Nothing will be drawn.");
  else
    m_render->SetWideScreen(m_config["WideScreen"].ValueAsDefault<bool>(false), m_config["WideBackground"].ValueAsDefault<bool>(false));
}

bool CModel2::LoadROMs(const ROMSet &rom_set)
{
  auto loadBytes = [&](const char *region, std::vector<uint8_t> &dest, size_t size, bool required) -> bool
  {
    dest.assign(size, 0);
    ROM rom = rom_set.get_rom(region);
    if (!rom.size)
    {
      if (required)
        ErrorLog("Model 2: ROM region '%s' is missing.", region);
      return !required;
    }
    rom.CopyTo(dest.data(), std::min(rom.size, size));
    return true;
  };

  auto loadWords = [&](const char *region, std::vector<uint32_t> &dest, size_t words, bool required) -> bool
  {
    ROM rom = rom_set.get_rom(region);
    if (!rom.size)
    {
      if (required)
        ErrorLog("Model 2: ROM region '%s' is missing.", region);
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

  if (!loadBytes("program", m_program, PROGRAM_SIZE, true))
    return false;
  if (!loadBytes("main_data", m_mainData, MAIN_DATA_SIZE, true))
    return false;
  // the TGP's function tables (2B has no TGP)
  if (!loadWords("copro_tables", m_coproTables, 0x10000, !m_model2b && !m_model2c))
    return false;
  if (m_coproTables.empty())
    m_coproTables.assign(0x10000, 0);
  // MAME's region is 8 MB whatever the ROMs fill: the TGP's reads are masked
  // to that, and past the data they read 0 (not the data again)
  loadWords("copro_data", m_coproData, 0x200000, false);
  if (!loadWords("polygons", m_polyROM, 0, true))
    return false;

  ROM textures = rom_set.get_rom("textures");
  m_textureROM.assign(0x1000000 / 2, 0xFFFF);
  if (textures.size)
    textures.CopyTo(reinterpret_cast<uint8_t *>(m_textureROM.data()), std::min<size_t>(textures.size, 0x1000000));

  if (m_original)
  {
    // The Model 1 sound board: program up to 0xC0000 bytes, two MultiPCM sample sets
    loadBytes("sound_program", m_soundProgram, 0xC0000, false);
    loadBytes("mpcm1", m_mpcm1, 0x400000, false);
    loadBytes("mpcm2", m_mpcm2, 0x400000, false);
    return true;
  }
  loadBytes("sound_program", m_soundProgram, 0x80000, false);
  // Some 2A cabinets (Manx TT DX) also have the Model 1 sound board
  if (rom_set.get_rom("m1_sound_program").size)
  {
    loadBytes("m1_sound_program", m_m1SoundProgram, 0xC0000, false);
    loadBytes("mpcm1", m_mpcm1, 0x400000, false);
    loadBytes("mpcm2", m_mpcm2, 0x400000, false);
  }
  // The sound board maps 16 MB of samples at 800000 with a bank bit choosing
  // the upper 8 MB, which no Model 2 game needs: both halves are the same
  loadBytes("samples", m_samples, 0x1000000, false);
  // Digital Sound Board (Sega Touring Car Championship: Z80, Top Skater: 68000)
  if (rom_set.get_rom("mpeg_program").size)
  {
    loadBytes("mpeg_program", m_dsbProgram, 0x20000, false);
    loadBytes("mpeg_music", m_dsbMusic, 0x1000000, false);
  }
  memcpy(&m_samples[0x800000], &m_samples[0], 0x800000);
  return true;
}

Result CModel2::Init(const Game &game, const ROMSet &rom_set)
{
  m_game = game;
  m_original = game.stepping == Game::Stepping::MODEL2;
  m_model2b = game.stepping == Game::Stepping::MODEL2B;
  m_model2c = game.stepping == Game::Stepping::MODEL2C;
  m_layout = LayoutOf(game.name, game.parent);
  m_driveBoard.present = game.driveboard_type == Game::DRIVE_BOARD_SKI || game.driveboard_type == Game::DRIVE_BOARD_MOTION;
  m_driveBoard.ski = game.driveboard_type == Game::DRIVE_BOARD_SKI;
  m_rchase2Drive = game.name == "rchase2" || game.parent == "rchase2";
  // The wheel drive board, if its ROM is there (optional, as Model 3's) and
  // force feedback is on (without, the game runs as with no board)
  m_wheelBoard.reset();
  m_wheelBoardReplies = false;
  if (game.driveboard_type == Game::DRIVE_BOARD_WHEEL && m_config["ForceFeedback"].ValueAsDefault<bool>(false))
  {
    ROM rom = rom_set.get_rom("driveboard_program");
    if (rom.size)
    {
      m_wheelBoardROM.assign(0x10000, 0xFF);
      rom.CopyTo(m_wheelBoardROM.data(), m_wheelBoardROM.size());
      // 2B/2C (Indy 500, Touring Car, Over Rev, Super GT 24h): EPR-18261
      m_wheelBoardReplies = m_model2b || m_model2c;
      if (m_wheelBoardReplies)
        m_wheelBoard = std::make_unique<CModel2DirectDriveBoard>(m_config);
      else
        m_wheelBoard = std::make_unique<CModel2DriveBoard>(m_config);
      if (m_wheelBoard->Init(m_wheelBoardROM.data()) != Result::OKAY)
        m_wheelBoard.reset();
      else
      {
        m_wheelBoard->AttachInputs(m_inputs, game.inputs);
        if (m_outputs)
          m_wheelBoard->AttachOutputs(m_outputs);
      }
    }
  }

  if (!LoadROMs(rom_set))
    return Result::FAIL;
  InitExtras(game, rom_set);

  m_ram.assign(0x40000, 0);
  m_workRAM.assign(0x100000, 0);
  m_bufferRAM.assign(0x20000 / 4, 0x07800f0f);
  m_backupRAM.assign(0x4000, 0xFF);
  m_paletteRAM.assign(0x4000 / 2, 0);
  m_palette.assign(0x4000 / 2, 0xFF000000);
  m_colorXlat.assign(0xC000 / 2, 0);
  m_textureRAM[0].assign(0x80000, 0);
  m_textureRAM[1].assign(0x80000, 0);
  m_lumaRAM.assign(0x8000, 0);
  m_fbVRAM[0].assign(0x80000 / 2, 0);
  m_fbVRAM[1].assign(0x80000 / 2, 0);
  m_waitStateRAM.assign(0x38, 0);
  m_tgpProgram.assign(0x1000, 0);
  // MAME's dual port RAM (and so the I/O board's view of it) starts at 0; Daytona
  // reads FF in some of it as a linked cabinet. The EEPROM area 100-17F comes from
  // the NVRAM (blank, FF, without one).
  m_dpram.assign(0x800, 0x00);
  std::fill(m_dpram.begin() + 0x100, m_dpram.begin() + 0x180, 0xFF);
  // The communication board: linked to the next machine with the network on
  // (the Network options, as the Model 3 net board), else to itself
  if (m_server && m_game.netboard_present)
    m_comm.Init(m_server, m_config["AddressOut"].ValueAs<std::string>(), (uint16_t) m_config["PortOut"].ValueAs<unsigned>(),
                m_config["Model2CommFrameSync"].ValueAsDefault<bool>(false));

  // MAME's colour space conversion ("optimal settings for vf2, fvipers and
  // schamp are bias 64, gain 51")
  for (int i = 0; i < 256; i++)
    m_gamma[i] = (uint8_t) std::max((i - 64.0) * 255.0 / 191.0, 0.0);

  m_i960.AttachBus(this);
  m_i960.SetFetchRegion(0x00000000, PROGRAM_SIZE, m_program.data());
  if (m_original)
  {
    m_i960.SetFetchRegion(0x00200000, 0x20000, m_ram.data());
    m_i960.SetFetchRegion(0x00220000, 0x20000, &m_program[0x20000]);
  }
  else
    m_i960.SetFetchRegion(0x00200000, (UINT32) m_ram.size(), m_ram.data());
  m_i960.SetFetchRegion(0x00500000, (UINT32) m_workRAM.size(), m_workRAM.data());

  m_tgp.AttachBus(this);
  m_tgp.SetProgramROM(m_tgpProgram.data(), (unsigned) m_tgpProgram.size());
  m_tgpx4.AttachBus(this);
  if (m_model2b)
  {
    m_fifoIn.size = m_fifoOut.size = 16;
    m_sharc.AttachBus(this);
    m_sharc.SetBootMode(CSHARC::BOOT_MODE_HOST);
    m_geoCompare = getenv("SUPERMODEL_MODEL2_GEO_COMPARE") != nullptr;
    m_geoLLE = !m_config["HighLevelGeometrizer"].ValueAsDefault<bool>(true) || getenv("SUPERMODEL_MODEL2_GEO_LLE") != nullptr || m_geoCompare;
    m_geoBus.board = this;
    m_geoSharc.AttachBus(&m_geoBus);
    m_geoSharc.SetBootMode(CSHARC::BOOT_MODE_HOST);
    m_geoPolyRAM.assign(0x8000, 0);
  }

  m_eeprom.Init();

  Model2::CVideo::Memory mem;
  mem.bufferRAM = m_bufferRAM.data();
  mem.paletteRAM = m_paletteRAM.data();
  mem.colorXlat = m_colorXlat.data();
  mem.lumaRAM = m_lumaRAM.data();
  mem.textureRAM0 = m_textureRAM[0].data();
  mem.textureRAM1 = m_textureRAM[1].data();
  mem.gamma = m_gamma;
  mem.polygonROM = m_polyROM.data();
  mem.polygonROMWords = (uint32_t) m_polyROM.size();
  mem.textureROM = m_textureROM.data();
  mem.textureROMWords = (uint32_t) m_textureROM.size();
  m_video3d.Init(mem);
  // Debugging aid: "frame:file" writes the words the rasterizer gets in that frame
  if (const char *trace = getenv("SUPERMODEL_MODEL2_RASTERTRACE"))
  {
    unsigned frame = 0;
    char file[1024] = { 0 };
    if (sscanf(trace, "%u:%1023s", &frame, file) == 2)
    {
      std::string path = file;
      m_video3d.m_pushTrace = [this, frame, path](uint32_t data)
      {
        static FILE *fp = nullptr;
        if (m_frameNumber == frame && !fp)
          fp = fopen(path.c_str(), "w");
        if (fp && m_frameNumber == frame)
          fprintf(fp, "%08X\n", data);
        else if (fp && m_frameNumber != frame)
        {
          fclose(fp);
          fp = nullptr;
        }
      };
    }
  }
  m_videoMemory = mem;
  m_softwareRender = m_config["Model2SoftwareRender"].ValueAsDefault<bool>(false) || getenv("SUPERMODEL_MODEL2_SOFTWARE") != nullptr;
  // quads or triangles, as the Model 3 renderer
  m_video3d.SetGPUQuads(m_config["QuadRendering"].ValueAsDefault<bool>(true));

  if (m_original && rom_set.get_rom("sound_program").size)
  {
    // The Model 1 sound board, linked through the UART both ways
    m_m1SoundBoard = std::make_unique<CModel1SoundBoard>(m_config);
    if (Result::OKAY != m_m1SoundBoard->Init(m_soundProgram.data(), m_soundProgram.size(), m_mpcm1.data(), m_mpcm1.size(), m_mpcm2.data(), m_mpcm2.size()))
      return Result::FAIL;
    m_uart.OnTransmit = [this](uint8_t data)
    {
      if (m_logIO)
        DebugLog("Main -> sound: %02X (frame %llu)\n", data, (unsigned long long) m_frameNumber);
      m_m1SoundBoard->WriteMIDI(data, (m_line + 0.5) / TOTAL_LINES);
    };
    m_m1SoundBoard->SetMIDIOutCallback([this](UINT8 data) { m_uart.ReceiveByte(data); });
  }
  // Sound board (68000 + SCSP), linked through the UART: what the i960
  // sends goes to the SCSP's MIDI input
  else if (!m_original && !m_soundProgram.empty() && rom_set.get_rom("sound_program").size)
  {
    m_soundBoard = std::make_unique<CSoundBoard>(m_config);
    if (Result::OKAY != m_soundBoard->Init(m_soundProgram.data(), m_samples.data()))
      return Result::FAIL;
    // A second sound board, the Model 1 one (Manx TT DX, MAME's manxttdx):
    // the UART's output goes to both, its replies come from the Model 1 board.
    // Its audio is mixed into the SCSP board's frame (one output): attached
    // like a DSB, it runs when the SCSP board does
    if (!m_m1SoundProgram.empty())
    {
      m_m1SoundBoard = std::make_unique<CModel1SoundBoard>(m_config);
      if (Result::OKAY != m_m1SoundBoard->Init(m_m1SoundProgram.data(), m_m1SoundProgram.size(), m_mpcm1.data(), m_mpcm1.size(), m_mpcm2.data(), m_mpcm2.size()))
        return Result::FAIL;
      m_m1SoundBoard->SetMIDIOutCallback([this](UINT8 data) { m_uart.ReceiveByte(data); });
      m_m1SoundMixer = std::make_unique<Model1SoundMixer>(m_m1SoundBoard.get());
      m_soundBoard->AttachDSB(m_m1SoundMixer.get());
    }
    else
    {
      // The SCSP's MIDI output is the UART's input (MAME's midi_out_cb ->
      // write_rxd): the sound program's replies, which some games wait for
      // (Model 2B: Cyber Troopers Virtual-On, Fighting Vipers)
      m_soundBoard->SetMIDIOutCallback([this](UINT8 data) { m_uart.ReceiveByte(data); });
    }
    // The Digital Sound Board (MPEG music), as on Model 3: the UART's output
    // goes to it too (MAME: txd_handler().append) and the SCSP board mixes it
    // in. The type comes from Games.xml's mpeg_board, as Model 3's: MAME's
    // dsbz80 (Z80, Sega Touring Car Championship) is Model 3's DSB1, its dsb2
    // (68000, Top Skater) the DSB2.
    if (!m_dsbProgram.empty() && !m_m1SoundMixer)
    {
      if (game.mpeg_board == Game::MPEG_BOARD_DSB1)
        m_dsb = std::make_unique<CDSB1>(m_config);
      else if (game.mpeg_board == Game::MPEG_BOARD_DSB2)
        m_dsb = std::make_unique<CDSB2>(m_config);
      else
        ErrorLog("No MPEG board type defined in game XML for MPEG ROMs.");
      if (m_dsb)
      {
        if (Result::OKAY != m_dsb->Init(m_dsbProgram.data(), m_dsbMusic.data()))
          return Result::FAIL;
        m_soundBoard->AttachDSB(m_dsb.get());
      }
    }
    m_uart.OnTransmit = [this](uint8_t data)
    {
      if (m_logIO)
        DebugLog("Main -> sound: %02X (frame %llu)\n", data, (unsigned long long) m_frameNumber);
      m_soundBoard->WriteMIDIPort(data);
      if (m_m1SoundBoard)
        m_m1SoundBoard->WriteMIDI(data, (m_line + 0.5) / TOTAL_LINES);
    };
  }
  else
    ErrorLog("Model 2: no sound program, sound board disabled.");
  m_uart.OnRxRdy = [this](bool) { SoundReady(); };
  m_uart.OnTxRdy = [this](bool) { SoundReady(); };

  m_logIO = m_config["Model2LogIO"].ValueAsDefault<bool>(false) || getenv("SUPERMODEL_MODEL2_LOGIO") != nullptr;

  // Debugging aid: "start,end" (hex) of the i960 program disassembled to the log
  if (const char *range = getenv("SUPERMODEL_MODEL2_DISASM"))
  {
    unsigned start = 0, end = 0;
    if (sscanf(range, "%x,%x", &start, &end) == 2)
    {
      for (UINT32 pc = start & ~3u; pc < end; )
      {
        unsigned len = 0;
        std::string text = m_i960.Disassemble(pc, &len);
        InfoLog("%08X: %08X %s\n", pc, Read32(pc), text.c_str());
        pc += len ? len : 4;
      }
    }
  }

  // Print game information
  std::set<std::string> extra_hw;
  if (m_dsb)
    extra_hw.insert(Util::Format() << "Digital Sound Board (Type " << (dynamic_cast<CDSB2 *>(m_dsb.get()) ? 2 : 1) << ")");
  if (m_soundBoard && m_m1SoundBoard)
    extra_hw.insert("Model 1 Sound Board");
  if (m_wheelBoard || m_driveBoard.present)
    extra_hw.insert("Drive Board");
  if (m_extra == Extra::Crypt5881 || m_extra == Extra::Comp0229)
    extra_hw.insert("Security Board");
  if (game.netboard_present)
    extra_hw.insert("Communication Board");
  if (!game.version.empty())
    std::cout << "    Title:          " << game.title << " (" << game.version << ")" << std::endl;
  else
    std::cout << "    Title:          " << game.title << std::endl;
  std::cout << "    ROM Set:        " << game.name << std::endl;
  std::cout << "    Developer:      " << game.manufacturer << std::endl;
  std::cout << "    Year:           " << game.year << std::endl;
  std::cout << "    Hardware:       " << Game::SteppingName(game.stepping) << std::endl;
  if (extra_hw.empty())
    extra_hw.insert("None");
  std::cout << "    Extra Hardware: " << Util::Format(", ").Join(extra_hw) << std::endl;
  std::cout << std::endl;

  InfoLog("Model 2: %s initialized.", m_game.title.c_str());
  return Result::OKAY;
}

CModel2::CModel2(Util::Config::Node &config, CInputs *inputs, COutputs *outputs, std::shared_ptr<CNetServer> server)
  : m_config(config),
    m_inputs(inputs),
    m_outputs(outputs),
    m_tiles(0x3FFF),
    m_server(std::move(server)),
    m_gamma{}
{
}

CModel2::~CModel2(void)
{
}
