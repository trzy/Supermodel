// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
// Ported to Supermodel from MAME (src/mame/sega/model1.cpp, model1.h, model1_m.cpp).

/*
 * Model1.h
 *
 * The Sega Model 1 system board: NEC V60 CPU, the TGP coprocessor (Fujitsu
 * MB86233) and its FIFOs, RAM, ROM banking, interrupts and timers, the video
 * board (geometrizer simulation, System 24 tilemaps, palette) and the I/O
 * board, sound board and their links.
 *
 * Emulated as MAME does, except:
 *
 *  - The I/O board's Z80 is not emulated: its BIOS puts the inputs in the
 *    dual port RAM (analog channels at 0-7, digital ports at 8-10), and that
 *    is done directly, as MAME did before it emulated the board (0.190).
 *  - The communication board (Virtua Racing, Wing War) is not emulated: its
 *    shared memory reads as RAM, the board as absent.
 *  - The scheduler: the V60 and the TGP run in turns of a few cycles, and a
 *    CPU that reads an empty FIFO or writes a full one stops until the other
 *    CPU unblocks it (MAME halts it with the same rules, see the FIFO class).
 */

#ifndef INCLUDED_MODEL1_H
#define INCLUDED_MODEL1_H

#include "Supermodel.h"
#include "Model3/IEmulator.h"
#include "CPU/Bus.h"
#include "CPU/V60/V60.h"
#include "CPU/MB86233/MB86233.h"
#include "Model1/Model1Geometry.h"
#include "Model1/Model1Geometrizer.h"
#include "Model1/Model1Tilemap.h"
#include "Model1/I8251.h"
#include "Graphics/Model1Render.h"
#include "Game.h"
#include "Util/NewConfig.h"
#include <cstdint>
#include <deque>
#include <memory>
#include <vector>

class CInputs;
class COutputs;
class CModel1SoundBoard;
class CDSB;

class CModel1: public IEmulator, public IBus, public IMB86233Bus
{
public:
  // IEmulator
  void SaveState(CBlockFile *SaveState) override;
  void LoadState(CBlockFile *SaveState) override;
  bool SupportsSaveStates(void) const override { return true; }
  void SaveNVRAM(CBlockFile *NVRAM) override;
  void LoadNVRAM(CBlockFile *NVRAM) override;
  void ClearNVRAM(void) override;
  void RunFrame(void) override;
  void RenderFrame(void) override;
  void Reset(void) override;
  const Game &GetGame(void) const override { return m_game; }
  void AttachRenderers(IRender2D *Render2DPtr, IRender3D *Render3DPtr, SuperAA *superAA) override;
  Result Init(const Game &game, const ROMSet &rom_set) override;
  bool PauseThreads(void) override { return true; }
  bool ResumeThreads(void) override { return true; }

  // V60 program space (IBus)
  UINT8 Read8(UINT32 addr) override;
  UINT16 Read16(UINT32 addr) override;
  void Write8(UINT32 addr, UINT8 data) override;
  void Write16(UINT32 addr, UINT16 data) override;

  // TGP (IMB86233Bus)
  UINT32 ReadData(UINT16 addr) override;
  void WriteData(UINT16 addr, UINT32 v) override;
  UINT32 ReadIO(UINT16 addr) override;
  void WriteIO(UINT16 addr, UINT32 v) override;

  CModel1(Util::Config::Node &config, CInputs *inputs, COutputs *outputs);
  ~CModel1(void);

private:
  // The V60 I/O space: the coprocessor ports, as in program space
  class CIOBus: public IBus
  {
  public:
    CModel1 *board = nullptr;
    UINT8 Read8(UINT32 addr) override;
    UINT16 Read16(UINT32 addr) override;
    void Write8(UINT32 addr, UINT8 data) override;
    void Write16(UINT32 addr, UINT16 data) override;
  };

  enum CPUID { CPU_V60 = 0, CPU_TGP = 1, CPU_NONE = 2 };

  // MAME's generic_fifo: a pop when empty stalls the reader (it retries) and
  // halts it until a push; a push when full goes to an overflow queue and
  // halts the writer until the overflow is drained.
  class CFIFO
  {
  public:
    std::deque<uint32_t> values;
    std::deque<uint32_t> extra;
    static constexpr size_t SIZE = 16;
  };

  // 16-bit access with a byte lane mask, as MAME's handlers see it
  uint16_t ReadWord(uint32_t addr, uint16_t mask);
  void WriteWord(uint32_t addr, uint16_t data, uint16_t mask);
  bool ReadCopro(uint32_t addr, uint16_t mask, uint16_t *data);   // ports in both spaces
  bool WriteCopro(uint32_t addr, uint16_t data, uint16_t mask);

  // Coprocessor glue (model1_m.cpp)
  uint32_t FIFOPop(CFIFO &fifo, CPUID reader, CPUID writer);
  void FIFOPush(CFIFO &fifo, uint32_t value, CPUID writer, CPUID reader);
  void Halt(CPUID cpu);
  void Unhalt(CPUID cpu, CPUID by);
  void CoproReset();
  uint32_t copro_sincos_r(unsigned offset);
  uint32_t copro_inv_r(unsigned offset);
  uint32_t copro_isqrt_r(unsigned offset);
  uint32_t copro_atan_r();
  uint32_t copro_data_r(unsigned offset);

  // Machine (model1.cpp)
  void IRQRaise(int level);
  int IRQAcknowledge();
  void IRQControlWrite(uint8_t data);
  void IRQMaskWrite(uint8_t data);
  void SoundReady();
  uint16_t TimerRead(unsigned offset);
  void TimerPeriodWrite(unsigned offset, uint16_t data, uint16_t mask);
  void BankWrite(uint16_t data, uint16_t mask);
  void SetBank(unsigned bank);

  // Scheduling
  void RunLine(void);
  void RunTimers(int v60Cycles);
  void UpdateInputs(void);
  void BuildVideoFrame(void);
  void CompareGeometry(const std::vector<Model1::CGeometry::PolyRecord> (&simulated)[2]);

  // ROM regions
  bool LoadROMs(const ROMSet &rom_set);

  Util::Config::Node &m_config;
  CInputs   *m_inputs;
  COutputs  *m_outputs;
  Game      m_game;

  // Devices
  CV60                  m_v60;
  CIOBus                m_ioBus;
  CMB86233              m_tgp;
  Model1::CGeometry     m_geometry;
  Model1::CGeometrizer  m_geometrizer;    // the video board DSPs run for real (Model1Geometrizer=1)
  CSys24Tilemap         m_tiles;
  CI8251                m_uart;
  std::unique_ptr<CModel1SoundBoard> m_soundBoard;
  std::unique_ptr<CDSB>   m_dsb;          // MPEG music board (Star Wars Arcade)
  std::vector<uint8_t>  m_dsbProgram;     // its Z80 program and MPEG data (the board keeps pointers)
  std::vector<uint8_t>  m_dsbMPEG;
  CModel1Render         *m_render = nullptr;
  SuperAA               *m_superAA = nullptr;

  // ROMs
  std::vector<uint8_t>  m_maincpu;        // 32 MB, MAME's "maincpu" region layout
  std::vector<uint32_t> m_tgpProgram;     // 0x800 words
  std::vector<uint32_t> m_geo5571, m_geo5572;     // geometrizer programs, 0x800 words
  std::vector<uint32_t> m_geoRecip, m_geoRsqrt;   // their table ROMs, 0x10000 words
  std::vector<uint32_t> m_coproTables;    // 0x10000 words
  std::vector<uint32_t> m_coproData;      // may be empty
  std::vector<uint32_t> m_polyROM;        // 0x400000 words
  std::vector<uint8_t>  m_sampleROM1;     // MultiPCM samples, 4 MB each
  std::vector<uint8_t>  m_sampleROM2;

  // RAM (16-bit words, little-endian as the V60 sees them)
  std::vector<uint16_t> m_ramA;           // 0x400000, 64 KB, battery backed
  std::vector<uint16_t> m_ramB;           // 0x500000, 256 KB
  std::vector<uint16_t> m_displayList0;   // 0x600000, 64 KB
  std::vector<uint16_t> m_displayList1;   // 0x610000, 64 KB
  std::vector<uint16_t> m_paletteRAM;     // 0x900000, 0x2000 words
  std::vector<uint16_t> m_colorXlat;      // 0x910000, 0x6000 words
  std::vector<uint8_t>  m_dpram;          // 0xC00000, 2 KB dual port RAM shared with the I/O board
  std::vector<uint16_t> m_commRAM;        // 0xB00000, communication board shared memory (not emulated)
  unsigned              m_bank = 0;

  // Coprocessor
  CFIFO     m_fifoIn, m_fifoOut;
  bool      m_halted[2] = { false, false };
  bool      m_emptyTriggered[2] = { false, false };   // fifoIn (TGP waits), fifoOut (V60 waits)
  bool      m_fullTriggered[2] = { false, false };
  std::vector<uint32_t> m_coproRAM;       // 0x2000 words
  uint16_t  m_v60CoproRAMAdr = 0;
  uint16_t  m_v60CoproRAMLatch[2] = { 0, 0 };
  uint32_t  m_v60FIFORead = 0, m_v60FIFOWrite = 0;
  uint32_t  m_coproSinCosBase = 0, m_coproInvBase = 0, m_coproISqrtBase = 0;
  uint32_t  m_coproAtanBase[4] = { 0, 0, 0, 0 };
  uint32_t  m_coproDataBase = 0;
  uint32_t  m_coproRAMAdr[4] = { 0, 0, 0, 0 };

  // Interrupts and timers
  uint8_t   m_irqStatus = 0;
  uint8_t   m_irqMask = 0xFF;
  int       m_lastIRQ = 0;
  uint16_t  m_timerPeriod[2] = { 0, 0 };
  uint16_t  m_timerVal[2] = { 0, 0 };
  uint16_t  m_timerMode = 0;
  int64_t   m_timerRemaining[2] = { 0, 0 };   // V60 cycles to the next tick (0 = stopped)

  // Time, in units of 1/80 MHz: a V60 cycle (16 MHz) is 5 units, a TGP cycle
  // (40 MHz / 3) is 6 units
  int64_t   m_time[2] = { 0, 0 };
  int64_t   m_lineEnd = 0;
  int       m_line = 0;
  CPUID     m_running = CPU_NONE;
  int       m_uartClockFrac = 0;
  uint64_t  m_frameNumber = 0;

  // Video
  CModel1Render::VideoFrame m_video;
  std::vector<uint32_t> m_palette;          // 0xFFRRGGBB, from palette RAM
  // I/O board: frames after a reset before it reports ready (its BIOS boot; an estimate)
  static constexpr int IO_BOARD_BOOT_FRAMES = 30;
  int       m_ioBoardBootFrames = 0;
  int       m_throttleLever = 0;    // keyboard throttle position (0-255), not saved

  bool      m_softwareRender = false;
  bool      m_useGeometrizer = false;
  bool      m_logIO = false;          // log the dual port RAM accesses (debugging the I/O board simulation)
  int       m_freezeAfterFrames = -1; // debugging: stop emulating after this many frames (-1 = never)
  int       m_framesRun = 0;
  bool      m_dumped = false;
};

#endif  // INCLUDED_MODEL1_H
