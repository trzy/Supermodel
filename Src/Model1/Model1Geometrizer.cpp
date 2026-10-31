/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
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
 * Model1Geometrizer.cpp
 *
 * The geometrizer DSPs run as real programs. See Model1Geometrizer.h.
 */

#include "Model1Geometrizer.h"
#include "BlockFile.h"
#include "OSD/Logger.h"

namespace Model1
{
  // Idle loops (polling gpio3 for input), from the disassembly
  static bool InRange(uint16_t pc, uint16_t lo, uint16_t hi) { return pc >= lo && pc <= hi; }

  // 5571 A, cmd F: waits here for gpio0 (end of frame), then for an input
  // word, before passing the F on and draining its input
  static constexpr uint16_t PC_5571A_WAIT_GPIO0 = 0x11C;

  // Run budget per chip and frame (instruction slots). A busy frame needs a
  // few hundred thousand.
  static constexpr int MAX_CYCLES = 20000000;

  // 5572 B outputs this filler word on every pass of its idle loop (it is
  // also command 0, a no-op)
  static constexpr uint32_t IDLE_WORD = 0x7F800000;


  /******************************************************************************
   Buses
  ******************************************************************************/

  UINT32 CGeometrizer::Bus::ReadData(UINT16 addr)
  {
    if (addr == 0x100)
    {
      if (in->empty())
      {
        cpu->Stall();
        return 0;
      }
      uint32_t v = in->front();
      in->pop_front();
      popped++;
      cpu->gpio3_w(in->empty() ? 1 : 0);
      return v;
    }
    return 0;
  }

  void CGeometrizer::Bus::WriteData(UINT16 addr, UINT32 v)
  {
    // Any address at 0x400 and up pushes the output FIFO (5571 A writes
    // 0x400 + (command & 7), 5572 A 0x400-0x407 and 0x700)
    if (addr >= 0x400)
    {
      out->push_back(v);
      if (chip + 1 < NUM_CHIPS)
        owner->m_cpu[chip + 1].gpio3_w(0);
    }
  }

  UINT32 CGeometrizer::Bus::ReadIO(UINT16 addr)
  {
    switch (chip)
    {
    case C5571A:
    case C5571B:
    {
      // Poly RAM/ROM port: io1 reads then advances by 1, io4 by 4 (skips a
      // stored normal)
      uint32_t v = owner->PolyRead(ioAddress);
      if (addr == 1 || addr == 4)
        ioAddress += addr;
      return v;
    }
    case C5572A:
      return owner->m_recip[addr];
    default:
      return owner->m_rsqrt[addr];
    }
  }

  void CGeometrizer::Bus::WriteIO(UINT16 addr, UINT32 v)
  {
    if (chip > C5571B)
      return;
    if (addr == 0)
      ioAddress = v;
    else if (addr == 1)
      owner->PolyWrite(ioAddress++, v);
  }

  uint32_t CGeometrizer::PolyRead(uint32_t adr) const
  {
    if (adr & 0x800000)
      return m_polyRAM[(adr - 0x800000) & (m_polyRAMWords - 1)];
    return m_polyROMWords ? m_polyROM[adr % m_polyROMWords] : 0;
  }

  void CGeometrizer::PolyWrite(uint32_t adr, uint32_t data)
  {
    m_polyRAM[(adr - 0x800000) & (m_polyRAMWords - 1)] = data;
  }


  /******************************************************************************
   Running
  ******************************************************************************/

  bool CGeometrizer::IsIdle(int chip) const
  {
    uint16_t pc = (uint16_t) m_cpu[chip].GetPC();
    switch (chip)
    {
    case C5571A: return InRange(pc, 0x108, 0x10D);
    case C5571B: return InRange(pc, 0x10E, 0x112);
    case C5572A: return InRange(pc, 0x147, 0x14B);
    // 5572 B's idle loop also passes through 662-666, which writes the filler
    // word, but so does its end command: only the loop head counts as idle,
    // or the end marker is left to come out at the start of the next frame
    default:     return InRange(pc, 0x14C, 0x150);
    }
  }

  // Runs a chip until it is idle with an empty input, or (5571 A, if asked)
  // waiting for the end of frame signal. Returns false if it doesn't get there.
  bool CGeometrizer::RunChip(int chip, bool stopAtEndOfFrame)
  {
    CMB86233 &cpu = m_cpu[chip];
    std::deque<uint32_t> &in = m_fifo[chip];
    cpu.gpio3_w(in.empty() ? 1 : 0);
    for (int cycles = 0; cycles < MAX_CYCLES; )
    {
      if (in.empty() && IsIdle(chip))
        return true;
      if (stopAtEndOfFrame && chip == C5571A && cpu.GetPC() == PC_5571A_WAIT_GPIO0)
        return true;
      cycles += cpu.Run(256);
    }
    return false;
  }

  // Display list length in 16-bit words up to and including the end command,
  // using the command lengths of MAME's display list walker
  static size_t ListLength(const uint16_t *list)
  {
    auto readi = [list](size_t adr) -> uint32_t { return list[adr & 0x7fff] | (uint32_t(list[(adr + 1) & 0x7fff]) << 16); };
    size_t offset = 0;
    while (offset < 0x8000)
    {
      uint32_t type = readi(offset);
      switch (type)
      {
      case 0:    offset += 2; break;
      case 1:
      case 0x41: offset += 8; break;
      case 2:
        offset += 18;
        for (;;)
        {
          uint32_t flags = readi(offset + 2);
          if (!(flags & 3))
            break;
          offset += (flags & 3) == 2 ? 12 : 20;
        }
        offset += 4;
        break;
      case 3:    offset += 16; break;
      case 4:    offset += 6 + (readi(offset + 4) + 1) * 2; break;
      case 5:
      case 6:    offset += 6 + readi(offset + 4) * 2; break;
      case 7:
      case 8:    offset += 4; break;
      case 9:    offset += 6; break;
      case 0xa:  offset += 8; break;
      case 0xb:  offset += 26; break;
      case 0xc:  offset += 6; break;
      default:   // 0xf or 0xffffffff end the list; anything else is unknown
        return offset + 2;
      }
    }
    return 0x8000;
  }

  bool CGeometrizer::RunFrame(const uint16_t *displayList)
  {
    m_output.clear();
    if (!m_ready)
      return false;

    size_t length = ListLength(displayList) & ~size_t(1);
    for (size_t i = 0; i < length; i += 2)
      m_fifo[0].push_back(displayList[i] | (uint32_t(displayList[i + 1]) << 16));

    bool ok = RunChip(C5571A, true);
    if (ok && m_cpu[C5571A].GetPC() == PC_5571A_WAIT_GPIO0)
    {
      // End of frame: raise gpio0 and give it a word to drain, so it passes
      // the end command on. gpio0 is a level: drop it once it has been seen,
      // or the idle loop takes it as another end of frame.
      CMB86233 &cpu = m_cpu[C5571A];
      cpu.gpio0_w(1);
      m_fifo[0].push_back(0);
      cpu.gpio3_w(0);
      for (int i = 0; i < 16 && cpu.GetPC() == PC_5571A_WAIT_GPIO0; i++)
        cpu.Run(1);
      cpu.gpio0_w(0);
      ok = RunChip(C5571A);
    }
    for (int chip = C5571B; ok && chip < NUM_CHIPS; chip++)
      ok = RunChip(chip);
    if (!ok)
      DebugLog("Model 1 geometrizer: a DSP didn't finish the frame (PCs %03X %03X %03X %03X)\n",
               m_cpu[0].GetPC(), m_cpu[1].GetPC(), m_cpu[2].GetPC(), m_cpu[3].GetPC());

    std::deque<uint32_t> &out = m_fifo[NUM_CHIPS];
    m_output.reserve(out.size());
    for (uint32_t v : out)
      if (v != IDLE_WORD)
        m_output.push_back(v);
    for (auto &fifo : m_fifo)
      fifo.clear();
    return ok;
  }


  /******************************************************************************
   Reset, save states, initialization
  ******************************************************************************/

  void CGeometrizer::Reset()
  {
    if (!m_ready)
      return;
    for (auto &fifo : m_fifo)
      fifo.clear();
    for (int chip = 0; chip < NUM_CHIPS; chip++)
    {
      CMB86233 &cpu = m_cpu[chip];
      cpu.Reset();
      cpu.gpio0_w(0);
      cpu.gpio1_w(0);           // held in reset: the program clears its RAM and waits
      cpu.gpio2_w(chip & 1);    // role: A = 0, B = 1
      cpu.gpio3_w(1);
      cpu.Run(5000);
      cpu.gpio1_w(1);
      RunChip(chip);
    }
  }

  void CGeometrizer::SaveState(CBlockFile *SaveState)
  {
    SaveState->NewBlock("Model 1 Geometrizer", __FILE__);
    for (int chip = 0; chip < NUM_CHIPS; chip++)
      SaveState->Write(&m_bus[chip].ioAddress, sizeof(m_bus[chip].ioAddress));
    static const char *names[NUM_CHIPS] = { "Model 1 5571 A", "Model 1 5571 B", "Model 1 5572 A", "Model 1 5572 B" };
    for (int chip = 0; chip < NUM_CHIPS; chip++)
      m_cpu[chip].SaveState(SaveState, names[chip]);
  }

  bool CGeometrizer::LoadState(CBlockFile *SaveState)
  {
    if (Result::OKAY != SaveState->FindBlock("Model 1 Geometrizer"))
    {
      Reset();
      return false;
    }
    for (int chip = 0; chip < NUM_CHIPS; chip++)
      SaveState->Read(&m_bus[chip].ioAddress, sizeof(m_bus[chip].ioAddress));
    static const char *names[NUM_CHIPS] = { "Model 1 5571 A", "Model 1 5571 B", "Model 1 5572 A", "Model 1 5572 B" };
    for (int chip = 0; chip < NUM_CHIPS; chip++)
      m_cpu[chip].LoadState(SaveState, names[chip]);
    return true;
  }

  bool CGeometrizer::Init(const std::vector<uint32_t> &rom5571, const std::vector<uint32_t> &rom5572,
                          const std::vector<uint32_t> &recipTable, const std::vector<uint32_t> &rsqrtTable,
                          const uint32_t *polyROM, uint32_t polyROMWords, uint32_t *polyRAM, uint32_t polyRAMWords)
  {
    m_ready = false;
    if (rom5571.size() < 0x800 || rom5572.size() < 0x800 || recipTable.size() < 0x10000 || rsqrtTable.size() < 0x10000 || !polyRAM)
      return false;
    m_rom5571 = rom5571;
    m_rom5572 = rom5572;
    m_recip = recipTable.data();
    m_rsqrt = rsqrtTable.data();
    m_polyROM = polyROM;
    m_polyROMWords = polyROMWords;
    m_polyRAM = polyRAM;
    m_polyRAMWords = polyRAMWords;

    for (int chip = 0; chip < NUM_CHIPS; chip++)
    {
      Bus &bus = m_bus[chip];
      bus.owner = this;
      bus.chip = chip;
      bus.cpu = &m_cpu[chip];
      bus.in = &m_fifo[chip];
      bus.out = &m_fifo[chip + 1];
      m_cpu[chip].AttachBus(&bus);
      m_cpu[chip].SetProgramROM(chip < C5572A ? m_rom5571.data() : m_rom5572.data(), 0x800);
    }
    m_ready = true;
    Reset();
    return true;
  }
}
