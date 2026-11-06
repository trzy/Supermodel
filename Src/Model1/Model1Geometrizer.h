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
 * Timing is not emulated: once a frame the display list is fed in and the
 * chips run until each is idle with an empty input. Like the real board,
 * the four run at the same time, each on its own thread, joined by
 * lock-free FIFOs; data only flows forward through them, so the result is
 * the same as running them one after another (which is done with fewer than
 * four CPU cores, or when multi-threading is disabled). The output of 5572 B
 * (the stream the video board's sorter and rasterizer consume) is kept for
 * the frame.
 *
 * A write to a full FIFO waits for the next chip. If that chip isn't running
 * or has failed, the word is lost and the frame fails, which keeps a runaway
 * DSP program from hanging the emulator.
 */

#ifndef INCLUDED_MODEL1GEOMETRIZER_H
#define INCLUDED_MODEL1GEOMETRIZER_H

#include "Types.h"
#include "CPU/MB86233/MB86233.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

class CBlockFile;

namespace Model1
{
  class CGeometrizer
  {
  public:
    // Program ROMs are 0x800 words; the tables 0x10000 words each. Poly RAM
    // is shared with the caller (it persists across frames and is saved with
    // the C++ geometry state). If multiThreaded, the chips run on their own
    // threads when there is a CPU core for each.
    bool Init(const std::vector<uint32_t> &rom5571, const std::vector<uint32_t> &rom5572,
              const std::vector<uint32_t> &recipTable, const std::vector<uint32_t> &rsqrtTable,
              const uint32_t *polyROM, uint32_t polyROMWords, uint32_t *polyRAM, uint32_t polyRAMWords,
              bool multiThreaded);
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

    ~CGeometrizer();

  private:
    enum Chip { C5571A, C5571B, C5572A, C5572B, NUM_CHIPS };

    // Single producer, single consumer FIFO between two chips. Each side
    // keeps its own index on its own cache line and a cached copy of the
    // other side's, so the cores only exchange cache lines when the FIFO
    // looks empty or full.
    class CFIFO
    {
    public:
      CFIFO();
      bool Push(uint32_t v);          // producer; false if full
      bool Pop(uint32_t &v);          // consumer; false if empty
      bool Empty();                   // consumer
      void Clear() { m_head.store(0); m_tail.store(0); m_headCache = m_tailCache = 0; m_done.store(false); m_consumerGone.store(false); }   // only while no chip runs
      void SetDone() { m_done.store(true, std::memory_order_release); }
      bool Done() const { return m_done.load(std::memory_order_acquire); }
      void SetConsumerGone() { m_consumerGone.store(true, std::memory_order_release); }
      bool ConsumerGone() const { return m_consumerGone.load(std::memory_order_acquire); }
    private:
      std::vector<uint32_t> m_buffer;
      size_t m_mask;
      alignas(64) std::atomic<size_t> m_head { 0 };   // next write
      size_t m_tailCache = 0;                         // producer's copy of m_tail
      alignas(64) std::atomic<size_t> m_tail { 0 };   // next read
      size_t m_headCache = 0;                         // consumer's copy of m_head
      alignas(64) std::atomic<bool> m_done { false }; // the producer has finished the frame
      std::atomic<bool> m_consumerGone { false };     // nobody will make room if the FIFO is full
    };

    // Each chip's bus and CPU on their own cache lines: the chips run on
    // different cores and write these all the time
    struct alignas(64) Bus : public IMB86233Bus
    {
      CGeometrizer *owner = nullptr;
      int chip = 0;
      CMB86233 *cpu = nullptr;
      CFIFO *in = nullptr;
      CFIFO *out = nullptr;                    // null for 5572 B
      std::vector<uint32_t> *final = nullptr;  // 5572 B's output
      uint32_t ioAddress = 0;                  // 5571 poly RAM/ROM address latch
      bool stalled = false;                    // a read found the input empty
      bool overflowed = false;                 // a write to the output was lost this frame

      UINT32 ReadData(UINT16 addr) override;
      void   WriteData(UINT16 addr, UINT32 v) override;
      UINT32 ReadIO(UINT16 addr) override;
      void   WriteIO(UINT16 addr, UINT32 v) override;
    };

    bool RunChip(int chip, bool stopAtEndOfFrame = false);
    static void WaitForInput(CFIFO &in);
    bool RunChipFrame(int chip);
    bool IsIdle(int chip) const;
    uint32_t PolyRead(uint32_t adr) const;
    void PolyWrite(uint32_t adr, uint32_t data);
    void StartThreads();
    void StopThreads();
    void Worker(int chip);

    bool m_ready = false;
    struct alignas(64) CPU : public CMB86233 { };
    CPU m_cpu[NUM_CHIPS];
    Bus m_bus[NUM_CHIPS];
    CFIFO m_fifo[NUM_CHIPS];           // m_fifo[i] feeds chip i
    std::vector<uint32_t> m_rom5571, m_rom5572;
    const uint32_t *m_recip = nullptr, *m_rsqrt = nullptr;
    const uint32_t *m_polyROM = nullptr;
    uint32_t m_polyROMWords = 0;
    uint32_t *m_polyRAM = nullptr;
    uint32_t m_polyRAMWords = 0;
    std::vector<uint32_t> m_output;

    // Worker threads, one per chip
    std::vector<std::thread> m_threads;
    std::mutex m_mutex;
    std::condition_variable m_start, m_finished;
    uint64_t m_frame = 0;              // incremented to start a frame
    int m_running = 0;                 // chips still running the frame
    bool m_quit = false;
    bool m_chipOK[NUM_CHIPS] = { };
  };
}

#endif  // INCLUDED_MODEL1GEOMETRIZER_H
