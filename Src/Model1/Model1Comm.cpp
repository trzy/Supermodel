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
 * Model1Comm.cpp
 *
 * The Model 1 communication board: MAME's m1comm (M1COMM_SIMULATION), its
 * sockets replaced by CNetBoardLink, or by a loopback when not networked.
 *
 * Based on original work by Ariane Fugmann in MAME. Portions Copyright (c)
 * Ariane Fugmann, used under the BSD 3-Clause License: see Docs/README.txt.
 */

#include "Model1/Model1Comm.h"
#include "Model3/NetBoardLink.h"
#include "BlockFile.h"
#include "OSD/Logger.h"
#include "OSD/Thread.h"
#include <algorithm>
#include <chrono>
#include <cstring>

CModel1Comm::CModel1Comm() :
    m_shared{},
    m_buffer0{}
{
}

CModel1Comm::~CModel1Comm()
{
  m_quit = true;
  if (m_connectThread.joinable())
    m_connectThread.join();
}

void CModel1Comm::Init(std::shared_ptr<CNetServer> server, const std::string &address, uint16_t port, bool frameSync)
{
  m_frameSync = frameSync;
  if (server)
  {
    m_link = std::make_unique<CNetBoardLink>(server, address, port);
    m_connectThread = std::thread(&CModel1Comm::ConnectProc, this);
  }
}

// As CModel2Comm: the output connection, then the previous machine's
void CModel1Comm::ConnectProc()
{
  Thread::SetCurrentName("Model1CommConnect");
  while (!m_link->Connect())
  {
    if (m_quit)
      return;
  }
  while (!m_link->Connected())
  {
    if (m_quit)
      return;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    // The next machine closes the connection if our join reaches it before
    // its own link exists (its server starts first): connect again
    m_link->Connect();
  }
  InfoLog("Model 1 communication board: connected.");
  m_connected = true;
}

// MAME: both sockets are open
bool CModel1Comm::LinesOpen() const
{
  return !m_link || m_connected;
}

void CModel1Comm::Reset()
{
  // MAME's device_reset (the board stays disabled until the game writes CN)
  m_zfg = 0;
  m_cn = 0;
  m_fg = 0;
  m_linkEnable = 0;
  m_linkAlive = 0;
  m_linkTimer = 0;
  m_loopback.clear();
}

void CModel1Comm::WriteCN(uint8_t data)
{
  m_cn = data & 1;
  if (!m_cn)
  {
    // reset command
    m_linkEnable = 0;
    m_zfg = 0;
  }
  else
  {
    // init command
    m_linkEnable = 1;
    m_linkID = 0;
    m_linkAlive = 0;
    m_linkCount = 0;
    m_linkTimer = 0xe8;   // 58 fps * 4 s
  }
}

void CModel1Comm::WriteFG(uint8_t data)
{
  if (!m_cn)
    return;
  m_fg = data & 1;
}

void CModel1Comm::Tick()
{
  if (m_linkEnable != 1)
    return;

  int recv = 0;
  int idx = 0;

  bool isMaster = m_shared[1] == 1;
  bool isSlave = m_shared[1] == 2;
  bool isRelay = m_shared[1] == 0;

  if (m_linkAlive == 2)
  {
    // link failed
    m_shared[0] = 0xff;
    return;
  }
  else if (m_linkAlive == 0)
  {
    // link not yet established
    m_shared[0] = 0x05;

    if (LinesOpen())
    {
      recv = ReadFrame();
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
            {
              m_buffer0[1]++;
              m_linkID = m_buffer0[1];
            }
            SendFrame();
          }
        }
        else if (idx == 0xfe)
        {
          // FE: link size
          if (isSlave || isRelay)
          {
            m_linkCount = m_buffer0[1];
            SendFrame();
          }
          InfoLog("Model 1 communication board: link established, id %d of %d.", m_linkID, m_linkCount);
          m_linkAlive = 1;
          m_zfg = 1;
          m_shared[0] = 0x01;
          m_shared[2] = m_linkID;
          m_shared[3] = m_linkCount;
        }

        recv = m_linkAlive == 0 ? ReadFrame() : 0;
      }

      // the master, link not yet established
      if (isMaster && m_linkAlive == 0)
      {
        if (m_linkTimer == 1)
        {
          // first packet
          m_buffer0[0] = 0xff;
          m_buffer0[1] = 0x01;
          SendFrame();
        }
        else if (m_linkTimer == 0)
        {
          // second packet
          m_buffer0[0] = 0xfe;
          m_buffer0[1] = m_linkCount;
          SendFrame();
          InfoLog("Model 1 communication board: link established, id %d of %d.", m_linkID, m_linkCount);
          m_linkAlive = 1;
          m_zfg = 1;
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
      recv = ReadFrame(m_linkTimer == 1);
      while (recv > 0)
      {
        idx = m_buffer0[0];
        if (idx > 0 && idx <= m_linkCount)
        {
          // another cabinet's data: into its slot of the ring buffer, and on
          if (idx != m_linkID)
          {
            int frameOffset = FRAME_START + idx * FRAME_SIZE;
            for (int j = 0; j < FRAME_SIZE; j++)
              m_shared[(frameOffset + j) & 0xfff] = m_buffer0[1 + j];
            SendFrame();
          }
        }
        else if (idx == 0xfc)
        {
          // FC: vsync
          m_linkTimer = 0;
          if (!isMaster)
            SendFrame();
        }
        else if (idx == 0xfd)
        {
          // FD: the master's status bytes
          if (!isMaster)
          {
            for (int j = 0; j < 0x0a; j++)
              m_shared[0x06 + j] = m_buffer0[1 + j];
            SendFrame();
          }
        }
        recv = ReadFrame();
      }
    } while (m_linkTimer == 1 && m_linkAlive == 1);

    // wait for the vsync (MAME's -comm_framesync)
    m_linkTimer = m_frameSync ? 1 : 0;

    // a relay sends no data of its own
    if (m_linkID != 0)
    {
      // the game's ready-to-send flag
      if (m_shared[4] != 0)
      {
        SendData(m_linkID, FRAME_START, FRAME_SIZE);
        // and its own slot of the ring buffer
        int frameOffset = FRAME_START + m_linkID * FRAME_SIZE;
        for (int j = 0; j < FRAME_SIZE; j++)
          m_shared[(frameOffset + j) & 0xfff] = m_buffer0[1 + j];
      }

      if (isMaster)
      {
        // the status bytes, then vsync
        SendData(0xfd, 0x06, 0x0a);
        m_buffer0[0] = 0xfc;
        m_buffer0[1] = 0x01;
        SendFrame();
      }
    }

    m_shared[5] = 0x00;
  }
}

// One frame into m_buffer0: its size, or 0 if there is none (wait: until one
// arrives or the link breaks)
int CModel1Comm::ReadFrame(bool wait)
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
      ErrorLog("Model 1 communication board: connection lost.");
      m_linkAlive = 2;
      m_linkTimer = 0;
      m_shared[0] = 0xff;
    }
    return 0;
  }
  memcpy(m_buffer0, frame.data(), std::min<size_t>(frame.size(), sizeof(m_buffer0)));
  return (int) frame.size();
}

void CModel1Comm::SendData(uint8_t frameType, int frameStart, int frameSize)
{
  m_buffer0[0] = frameType;
  for (int i = 0; i < frameSize; i++)
    m_buffer0[1 + i] = m_shared[(frameStart + i) & 0xfff];
  SendFrame();
}

void CModel1Comm::SendFrame()
{
  if (!m_link)
  {
    m_loopback.emplace_back(m_buffer0, m_buffer0 + DATA_SIZE);
    return;
  }
  if (m_connected)
    m_link->Send(m_buffer0, DATA_SIZE);
}

void CModel1Comm::SaveState(CBlockFile *file)
{
  file->NewBlock("Model 1 comm", __FILE__);
  file->Write(m_shared, sizeof(m_shared));
  file->Write(m_zfg);
  file->Write(m_cn);
  file->Write(m_fg);
  file->Write(m_linkEnable);
  file->Write(m_linkTimer);
  file->Write(m_linkAlive);
  file->Write(m_linkID);
  file->Write(m_linkCount);
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

void CModel1Comm::LoadState(CBlockFile *file)
{
  if (Result::OKAY != file->FindBlock("Model 1 comm"))
  {
    ErrorLog("Unable to load Model 1 communication board state. Save state file is corrupt.");
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
  uint32_t n = 0;
  file->Read(n);
  m_loopback.clear();
  for (uint32_t i = 0; i < n && i < 64; i++)
  {
    uint32_t size = 0;
    file->Read(size);
    std::vector<uint8_t> frame(std::min<uint32_t>(size, sizeof(m_buffer0)));
    file->Read(frame.data(), (uint32_t) frame.size());
    m_loopback.push_back(std::move(frame));
  }
}
