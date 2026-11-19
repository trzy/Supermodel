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
 * Model1Comm.h
 *
 * The Model 1 communication board (837-8842: Virtua Racing, Virtua Formula,
 * Wing War), ported from MAME's m1comm with M1COMM_SIMULATION (Ariane
 * Fugmann; src/mame/sega/m1comm.cpp): the board's Z80, DMA and data link
 * controller are not run, its link protocol is simulated. The cabinets make a
 * ring; each passes every frame on to the next one:
 *
 *    FF id         link setup from the master; each slave takes the next id
 *    FE count      link established
 *    FD status     the master's 10 status bytes (shared memory 06-0F)
 *    FC            vsync, from the master once per frame
 *    id data...    one cabinet's part of the ring buffer (1C4 bytes from
 *                  shared memory 10), which each cabinet stores in its slot
 *                  10 + id * 1C4
 *
 * The game sets the role in shared memory 01 (1 master, 2 slave, 0 relay) and
 * the board reports the link in 00 (5 linking, 1 established, FF failed), the
 * id in 02, the number of cabinets in 03; 04 is the game's ready-to-send flag.
 *
 * The frames go to the next machine through CNetBoardLink, as the Model 2
 * board's (the Network options). Without the network the board is linked to
 * itself: a master runs the link alone.
 *
 * Based on original work by Ariane Fugmann in MAME. Portions Copyright (c)
 * Ariane Fugmann, used under the BSD 3-Clause License: see Docs/README.txt.
 */

#ifndef INCLUDED_MODEL1COMM_H
#define INCLUDED_MODEL1COMM_H

#include "Network/NetServer.h"
#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <thread>
#include <vector>

class CBlockFile;
class CNetBoardLink;

class CModel1Comm
{
public:
  // shared memory (B00000-B00FFF, bytes), cn (B01000), fg (B01002)
  uint8_t ReadShared(unsigned offset) const { return m_shared[offset & 0xfff]; }
  void    WriteShared(unsigned offset, uint8_t data) { m_shared[offset & 0xfff] = data; }
  uint8_t ReadCN() const { return m_cn | 0xfe; }
  void    WriteCN(uint8_t data);
  uint8_t ReadFG() const { return (m_fg | (~m_zfg << 7) | 0x7e) & 0xff; }
  void    WriteFG(uint8_t data);

  // At vblank (MAME's check_vint_irq)
  void Tick();

  void Reset();
  void SaveState(CBlockFile *file);
  void LoadState(CBlockFile *file);

  /*
   * Init(server, address, port, frameSync):
   *
   * With a server, frames go to the next machine at address:port; without
   * one the board is linked to itself. frameSync: MAME's -comm_framesync, the
   * machines wait for each other's vsync (lock step).
   */
  void Init(std::shared_ptr<CNetServer> server, const std::string &address, uint16_t port, bool frameSync);

  CModel1Comm();
  ~CModel1Comm();

private:
  int  ReadFrame(bool wait = false);
  void SendData(uint8_t frameType, int frameStart, int frameSize);
  void SendFrame();
  bool LinesOpen() const;
  void ConnectProc();

  static constexpr int FRAME_START = 0x10;
  static constexpr int FRAME_SIZE = 0x1c4;
  static constexpr int DATA_SIZE = FRAME_SIZE + 1;

  uint8_t  m_shared[0x1000];
  uint8_t  m_zfg = 0;             // the Z80 side's flip gate (FG bit 7 reads its inverse)
  uint8_t  m_cn = 0;              // board enable
  uint8_t  m_fg = 0;              // the V60 side's flip gate
  uint8_t  m_buffer0[0x200];
  bool     m_frameSync = false;
  uint8_t  m_linkEnable = 0;
  uint16_t m_linkTimer = 0;
  uint8_t  m_linkAlive = 0;       // 0 linking, 1 established, 2 failed
  uint8_t  m_linkID = 0;
  uint8_t  m_linkCount = 0;

  // Linked to itself: the frames sent, received at the next read
  std::deque<std::vector<uint8_t>> m_loopback;

  // Linked to other machines
  std::unique_ptr<CNetBoardLink> m_link;
  std::thread m_connectThread;
  std::atomic_bool m_connected = false;
  std::atomic_bool m_quit = false;
};

#endif  // INCLUDED_MODEL1COMM_H
