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
 * Model1Geometrizer.h
 *
 * The four geometrizer DSPs of the Model 1 video board, run as real MB86233
 * programs: two 315-5571s (display list walk and transformation) and two
 * 315-5572s (projection and lighting), each pair running one program in two
 * roles chosen by its gpio2 pin:
 *
 *   display list -> 5571 A -> 5571 B -> 5572 A -> 5572 B -> video board
 *
 * The chips are joined by FIFOs (data read 0x100 pops the input, writes at
 * 0x400 and up push the output; gpio3 is high while the input is empty).
 * 5571 A reads polygon data through its I/O port (io0 = address latch,
 * io1 = read/write then +1, io4 = read then +4); the 5572s read their
 * reciprocal (A) and reciprocal square root (B) table ROMs on theirs.
 *
 * Timing is not emulated: once a frame the display list is fed in and each
 * chip runs until it is idle with an empty input, in pipeline order. The
 * output of 5572 B (the stream the video board's sorter and rasterizer
 * consume) is kept for the frame.
 */

#ifndef INCLUDED_MODEL1GEOMETRIZER_H
#define INCLUDED_MODEL1GEOMETRIZER_H

#include "Types.h"
#include "CPU/MB86233/MB86233.h"
#include <cstdint>
#include <deque>
#include <vector>

class CBlockFile;

namespace Model1
{
  class CGeometrizer
  {
  public:
    // Program ROMs are 0x800 words; the tables 0x10000 words each. Poly RAM
    // is shared with the caller (it persists across frames and is saved with
    // the C++ geometry state).
    bool Init(const std::vector<uint32_t> &rom5571, const std::vector<uint32_t> &rom5572,
              const std::vector<uint32_t> &recipTable, const std::vector<uint32_t> &rsqrtTable,
              const uint32_t *polyROM, uint32_t polyROMWords, uint32_t *polyRAM, uint32_t polyRAMWords);
    bool IsReady() const { return m_ready; }

    void Reset();

    // Walks the display list (16-bit words, MAME's layout) through the
    // pipeline. Returns false if a chip didn't finish (runaway program).
    bool RunFrame(const uint16_t *displayList);

    // 5572 B's output for the last frame, idle filler words removed
    const std::vector<uint32_t> &Output() const { return m_output; }

    void SaveState(CBlockFile *SaveState);
    // Returns false for a save state made without the geometrizer (the DSPs
    // are reset and need their light parameters and mode sent again)
    bool LoadState(CBlockFile *SaveState);

  private:
    enum Chip { C5571A, C5571B, C5572A, C5572B, NUM_CHIPS };

    struct Bus : public IMB86233Bus
    {
      CGeometrizer *owner = nullptr;
      int chip = 0;
      CMB86233 *cpu = nullptr;
      std::deque<uint32_t> *in = nullptr;
      std::deque<uint32_t> *out = nullptr;
      uint32_t ioAddress = 0;   // 5571 poly RAM/ROM address latch
      uint64_t popped = 0;      // input words consumed

      UINT32 ReadData(UINT16 addr) override;
      void   WriteData(UINT16 addr, UINT32 v) override;
      UINT32 ReadIO(UINT16 addr) override;
      void   WriteIO(UINT16 addr, UINT32 v) override;
    };

    bool RunChip(int chip, bool stopAtEndOfFrame = false);
    bool IsIdle(int chip) const;
    uint32_t PolyRead(uint32_t adr) const;
    void PolyWrite(uint32_t adr, uint32_t data);

    bool m_ready = false;
    CMB86233 m_cpu[NUM_CHIPS];
    Bus m_bus[NUM_CHIPS];
    std::deque<uint32_t> m_fifo[NUM_CHIPS + 1];   // m_fifo[i] feeds chip i; the last is the output
    std::vector<uint32_t> m_rom5571, m_rom5572;
    const uint32_t *m_recip = nullptr, *m_rsqrt = nullptr;
    const uint32_t *m_polyROM = nullptr;
    uint32_t m_polyROMWords = 0;
    uint32_t *m_polyRAM = nullptr;
    uint32_t m_polyRAMWords = 0;
    std::vector<uint32_t> m_output;
  };
}

#endif  // INCLUDED_MODEL1GEOMETRIZER_H
