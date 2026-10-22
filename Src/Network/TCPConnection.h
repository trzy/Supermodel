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
 * TCPConnection.h
 *
 * Header file for the CTCPConnection class: one TCP connection of a
 * CNetServer, incoming or outgoing. NetProtocol.h gives the message format.
 *
 * Only CNetServer makes connections (it accepts them, or CNetServer::Connect()
 * opens them). The server thread receives messages for all of its
 * connections. The connection handles Hello, Ping, and Pong. The server gives
 * all other messages to the handlers that are registered for their IDs.
 *
 * Each connection has a send thread. Send() puts a message in a queue and
 * does not wait for the send. Messages go out in the sequence of the Send()
 * calls.
 */

#ifndef INCLUDED_TCPCONNECTION_H
#define INCLUDED_TCPCONNECTION_H

#include "Network/NetProtocol.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

class CNetServer;

class CTCPConnection: public std::enable_shared_from_this<CTCPConnection>
{
public:
  /*
   * Send(message, size):
   * Send(message):
   *
   * Puts a message in the send queue. The message starts with a
   * NetProtocol::MessageHeader. The size in the header must be the number of
   * bytes in the message. Thread safe.
   *
   * Returns:
   *    False if the connection is closed, if the message is too small or too
   *    large, or if the size in the header is not correct.
   */
  bool Send(const void *message, size_t size);
  bool Send(std::vector<uint8_t> message);

  template <typename T>
  bool Send(const T &message)
  {
    static_assert(std::is_base_of<NetProtocol::MessageHeader, T>::value, "A message must start with a MessageHeader");
    return Send(&message, sizeof(T));
  }

  /*
   * Ping(void):
   *
   * Sends Ping. When Pong comes back, GetRoundTripTime() gives the new value.
   * The server also sends Ping every 30 seconds after Hello is received. Thread
   * safe.
   */
  void Ping(void);

  /*
   * Close(void):
   *
   * Requests the server to close the connection. Messages that are in the
   * send queue are sent first. Thread safe.
   */
  void Close(void);

  bool IsOpen(void) const
  {
    return m_open;
  }

  bool IsOutgoing(void) const
  {
    return m_outgoing;
  }

  // Address and port of the other side, for example "192.168.1.5:1970"
  const std::string &GetPeerName(void) const
  {
    return m_peerName;
  }

  // Hello text from the other side. Empty until Hello is received.
  std::string GetPeerHello(void) const;

  // Round trip time from the last Pong in microseconds, or -1 if unknown
  int64_t GetRoundTripTime(void) const
  {
    return m_roundTripTime;
  }

  ~CTCPConnection(void);

private:
  friend class CNetServer;

  CTCPConnection(void *socket, bool outgoing, const std::string &helloText);

  // Server thread only
  bool Receive(CNetServer &server);
  void PingIfDue(uint64_t intervalMicroseconds);
  bool ProcessMessage(CNetServer &server, const uint8_t *message, size_t size);
  void Shutdown(void);
  void *GetSocket(void) const
  {
    return m_socket;
  }
  bool CloseRequested(void) const
  {
    return m_closeRequested;
  }

  void SendThreadFunction(void);

  void *m_socket;                       // TCPsocket
  const bool m_outgoing;
  std::string m_peerName;
  std::atomic<bool> m_open;
  std::atomic<bool> m_closeRequested;
  std::atomic<int64_t> m_roundTripTime;
  std::atomic<uint64_t> m_lastPingTime;  // microseconds

  // Server thread only
  std::vector<uint8_t> m_receiveBuffer;
  bool m_helloReceived = false;
  bool m_shutdown = false;

  mutable std::mutex m_helloMutex;
  std::string m_peerHello;

  // Send queue
  std::mutex m_sendMutex;
  std::condition_variable m_sendCondition;
  std::deque<std::vector<uint8_t>> m_sendQueue;
  bool m_stopSending = false;
  std::thread m_sendThread;
};

#endif  // INCLUDED_TCPCONNECTION_H
