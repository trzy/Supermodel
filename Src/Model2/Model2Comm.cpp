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
 * Model2Comm.cpp
 *
 * The Model 2 communication board: MAME's m2comm (M2COMM_SIMULATION), its
 * sockets replaced by CNetBoardLink, or by a loopback when not networked.
 *
 * Based on original work by Ariane Fugmann in MAME. Portions Copyright (c)
 * Ariane Fugmann, used under the BSD 3-Clause License: see Docs/README.txt.
 */

#include "Model2/Model2Comm.h"
#include "Model3/NetBoardLink.h"
#include "BlockFile.h"
#include "OSD/Logger.h"
#include "OSD/Thread.h"
#include <algorithm>
#include <cstring>

CModel2Comm::CModel2Comm()
{
  memset(m_shared, 0, sizeof(m_shared));
  memset(m_buffer0, 0, sizeof(m_buffer0));
}

CModel2Comm::~CModel2Comm()
{
  m_quit = true;
  if (m_connectThread.joinable())
    m_connectThread.join();
}

void CModel2Comm::Init(std::shared_ptr<CNetServer> server, const std::string &address, uint16_t port, bool frameSync)
{
  m_frameSync = frameSync;
  if (server)
  {
    m_link = std::make_unique<CNetBoardLink>(server, address, port);
    m_connectThread = std::thread(&CModel2Comm::ConnectProc, this);
  }
}

// As CSimNetBoard: the output connection, then the previous machine's
void CModel2Comm::ConnectProc()
{
  while (!m_link->Connect())
  {
    if (m_quit)
      return;
  }
  while (!m_link->Connected())
  {
    if (m_quit)
      return;
    CThread::Sleep(1);
  }
  InfoLog("Model 2 communication board: connected.");
  m_connected = true;
}

// MAME: both sockets are open (always, linked to itself)
bool CModel2Comm::LinesOpen() const
{
  return !m_link || m_connected;
}

void CModel2Comm::Reset()
{
  // MAME's device_reset
  m_zfg = 0;
  m_cn = 0;
  m_fg = 0;
  m_linkEnable = 0;
  m_linkAlive = 0;
  m_linkTimer = 0;
  m_zfgDelay = 0;
  m_loopback.clear();
}

void CModel2Comm::WriteCN(uint8_t data)
{
  m_cn = data & 1;
  if (!m_cn)
  {
    // reset command
    m_linkEnable = 0;
    m_zfg = 0;
    m_cn = 0;
    m_fg = 0;
  }
  else
  {
    // init command
    m_linkEnable = 1;
    m_linkID = 0;
    m_linkAlive = 0;
    m_linkCount = 0;
    m_linkTimer = 0xe8;   // 58 fps * 4 s
    memset(m_shared, 0, sizeof(m_shared));
    m_shared[0x01] = 0x02;
    // frame size 0E00
    m_shared[0x12] = 0x00;
    m_shared[0x13] = 0x0e;
    // frame offset
    m_shared[0x14] = m_frameOffset & 0xff;
    m_shared[0x15] = m_frameOffset >> 8;
    Tick();
  }
}

uint8_t CModel2Comm::ReadFG()
{
  ReadFGLink();
  return (m_fg | (~m_zfg << 7) | 0x7e) & 0xff;
}

void CModel2Comm::Tick()
{
  if (m_linkEnable != 1)
    return;

  int frameStart = 0x2000;
  int frameSize = std::min((m_shared[0x13] << 8) | m_shared[0x12], 0x1000);
  int frameOffset = (frameStart | (m_shared[0x15] << 8) | m_shared[0x14]) & 0x3fff;
  int dataSize = frameSize + 1;
  int recv = 0;
  int idx = 0;

  // EPR-16726 uses fg for master/slave; EPR-18643(A) checks shared[1], with a fallback to fg
  bool isMaster = m_fg == 1 || m_shared[1] == 1;
  bool isSlave = m_fg == 0 && m_shared[1] == 2;
  bool isRelay = m_fg == 0 && m_shared[1] == 0;

  if (m_linkAlive == 2)
  {
    // link failed
    m_shared[0] = 0xff;
    return;
  }
  else if (m_linkAlive == 0)
  {
    // link not yet established
    m_shared[0] = 0x00;
    m_shared[2] = 0xff;
    m_shared[3] = 0xff;

    if (LinesOpen())
    {
      m_zfg ^= 1;

      recv = ReadFrame(dataSize);
      while (recv > 0)
      {
        idx = m_buffer0[0];
        if (idx == 0xff)
        {
          // FF: link id
          if (isMaster)
          {
            // the master gets the first id and starts the next state
            m_linkID = 1;
            m_linkCount = m_buffer0[1];
            m_linkTimer = 0;
          }
          else
          {
            // a slave takes its own id, a relay does nothing
            if (isSlave)
              m_buffer0[1]++;
            SendFrame(dataSize);
          }
        }
        else if (idx == 0xfe)
        {
          // FE: link size
          if (isSlave)
          {
            m_linkID = m_buffer0[1];
            m_linkCount = m_buffer0[2];
            m_buffer0[1]--;
            SendFrame(dataSize);
          }
          else if (isRelay)
          {
            m_linkID = 0;
            m_linkCount = m_buffer0[2];
            SendFrame(dataSize);
          }
          InfoLog("Model 2 communication board: link established, id %d of %d.", m_linkID, m_linkCount);
          m_linkAlive = 1;
          m_shared[0] = 0x01;
          m_shared[2] = m_linkID;
          m_shared[3] = m_linkCount;
        }

        recv = m_linkAlive == 0 ? ReadFrame(dataSize) : 0;
      }

      // the master, link not yet established
      if (isMaster && m_linkAlive == 0)
      {
        if (m_linkTimer == 1)
        {
          // first packet
          m_buffer0[0] = 0xff;
          m_buffer0[1] = 0x01;
          m_buffer0[2] = 0x00;
          SendFrame(dataSize);
        }
        else if (m_linkTimer == 0)
        {
          // second packet
          m_buffer0[0] = 0xfe;
          m_buffer0[1] = m_linkCount;
          m_buffer0[2] = m_linkCount;
          SendFrame(dataSize);
          InfoLog("Model 2 communication board: link established, id %d of %d.", m_linkID, m_linkCount);
          m_linkAlive = 1;
          m_shared[0] = 0x01;
          m_shared[2] = m_linkID;
          m_shared[3] = m_linkCount;
        }
        else if (m_linkTimer > 1)
          m_linkTimer--;
      }
    }
  }

  // link established
  if (m_linkAlive == 1)
  {
    do
    {
      // waiting for the vsync (frame sync): the read waits
      recv = ReadFrame(dataSize, m_linkTimer == 1);
      while (recv > 0)
      {
        idx = m_buffer0[0];
        if (idx <= m_linkCount)
        {
          // into the ring buffer
          for (int j = 0; j < frameSize; j++)
            m_shared[(frameOffset + j) & 0x3fff] = m_buffer0[1 + j];
          m_zfg ^= 1;
          if (isSlave)
            SendData(m_linkID, frameStart, frameSize, dataSize);
          else if (isRelay)
            SendFrame(dataSize);
        }
        else if (idx == 0xfc)
        {
          // FC: vsync
          m_linkTimer = 0;
          if (!isMaster)
            SendFrame(dataSize);
        }
        recv = ReadFrame(dataSize);
      }
    } while (m_linkTimer == 1 && m_linkAlive == 1);

    // wait for the vsync (MAME's -comm_framesync)
    m_linkTimer = m_frameSync ? 1 : 0;

    if (isMaster)
    {
      // the ring buffer
      SendData(m_linkID, frameStart, frameSize, dataSize);
      // vsync
      m_buffer0[0] = 0xfc;
      m_buffer0[1] = 0x01;
      SendFrame(dataSize);
    }

    m_zfgDelay = 2;
  }
}

// MAME's read_fg: frames that arrived since the vblank
void CModel2Comm::ReadFGLink()
{
  if (m_zfgDelay > 0)
  {
    m_zfgDelay--;
    return;
  }
  if (m_linkAlive != 1)
    return;

  int frameStart = 0x2000;
  int frameSize = std::min((m_shared[0x13] << 8) | m_shared[0x12], 0x1000);
  int frameOffset = (frameStart | (m_shared[0x15] << 8) | m_shared[0x14]) & 0x3fff;
  int dataSize = frameSize + 1;

  bool isMaster = m_fg == 1 || m_shared[1] == 1;
  bool isSlave = m_fg == 0 && m_shared[1] == 2;
  bool isRelay = m_fg == 0 && m_shared[1] == 0;

  do
  {
    int recv = ReadFrame(dataSize, m_linkTimer == 1);
    while (recv > 0)
    {
      int idx = m_buffer0[0];
      if (idx <= m_linkCount)
      {
        for (int j = 0; j < frameSize; j++)
          m_shared[(frameOffset + j) & 0x3fff] = m_buffer0[1 + j];
        m_zfg ^= 1;
        if (isSlave)
          SendData(m_linkID, frameStart, frameSize, dataSize);
        else if (isRelay)
          SendFrame(dataSize);
      }
      else if (idx == 0xfc)
      {
        m_linkTimer = 0;
        if (!isMaster)
          SendFrame(dataSize);
      }
      recv = ReadFrame(dataSize);
    }
  } while (m_linkTimer == 1 && m_linkAlive == 1);
}

// One frame into m_buffer0: its size, or 0 if there is none (wait: until one
// arrives or the link breaks)
int CModel2Comm::ReadFrame(int dataSize, bool wait)
{
  if (!m_link)
  {
    if (m_loopback.empty())
      return 0;
    std::vector<uint8_t> frame = std::move(m_loopback.front());
    m_loopback.pop_front();
    memcpy(m_buffer0, frame.data(), std::min<size_t>(frame.size(), sizeof(m_buffer0)));
    return (int) frame.size();
  }
  if (!m_connected || (!wait && !m_link->CheckDataAvailable()))
    return 0;
  std::vector<uint8_t> &frame = m_link->Receive();
  if (frame.empty())
  {
    // the previous machine has gone
    if (m_linkAlive == 1)
    {
      ErrorLog("Model 2 communication board: connection lost.");
      m_linkAlive = 2;
      m_linkTimer = 0;
    }
    return 0;
  }
  memcpy(m_buffer0, frame.data(), std::min<size_t>(frame.size(), sizeof(m_buffer0)));
  (void) dataSize;
  return (int) frame.size();
}

void CModel2Comm::SendData(uint8_t frameType, int frameStart, int frameSize, int dataSize)
{
  m_buffer0[0] = frameType;
  for (int i = 0; i < frameSize; i++)
    m_buffer0[1 + i] = m_shared[(frameStart + i) & 0x3fff];
  SendFrame(dataSize);
}

void CModel2Comm::SendFrame(int dataSize)
{
  if (!m_link)
  {
    m_loopback.emplace_back(m_buffer0, m_buffer0 + dataSize);
    return;
  }
  if (m_connected)
    m_link->Send(m_buffer0, dataSize);
}

void CModel2Comm::SaveState(CBlockFile *file)
{
  file->NewBlock("Model 2 comm", __FILE__);
  file->Write(m_shared, sizeof(m_shared));
  file->Write(m_zfg);
  file->Write(m_cn);
  file->Write(m_fg);
  file->Write(m_linkEnable);
  file->Write(m_linkTimer);
  file->Write(m_linkAlive);
  file->Write(m_linkID);
  file->Write(m_linkCount);
  file->Write(m_zfgDelay);
  // the loopback frames (not the network's)
  uint32_t n = (uint32_t) m_loopback.size();
  file->Write(n);
  for (const auto &frame : m_loopback)
  {
    uint32_t size = (uint32_t) frame.size();
    file->Write(size);
    file->Write(frame.data(), size);
  }
}

void CModel2Comm::LoadState(CBlockFile *file)
{
  if (Result::OKAY != file->FindBlock("Model 2 comm"))
  {
    ErrorLog("Unable to load Model 2 communication board state. Save state file is corrupt.");
    return;
  }
  file->Read(m_shared, sizeof(m_shared));
  file->Read(m_zfg);
  file->Read(m_cn);
  file->Read(m_fg);
  file->Read(m_linkEnable);
  file->Read(m_linkTimer);
  file->Read(m_linkAlive);
  file->Read(m_linkID);
  file->Read(m_linkCount);
  file->Read(m_zfgDelay);
  uint32_t n = 0;
  file->Read(n);
  m_loopback.clear();
  for (uint32_t i = 0; i < n && i < 64; i++)
  {
    uint32_t size = 0;
    file->Read(size);
    std::vector<uint8_t> frame(std::min<uint32_t>(size, 0x4000));
    file->Read(frame.data(), (uint32_t) frame.size());
    m_loopback.push_back(std::move(frame));
  }
}
