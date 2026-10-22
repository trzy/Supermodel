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
 * TCPConnection.cpp
 *
 * Implementation of the CTCPConnection class. Refer to TCPConnection.h.
 */

#include "TCPConnection.h"

#include "Network/NetServer.h"
#include "OSD/Logger.h"
#include "SDLIncludes.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>

using namespace NetProtocol;

// Maximum number of bytes that one receive operation reads
static constexpr size_t RECEIVE_CHUNK_SIZE = 64 * 1024;

static uint64_t GetMicroseconds(void)
{
  return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

// Text from the network can have any bytes. Replace bytes that do not print.
static std::string MakePrintable(const uint8_t *text, size_t length)
{
  std::string result(reinterpret_cast<const char *>(text), length);
  for (char &c: result)
  {
    if (uint8_t(c) < 0x20 || uint8_t(c) > 0x7E)
      c = '?';
  }
  return result;
}


/******************************************************************************
 Send
******************************************************************************/

bool CTCPConnection::Send(const void *message, size_t size)
{
  if (size < sizeof(MessageHeader) || size > MAX_MESSAGE_SIZE)
    return false;
  const uint8_t *p = static_cast<const uint8_t *>(message);
  return Send(std::vector<uint8_t>(p, p + size));
}

bool CTCPConnection::Send(std::vector<uint8_t> message)
{
  if (message.size() < sizeof(MessageHeader) || message.size() > MAX_MESSAGE_SIZE)
    return false;
  MessageHeader header;
  memcpy(&header, message.data(), sizeof(header));
  if (header.size != message.size())
    return false;
  {
    std::lock_guard<std::mutex> lock(m_sendMutex);
    if (!m_open || m_stopSending)
      return false;
    m_sendQueue.push_back(std::move(message));
  }
  m_sendCondition.notify_one();
  return true;
}

void CTCPConnection::Ping(void)
{
  Send(PingMessage(GetMicroseconds()));
}

// Sends the messages in the queue. After Shutdown() requests a stop, sends
// the remaining messages, then stops.
void CTCPConnection::SendThreadFunction(void)
{
  TCPsocket socket = static_cast<TCPsocket>(m_socket);
  while (true)
  {
    std::vector<uint8_t> message;
    {
      std::unique_lock<std::mutex> lock(m_sendMutex);
      m_sendCondition.wait(lock, [this] { return !m_sendQueue.empty() || m_stopSending; });
      if (m_sendQueue.empty())
        return;
      message = std::move(m_sendQueue.front());
      m_sendQueue.pop_front();
    }
    int length = int(message.size());
    if (SDLNet_TCP_Send(socket, message.data(), length) != length)
    {
      // The other side disconnected. The server thread closes the socket.
      m_open = false;
      m_closeRequested = true;
      return;
    }
  }
}


/******************************************************************************
 Receive
******************************************************************************/

// Receives the bytes that are available and processes all complete messages.
// Returns false if the connection must close.
bool CTCPConnection::Receive(CNetServer &server)
{
  size_t used = m_receiveBuffer.size();
  m_receiveBuffer.resize(used + RECEIVE_CHUNK_SIZE);
  int received = SDLNet_TCP_Recv(static_cast<TCPsocket>(m_socket), m_receiveBuffer.data() + used, int(RECEIVE_CHUNK_SIZE));
  if (received <= 0)
  {
    m_receiveBuffer.resize(used);
    return false;   // the other side disconnected
  }
  m_receiveBuffer.resize(used + size_t(received));

  size_t offset = 0;
  while (m_receiveBuffer.size() - offset >= sizeof(MessageHeader))
  {
    MessageHeader header;
    memcpy(&header, m_receiveBuffer.data() + offset, sizeof(header));
    if (header.size < sizeof(MessageHeader) || header.size > MAX_MESSAGE_SIZE)
    {
      ErrorLog("Network: %s sent a message of %u bytes, which is not a permitted size. Connection closed.", m_peerName.c_str(), header.size);
      return false;
    }
    if (m_receiveBuffer.size() - offset < header.size)
      break;  // remainder of message not received yet
    if (!ProcessMessage(server, m_receiveBuffer.data() + offset, header.size))
      return false;
    offset += header.size;
  }
  m_receiveBuffer.erase(m_receiveBuffer.begin(), m_receiveBuffer.begin() + offset);
  return true;
}

bool CTCPConnection::ProcessMessage(CNetServer &server, const uint8_t *message, size_t size)
{
  MessageHeader header;
  memcpy(&header, message, sizeof(header));

  if (header.id == HelloMessage::ID)
  {
    size_t textLength = size - sizeof(HelloMessage);
    if (m_helloReceived || textLength > MAX_HELLO_TEXT)
    {
      ErrorLog("Network: %s sent an incorrect Hello. Connection closed.", m_peerName.c_str());
      return false;
    }
    m_helloReceived = true;
    std::string text = MakePrintable(message + sizeof(HelloMessage), textLength);
    {
      std::lock_guard<std::mutex> lock(m_helloMutex);
      m_peerHello = text;
    }
    CNetServer::LogInfo("Network: %s is %s.", m_peerName.c_str(), text.c_str());
    Ping();
    return true;
  }

  if (!m_helloReceived)
  {
    ErrorLog("Network: first message from %s is not Hello. Connection closed.", m_peerName.c_str());
    return false;
  }

  if (header.id == PingMessage::ID)
  {
    PingMessage ping;
    if (size != sizeof(ping))
    {
      ErrorLog("Network: %s sent an incorrect Ping. Connection closed.", m_peerName.c_str());
      return false;
    }
    memcpy(&ping, message, sizeof(ping));
    Send(PongMessage(ping.sendTime, GetMicroseconds()));
    return true;
  }

  if (header.id == PongMessage::ID)
  {
    PongMessage pong;
    if (size != sizeof(pong))
    {
      ErrorLog("Network: %s sent an incorrect Pong. Connection closed.", m_peerName.c_str());
      return false;
    }
    memcpy(&pong, message, sizeof(pong));
    int64_t roundTripTime = int64_t(GetMicroseconds() - pong.pingSendTime);
    if (m_roundTripTime.exchange(roundTripTime) < 0)
      CNetServer::LogInfo("Network: round trip time to %s is %1.3f ms.", m_peerName.c_str(), double(roundTripTime) / 1000.0);
    return true;
  }

  return server.Dispatch(shared_from_this(), header.id, message, size);
}


/******************************************************************************
 Open and Close
******************************************************************************/

std::string CTCPConnection::GetPeerHello(void) const
{
  std::lock_guard<std::mutex> lock(m_helloMutex);
  return m_peerHello;
}

void CTCPConnection::Close(void)
{
  m_open = false;
  m_closeRequested = true;
}

// Stops the send thread and closes the socket. If the other side is
// connected, the messages in the queue are sent first.
void CTCPConnection::Shutdown(void)
{
  if (m_shutdown)
    return;
  m_shutdown = true;
  m_open = false;
  {
    std::lock_guard<std::mutex> lock(m_sendMutex);
    m_stopSending = true;
  }
  m_sendCondition.notify_one();
  if (m_sendThread.joinable())
    m_sendThread.join();
  SDLNet_TCP_Close(static_cast<TCPsocket>(m_socket));
  m_socket = nullptr;
}

CTCPConnection::CTCPConnection(void *socket, bool outgoing, const std::string &helloText)
  : m_socket(socket),
    m_outgoing(outgoing),
    m_open(true),
    m_closeRequested(false),
    m_roundTripTime(-1)
{
  IPaddress *peer = SDLNet_TCP_GetPeerAddress(static_cast<TCPsocket>(socket));
  if (peer)
  {
    uint32_t host = SDL_SwapBE32(peer->host);
    char name[32];
    snprintf(name, sizeof(name), "%u.%u.%u.%u:%u", host >> 24, (host >> 16) & 0xFF, (host >> 8) & 0xFF, host & 0xFF, unsigned(SDL_SwapBE16(peer->port)));
    m_peerName = name;
  }
  else
    m_peerName = "unknown address";

  // Hello is the first message
  size_t textLength = std::min<size_t>(helloText.size(), MAX_HELLO_TEXT);
  HelloMessage hello(textLength);
  std::vector<uint8_t> message(hello.size);
  memcpy(message.data(), &hello, sizeof(hello));
  memcpy(message.data() + sizeof(hello), helloText.data(), textLength);
  m_sendQueue.push_back(std::move(message));

  m_sendThread = std::thread(&CTCPConnection::SendThreadFunction, this);
}

CTCPConnection::~CTCPConnection(void)
{
  Shutdown();
}
