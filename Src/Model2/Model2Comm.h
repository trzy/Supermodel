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
 * Model2Comm.h
 *
 * The Model 2 communication board (837-10537, and the EPR-18643 board of the
 * later games), ported from MAME's m2comm with M2COMM_SIMULATION (Ariane
 * Fugmann; src/mame/sega/m2comm.cpp): the board's Z80 is not run, its link
 * protocol is simulated. The cabinets make a ring; each sends frames to the
 * next one:
 *
 *    FF id count   link setup from the master; each slave takes the next id
 *    FE id count   link established
 *    FC            vsync, from the master once per frame
 *    id data...    one cabinet's part of the ring buffer (shared memory
 *                  2000-), which each cabinet copies to frame offset (1C0)
 *
 * The frames go to the next machine through CNetBoardLink, Supermodel's ring
 * connection (the Network options, as the Model 3 net board). Without the
 * network the board is linked to itself, which is what MAME does with no
 * other machine on its sockets: a master runs the link alone.
 *
 * Based on original work by Ariane Fugmann in MAME. Portions Copyright (c)
 * Ariane Fugmann, used under the BSD 3-Clause License: see Docs/README.txt.
 */

#ifndef INCLUDED_MODEL2COMM_H
#define INCLUDED_MODEL2COMM_H

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

class CModel2Comm
{
public:
  // shared memory (01A00000-01A03FFF), cn (01A04000), fg (01A04002)
  uint8_t ReadShared(unsigned offset) const { return m_shared[offset & 0x3fff]; }
  void    WriteShared(unsigned offset, uint8_t data) { m_shared[offset & 0x3fff] = data; }
  uint8_t ReadCN() const { return m_cn | 0xfe; }
  void    WriteCN(uint8_t data);
  uint8_t ReadFG();
  void    WriteFG(uint8_t data) { m_fg = data & 1; }

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

  bool Networked() const { return m_link != nullptr; }

  CModel2Comm();
  ~CModel2Comm();

private:
  void ReadFGLink();
  int  ReadFrame(int dataSize, bool wait = false);
  void SendData(uint8_t frameType, int frameStart, int frameSize, int dataSize);
  void SendFrame(int dataSize);
  bool LinesOpen() const;
  void ConnectProc();

  uint8_t  m_shared[0x4000];
  uint8_t  m_zfg = 0;             // the Z80 side's flip gate (FG bit 7 reads its inverse)
  uint8_t  m_cn = 0;              // board enable
  uint8_t  m_fg = 0;              // the i960 side's flip gate
  uint8_t  m_buffer0[0x4000];
  bool     m_frameSync = false;
  uint16_t m_frameOffset = 0x1c0; // 180 in Power Sled
  uint8_t  m_linkEnable = 0;
  uint16_t m_linkTimer = 0;
  uint8_t  m_linkAlive = 0;       // 0 linking, 1 established, 2 failed
  uint8_t  m_linkID = 0;
  uint8_t  m_linkCount = 0;
  uint8_t  m_zfgDelay = 0;

  // Linked to itself: the frames sent, received at the next read
  std::deque<std::vector<uint8_t>> m_loopback;

  // Linked to other machines
  std::unique_ptr<CNetBoardLink> m_link;
  std::thread m_connectThread;
  std::atomic_bool m_connected = false;
  std::atomic_bool m_quit = false;
};

#endif  // INCLUDED_MODEL2COMM_H
