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
 * Model2.h
 *
 * Sega Model 2A-CRX, ported from MAME's model2 driver (R. Belmont, Olivier
 * Galibert, ElSemi, Angelo Salese, Matthew Daniels; src/mame/sega/model2.cpp).
 *
 * Based on original work by R. Belmont, Olivier Galibert, ElSemi, Angelo
 * Salese and Matthew Daniels in MAME. Portions Copyright (c) R. Belmont,
 * Olivier Galibert, ElSemi, Angelo Salese and Matthew Daniels, used under the
 * BSD 3-Clause License: see Docs/README.txt.
 *
 *   i960KB (25 MHz)            main CPU
 *   MB86234 TGP (50 MHz)       coprocessor, program uploaded by the i960,
 *                              joined to it by two FIFOs
 *   geometrizer                reads the display list from buffer RAM
 *                              (internal ROM, not dumped: MAME's
 *                              simulation, Model2Video)
 *   System 24 tilemaps         the 2D layers, as Model 1
 *   68000 + SCSP               sound, linked by a UART (the Model 3 sound
 *                              board, with one SCSP)
 *   315-5649                   I/O ports, 93C46 EEPROM
 *
 * The original Model 2 (stepping "2", Daytona USA) differs: the Model 1 sound
 * board (68000 + 2 MultiPCM + YM3438) and the Model 1 I/O board on dual port
 * RAM instead of the SCSP board and the 315-5649, 128 KB of RAM.
 *
 * 2B-CRX (stepping "2B", Fighting Vipers) has an ADSP-21062 SHARC (32 MHz)
 * for the coprocessor, booted by the i960 with host DMA, which polls the
 * FIFOs through its flag inputs; texture and luma RAM move to 11000000.
 * Its geometrizer is a second SHARC with an uploaded program, simulated here
 * as on 2A (the upload is kept).
 *
 * 2C-CRX (stepping "2C", The House of the Dead) has the TGPx4 (MB86235,
 * 20 MHz) for the coprocessor: its 64-bit program uploaded by the i960 (32 bits
 * at a time), FIFOs as 2A's, buffer RAM at 400000 and the copro data ROM at
 * 800000 in its data space; texture RAM 2 MB per sheet at 11000000.
 */

#ifndef INCLUDED_MODEL2_H
#define INCLUDED_MODEL2_H

#include "Supermodel.h"
#include "Model3/IEmulator.h"
#include "Model3/93C46.h"
#include "CPU/I960/I960.h"
#include "CPU/MB86233/MB86233.h"
#include "CPU/SHARC/SHARC.h"
#include "CPU/MB86235/MB86235.h"
#include "Model1/Model1Tilemap.h"
#include "Model2/Model2Video.h"
#include "Model2/Model2Comm.h"
#include "Model2/Sega315_5838.h"
#include "Model2/Model2DriveBoard.h"
#include "Model3/Crypto.h"
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
class CSoundBoard;
class CModel1SoundBoard;
class CDSB;

class CModel2: public IEmulator, public II960Bus, public IMB86233Bus, public ISHARCBus, public IMB86235Bus
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

  // i960 bus
  UINT8  Read8(UINT32 addr) override;
  UINT16 Read16(UINT32 addr) override;
  UINT32 Read32(UINT32 addr) override;
  void   Write8(UINT32 addr, UINT8 data) override;
  void   Write16(UINT32 addr, UINT16 data) override;
  void   Write32(UINT32 addr, UINT32 data) override;
  bool   Burst(UINT32 addr) override;

  // TGP bus
  UINT32 ReadIO(UINT16 addr) override;
  void   WriteIO(UINT16 addr, UINT32 v) override;
  UINT32 ReadRF(UINT8 addr) override;
  void   WriteRF(UINT8 addr, UINT32 v) override;

  // 2B coprocessor (SHARC) and 2C coprocessor (TGPx4) data bus
  UINT32 ReadData(UINT32 addr) override;
  void   WriteData(UINT32 addr, UINT32 data) override;

  // The TGP's external data space is unmapped on this board: it keeps the
  // MB86233 bus defaults (these take a UINT16 address, unlike those above)
  using IMB86233Bus::ReadData;
  using IMB86233Bus::WriteData;

  // 2C TGPx4 FIFO ports
  UINT32 FIFOInPop() override;
  bool   FIFOInEmpty() override { return m_fifoIn.values.empty(); }
  bool   FIFOInFull() override { return m_fifoIn.values.size() >= m_fifoIn.size; }
  void   FIFOInClear() override;
  void   FIFOOutPush(UINT32 data) override;
  bool   FIFOOutEmpty() override { return m_fifoOut.values.empty(); }
  bool   FIFOOutFull() override { return m_fifoOut.values.size() >= m_fifoOut.size; }
  void   FIFOOutClear() override;

  CModel2(Util::Config::Node &config, CInputs *inputs, COutputs *outputs, std::shared_ptr<CNetServer> server = nullptr);
  ~CModel2(void);

private:
  // The coprocessor is the TGP, or the SHARC on 2B
  enum CPUID { CPU_I960 = 0, CPU_COPRO = 1, CPU_NONE = 2 };

  // MAME's generic_fifo_u32_device as Model 2 uses it: 8 entries (16 on 2B); a
  // write to a full FIFO is kept and halts the writer until there is room, a
  // read of an empty one stalls and halts the reader until a write. The 2B
  // SHARC is never halted: it polls the FIFOs' flags.
  class CFIFO
  {
  public:
    std::deque<uint32_t> values;
    std::deque<uint32_t> extra;
    size_t size = 8;
  };

  // MAME's 315-5649 I/O chip (sega_315_5649_device)
  struct IOChip
  {
    uint8_t portValue[7] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    uint8_t portConfig = 0xFF;
    uint8_t analogChannel = 0;
  };

  uint32_t ReadDword(uint32_t addr, uint32_t mask);
  void WriteDword(uint32_t addr, uint32_t data, uint32_t mask);
  int  AreaWaits(uint32_t addr) const;
  void ChargeWaits(uint32_t addr) { m_i960.AddWaitCycles(AreaWaits(addr) * CYCLES_PER_WAIT); }
  // i960 cycles per programmed wait state, as sm2-emu (fitted to hardware
  // captures). Sega Rally's rival cars start in the wrong places below 3.
  static constexpr int CYCLES_PER_WAIT = 3;

  // Coprocessor
  uint32_t FIFOPop(CFIFO &fifo, CPUID reader, CPUID writer);
  void FIFOPush(CFIFO &fifo, uint32_t value, CPUID writer, CPUID reader);
  void Halt(CPUID cpu);
  void Unhalt(CPUID cpu, int64_t time);
  void CoproCtlWrite(uint32_t data, uint32_t mask);
  void CoproFIFOWrite(uint32_t data);
  void UpdateSHARCFlags();

  // 2B geometrizer run low level (its uploaded program on a second SHARC)
  class CGeoBus : public ISHARCBus
  {
  public:
    CModel2 *board = nullptr;
    UINT32 ReadData(UINT32 addr) override { return board->GeoSHARCRead(addr); }
    void   WriteData(UINT32 addr, UINT32 data) override { board->GeoSHARCWrite(addr, data); }
  };
  UINT32 GeoSHARCRead(UINT32 addr);
  void   GeoSHARCWrite(UINT32 addr, UINT32 data);
  uint32_t GeoFeed(bool command);
  void   RunGeometrizer();
  void   CompareGeometrizers(const std::vector<uint32_t> &hle, const std::vector<uint32_t> &lle);
  uint32_t copro_sincos_r(unsigned offset);
  uint32_t copro_inv_r(unsigned offset);
  uint32_t copro_isqrt_r(unsigned offset);
  uint32_t copro_atan_r();
  uint32_t copro_tgp_memory_r(unsigned offset);
  void copro_tgp_memory_w(unsigned offset, uint32_t data);

  // Geometrizer ports
  void GeoWrite(uint32_t offset, uint32_t data);
  uint32_t GeoRead(uint32_t offset);
  void GeoPush(uint32_t data);

  // Interrupts and timers
  void IRQUpdate();
  void IRQEnableWrite(uint32_t data, uint32_t mask);
  void SoundReady();
  int64_t I960Now() const;
  int64_t Now(CPUID cpu) const;
  uint32_t TimerRead(unsigned n);
  void TimerWrite(unsigned n, uint32_t data, uint32_t mask);
  void RunTimers();

  // I/O
  uint8_t IORead(unsigned offset);
  uint8_t RailChase2DriveRead() const;
  void IOWrite(unsigned offset, uint8_t data);
  void EEPROMWrite(uint8_t data);
  uint8_t In0Read();

  void PaletteWrite(unsigned offset, uint16_t data, uint16_t mask);
  void UpdatePaletteEntry(unsigned i);
  void RunLine(void);
  void UpdateInputs(void);
  void UpdateIOBoard(const uint8_t (&an)[8], uint8_t in0, uint8_t in1, uint8_t in2);
  void BuildVideoFrame(void);
  bool LoadROMs(const ROMSet &rom_set);

  Util::Config::Node &m_config;
  CInputs   *m_inputs;
  COutputs  *m_outputs;
  Game      m_game;

  CI960                 m_i960;
  CMB86233              m_tgp;
  CSHARC                m_sharc;          // 2B coprocessor
  CMB86235              m_tgpx4;          // 2C coprocessor
  CSHARC                m_geoSharc;       // 2B geometrizer (low level, if enabled)
  CGeoBus               m_geoBus;
  CSys24Tilemap         m_tiles;
  Model2::CVideo        m_video3d;        // geometrizer, z-sort, rasterizer
  CI8251                m_uart;
  CModel2Comm           m_comm;           // communication board (01A00000)
  std::shared_ptr<CNetServer> m_server;
  C93C46                m_eeprom;
  std::unique_ptr<CSoundBoard> m_soundBoard;           // 2A: 68000 + SCSP
  std::unique_ptr<CModel1SoundBoard> m_m1SoundBoard;   // original: the Model 1 sound board
  std::unique_ptr<CDSB> m_dsb;                          // Digital Sound Board (MPEG music: Touring Car Championship, Top Skater)
  std::unique_ptr<CDSB> m_m1SoundMixer;                 // 2A with both boards: the Model 1 board mixed into the SCSP board's output
  bool                  m_original = false;           // original Model 2 board (Daytona)
  bool                  m_model2b = false;            // 2B-CRX (SHARC coprocessor)
  bool                  m_model2c = false;            // 2C-CRX (TGPx4 coprocessor)
  CModel1Render         *m_render = nullptr;
  SuperAA               *m_superAA = nullptr;

  // ROMs
  std::vector<uint8_t>  m_program;        // i960 program, 2 MB at 0
  std::vector<uint8_t>  m_mainData;       // 0x2000000 (data at 2000000, the second 16 MB also at 6000000)
  std::vector<uint32_t> m_coproTables;    // 0x10000 words
  std::vector<uint32_t> m_coproData;      // may be empty
  std::vector<uint32_t> m_polyROM;        // the geometrizer's model data
  std::vector<uint16_t> m_textureROM;
  std::vector<uint8_t>  m_soundProgram;   // 512 KB
  std::vector<uint8_t>  m_samples;        // 16 MB, the upper 8 MB mirror the lower
  std::vector<uint8_t>  m_mpcm1, m_mpcm2;  // Model 1 sound board: MultiPCM samples, 4 MB each
  std::vector<uint8_t>  m_dsbProgram, m_dsbMusic;   // Digital Sound Board: its CPU's program, the MPEG data
  std::vector<uint8_t>  m_m1SoundProgram;  // 2A with a second, Model 1 sound board (Manx TT DX): its 68000 program

  // RAM
  std::vector<uint8_t>  m_ram;            // 0x200000, 256 KB
  std::vector<uint8_t>  m_workRAM;        // 0x500000, 1 MB
  std::vector<uint32_t> m_bufferRAM;      // 0x900000, 128 KB: the geometrizer's display list
  std::vector<uint8_t>  m_backupRAM;      // 0x1D00000, 16 KB, battery backed
  std::vector<uint16_t> m_paletteRAM;     // 0x1800000, 0x2000 words
  std::vector<uint16_t> m_colorXlat;      // 0x1810000, 0x6000 words
  std::vector<uint32_t> m_textureRAM[2];  // 0x12000000 and 0x12400000 (2B: 11000000, 11200000), 0x80000 words each
  std::vector<uint8_t>  m_lumaRAM;        // 0x12800000 (2B: 11400000), 32 KB
  std::vector<uint16_t> m_fbVRAM[2];      // 0x11600000 and 0x11680000
  std::vector<uint8_t>  m_waitStateRAM;   // 0xE00000, 0x38 bytes
  std::vector<uint8_t>  m_dpram;          // original: 0x1C00000, 2 KB shared with the I/O board
  std::vector<uint32_t> m_tgpProgram;     // 0x1000 words, uploaded by the i960
  std::vector<uint32_t> m_geoProgram;     // 2B: the geometrizer's program, as uploaded
  std::vector<uint32_t> m_geoPolyRAM;     // 2B low level geometrizer: slow polygon RAM, 0x8000 words

  // Coprocessor
  CFIFO     m_fifoIn, m_fifoOut;
  bool      m_halted[2] = { false, false };
  bool      m_emptyTriggered[2] = { false, false };   // fifoIn (TGP waits), fifoOut (i960 waits)
  bool      m_fullTriggered[2] = { false, false };
  bool      m_coproHeld = true;           // in reset until the i960 boots it
  uint32_t  m_coproCtl = 0, m_coproCount = 0;
  uint32_t  m_coproBank = 0;
  uint32_t  m_coproSinCosBase = 0, m_coproInvBase = 0, m_coproISqrtBase = 0;
  uint32_t  m_coproAtanBase[4] = { 0, 0, 0, 0 };

  // Geometrizer ports
  uint32_t  m_geoCtl = 0, m_geoCount = 0;
  uint32_t  m_geoWriteAddress = 0, m_geoReadAddress = 0;
  bool      m_geoLLE = false;             // 2B: run the geometrizer's program instead of the simulation
  bool      m_geoCompare = false;         // 2B: run both and compare (debugging aid)
  uint64_t  m_geoCompareStats[5] = {};    // words: exact, 1-4 apart, up to 0x400, more, unmatched
  bool      m_geoCompareLogged = false;
  uint64_t  m_geoClass[7] = {};           // compare: kinds of differences
  uint32_t  m_geoFeedAddress = 0;         // buffer RAM word the display list is read from next
  uint32_t  m_geoFeedCount = 0;           // words fed this frame (runaway guard)
  uint32_t  m_geoROMAddress = 0;          // the address latched at 430000
  uint32_t  m_geoOutCount = 0;            // words output this frame (debug log)
  bool      m_geoEndSeen = false, m_geoParked = false;

  // Video
  uint32_t  m_videoControl = 0;
  uint32_t  m_vblankFrame = 0;            // MAME's m_framenum: the frame number, latched at vblank
  bool      m_renderTestMode = false, m_renderMode = false, m_renderUnk = false;
  uint8_t   m_gamma[256];
  std::vector<uint32_t> m_palette;        // 0xFFRRGGBB, from palette RAM through the colour tables
  bool      m_paletteDirty = false;
  CModel1Render::VideoFrame m_video;
  Model2::CVideo::Memory m_videoMemory;     // what the renderer reads textures and colours from
  bool      m_softwareRender = false;       // MAME's software rasterizer instead of the GPU

  // Interrupts and timers
  uint32_t  m_intReq = 0, m_intEna = 0;
  uint32_t  m_timerVal[4] = { 0, 0, 0, 0 };
  uint32_t  m_timerOrig[4] = { 0, 0, 0, 0 };
  bool      m_timerRun[4] = { false, false, false, false };
  int64_t   m_timerStart[4] = { 0, 0, 0, 0 };   // time (units) the timer was loaded

  // I/O
  IOChip    m_io;
  uint8_t   m_ctrlMode = 0;
  uint8_t   m_in[3] = { 0xFF, 0xFF, 0xFF };     // IN0-IN2, active low
  uint8_t   m_analog[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  // Protection and other per-game extras (see InitExtras)
  enum class Extra { None, Crypt5881, Comp0229, DaytonaMaxx, DaytonaGtx };
  Extra                 m_extra = Extra::None;
  std::vector<uint8_t>  m_protRAM;               // 5881/0229: the i960 RAM window at 1D80000 the chip reads
  std::vector<uint8_t>  m_protData;              // Daytona GTX: the protection ROM (2 banks)
  CCrypto               m_crypt5881;
  bool                  m_cryptFirstRead = true;
  Model2::C315_5838     m_comp0229;
  bool                  m_doaDummy = false, m_doaStatus = false;
  uint8_t               m_maxxState = 0, m_gtxState = 0;
  void InitExtras(const Game &game, const ROMSet &rom_set);
  bool ExtraRead(uint32_t addr, uint32_t mask, uint32_t *data);
  bool ExtraWrite(uint32_t addr, uint32_t data, uint32_t mask);

  // The game's controls (MAME's input port sets; see UpdateInputs)
  enum class Layout { Fighting, Soccer, VirtualOn, Daytona, Desert, SegaRally, Bike, Indy500, SkyTarget, AnalogGun,
                      PowerSled, Ski, WaveRunner, WaterSki, TopSkater, Gun };
  static Layout LayoutOf(const std::string &game, const std::string &parent);
  Layout    m_layout = Layout::Fighting;
  uint16_t  m_gun[4] = { 0x0e9, 0x180, 0x0e9, 0x17b };   // light guns (MAME's P1_Y, P1_X, P2_Y, P2_X)
  uint8_t   m_gunOffscreen = 0;                          // bit 0: P1, bit 1: P2
  uint8_t   m_lightgunMux = 0;                           // MAME's m_lightgun_mux (serial channel 2)
  // The racing games' wheel drive board (force feedback), when its ROM is
  // there and force feedback is on: Daytona's and Sega Rally's
  // (CModel2DriveBoard), or the 2B/2C games' EPR-18261 board
  // (CModel2DirectDriveBoard), which answers on port D
  std::unique_ptr<CDriveBoard> m_wheelBoard;
  std::vector<uint8_t> m_wheelBoardROM;
  bool      m_wheelBoardReplies = false;
  bool      m_rchase2Drive = false;                      // Rail Chase 2's drive board on port D/E (MAME's simulation)

  // Drive board (Sega Ski Super G's ski board, Manx TT DX's motion slider),
  // simulated: port E commands, port D replies (see DriveBoardRead)
  struct DriveBoard
  {
    bool     present = false;
    bool     ski = false;      // Ski Super G (else the motion slider)
    bool     initialized = false;
    uint32_t initCommands = 0; // commands received during the power-on sequence
    uint8_t  readMode = 0;     // Dx command: what reads return
  } m_driveBoard;
  uint8_t DriveBoardRead();
  void DriveBoardWrite(uint8_t cmd);
  uint8_t   m_gearSel = 0;                      // last gear selected (MAME's daytona_gearbox_r)
  int       m_ioBoardBootFrames = 0;            // original: frames until the I/O board reports ready
  static constexpr int IO_BOARD_BOOT_FRAMES = 30;

  // Scheduling
  int64_t   m_time[2] = { 0, 0 };
  int64_t   m_lineEnd = 0;
  int       m_line = 0;
  CPUID     m_running = CPU_NONE;
  int       m_uartClockFrac = 0;
  uint64_t  m_frameNumber = 0;
  bool      m_logIO = false;
};

#endif  // INCLUDED_MODEL2_H
