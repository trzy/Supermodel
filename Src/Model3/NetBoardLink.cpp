/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2003-2026 The Supermodel Team
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
 * NetBoardLink.cpp
 *
 * Implementation of the CNetBoardLink class. Refer to NetBoardLink.h.
 */

#include "NetBoardLink.h"

#include "OSD/Logger.h"
#include <cstring>

namespace NetBoardProtocol
{
#pragma pack(push, 1)

  struct JoinMessage: public NetProtocol::MessageHeader
  {
    static constexpr uint32_t ID = NetProtocol::ID("NBJ1");

    JoinMessage()
      : MessageHeader(ID, sizeof(JoinMessage))
    {
    }
  };

  static_assert(sizeof(JoinMessage) == 8, "JoinMessage must be 8 bytes");

  // Then follows the block of data
  struct DataMessage: public NetProtocol::MessageHeader
  {
    static constexpr uint32_t ID = NetProtocol::ID("NBD1");

    DataMessage(size_t dataLength = 0)
      : MessageHeader(ID, sizeof(DataMessage) + dataLength)
    {
    }
  };

  static_assert(sizeof(DataMessage) == 8, "DataMessage must be 8 bytes");

#pragma pack(pop)
}

using namespace NetBoardProtocol;


/******************************************************************************
 Send and Receive
******************************************************************************/

void CNetBoardLink::Send(const void *data, size_t length)
{
  std::shared_ptr<CTCPConnection> output;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    output = m_output;
  }
  if (!output)
    return;
  DataMessage header(length);
  std::vector<uint8_t> message(header.size);
  memcpy(message.data(), &header, sizeof(header));
  if (length > 0)
    memcpy(message.data() + sizeof(header), data, length);
  output->Send(std::move(message));
}

// Caller must hold m_mutex
bool CNetBoardLink::LinkBroken(void) const
{
  return !m_input || (m_output && !m_output->IsOpen());
}

std::vector<uint8_t> &CNetBoardLink::Receive(void)
{
  std::unique_lock<std::mutex> lock(m_mutex);
  m_condition.wait(lock, [this] { return !m_queue.empty() || LinkBroken(); });
  if (m_queue.empty())
    m_received.clear();
  else
  {
    m_received = std::move(m_queue.front());
    m_queue.pop_front();
  }
  return m_received;
}

bool CNetBoardLink::CheckDataAvailable(void)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return !m_queue.empty();
}


/******************************************************************************
 Connections
******************************************************************************/

bool CNetBoardLink::Connect(void)
{
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_output && m_output->IsOpen())
      return true;
  }
  std::shared_ptr<CTCPConnection> output = m_server->Connect(m_address, m_port);
  if (!output)
    return false;
  output->Send(JoinMessage());
  std::lock_guard<std::mutex> lock(m_mutex);
  m_output = output;
  return true;
}

bool CNetBoardLink::Connected(void)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_input && m_output && m_output->IsOpen();
}

bool CNetBoardLink::OnJoin(const std::shared_ptr<CTCPConnection> &connection)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_input && m_input != connection && m_input->IsOpen())
  {
    ErrorLog("Net board: %s tried to link, but %s is the previous machine. Make sure that each machine has a different next machine.", connection->GetPeerName().c_str(), m_input->GetPeerName().c_str());
    return false;
  }
  m_input = connection;
  m_condition.notify_all();
  return true;
}

bool CNetBoardLink::OnData(const std::shared_ptr<CTCPConnection> &connection, const uint8_t *message, size_t size)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (connection != m_input)
  {
    ErrorLog("Net board: %s sent data before it linked.", connection->GetPeerName().c_str());
    return false;
  }
  m_queue.emplace_back(message + sizeof(DataMessage), message + size);
  m_condition.notify_all();
  return true;
}

void CNetBoardLink::OnDisconnect(const std::shared_ptr<CTCPConnection> &connection)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (connection == m_input)
    m_input.reset();
  m_condition.notify_all();   // also for the output connection
}

CNetBoardLink::CNetBoardLink(std::shared_ptr<CNetServer> server, const std::string &address, uint16_t port)
  : m_server(std::move(server)),
    m_address(address),
    m_port(port)
{
  m_joinHandler = m_server->RegisterHandler(JoinMessage::ID, [this](const std::shared_ptr<CTCPConnection> &connection, const uint8_t *, size_t size)
  {
    return size == sizeof(JoinMessage) && OnJoin(connection);
  });
  m_dataHandler = m_server->RegisterHandler(DataMessage::ID, [this](const std::shared_ptr<CTCPConnection> &connection, const uint8_t *message, size_t size)
  {
    return OnData(connection, message, size);
  });
  m_disconnectHandler = m_server->RegisterDisconnectHandler([this](const std::shared_ptr<CTCPConnection> &connection)
  {
    OnDisconnect(connection);
  });
}

CNetBoardLink::~CNetBoardLink(void)
{
  m_joinHandler.Reset();
  m_dataHandler.Reset();
  m_disconnectHandler.Reset();
  if (m_output)
    m_output->Close();
}
