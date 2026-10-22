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
 * NetServer.h
 *
 * Header file for the CNetServer class: the one TCP port of Supermodel. All
 * network subsystems (the net board, the SCSI TCP bridge) use it.
 * NetProtocol.h gives the message format.
 *
 * Operation
 * ---------
 * The server accepts connections from any computer. Connect() opens
 * connections to other computers. The two types of connection operate in the
 * same way (CTCPConnection).
 *
 * A subsystem registers a handler for each message ID that it uses. The
 * server gives each message to the handlers for its ID. If no handler is
 * registered for an ID, or if a handler returns false, the server closes the
 * connection. Thus, a client that connects to the incorrect subsystem (for
 * example, a net board that connects to a Pro-1000) is disconnected.
 *
 * Threads
 * -------
 * The server has one thread. It accepts connections, receives messages, and
 * calls all handlers. Handlers must not block.
 *
 * RegisterHandler() and RegisterDisconnectHandler() give a Registration. When
 * the Registration is destroyed, the handler is removed. If the handler
 * operates on a different thread at that time, the destructor waits until it
 * completes. Thus, after the destructor, the handler does not operate again.
 * A handler can remove itself.
 */

#ifndef INCLUDED_NETSERVER_H
#define INCLUDED_NETSERVER_H

#include "Network/TCPConnection.h"
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class CNetServer
{
public:
  using MessageHandler = std::function<bool(const std::shared_ptr<CTCPConnection> &connection, const uint8_t *message, size_t size)>;
  using DisconnectHandler = std::function<void(const std::shared_ptr<CTCPConnection> &connection)>;

  struct Handlers;

  class Registration
  {
  public:
    // Removes the handler
    void Reset(void);

    Registration(void) = default;
    Registration(Registration &&other) noexcept;
    Registration &operator=(Registration &&other) noexcept;
    Registration(const Registration &) = delete;
    Registration &operator=(const Registration &) = delete;
    ~Registration(void);

  private:
    friend class CNetServer;
    Registration(std::weak_ptr<Handlers> handlers, uint64_t id);

    std::weak_ptr<Handlers> m_handlers;
    uint64_t m_id = 0;
  };

  /*
   * Start(void):
   *
   * Starts the server thread and listens on the TCP port.
   *
   * Returns:
   *    False if the server cannot listen on the port. Connect() can still
   *    operate.
   */
  bool Start(void);

  /*
   * RegisterHandler(id, handler):
   *
   * Registers a handler for messages with an ID. More than one handler can
   * have the same ID. Hello, Ping, and Pong do not go to handlers.
   *
   * The handler receives the full message, including the header. It returns
   * false if the message is not correct. The server then closes the
   * connection.
   */
  [[nodiscard]] Registration RegisterHandler(uint32_t id, MessageHandler handler);

  /*
   * RegisterDisconnectHandler(handler):
   *
   * Registers a handler that the server calls when a connection closes.
   */
  [[nodiscard]] Registration RegisterDisconnectHandler(DisconnectHandler handler);

  /*
   * Connect(host, port):
   *
   * Opens a connection to a different computer (or to this one). Waits until
   * the connection is open or fails. Start() must be called first.
   *
   * Returns:
   *    The connection, or nullptr if it cannot open.
   */
  std::shared_ptr<CTCPConnection> Connect(const std::string &host, uint16_t port);

  uint16_t GetPort(void) const
  {
    return m_port;
  }

  // Message ID as text for the log, for example "'HEL1'"
  static std::string IDToText(uint32_t id);

  /*
   * CNetServer(port, helloText):
   *
   * Parameters:
   *    port        TCP port.
   *    helloText   Text that this side sends in Hello on each connection.
   */
  CNetServer(uint16_t port, const std::string &helloText);
  ~CNetServer(void);

private:
  friend class CTCPConnection;

  // Writes to the log and to the console
  static void LogInfo(const char *fmt, ...);

  // Gives a message to the handlers. Returns false if the connection must
  // close.
  bool Dispatch(const std::shared_ptr<CTCPConnection> &connection, uint32_t id, const uint8_t *message, size_t size);
  void ThreadFunction(void);
  void Accept(void);
  bool AddConnection(const std::shared_ptr<CTCPConnection> &connection);
  void RemoveConnection(const std::shared_ptr<CTCPConnection> &connection);

  const uint16_t m_port;
  const std::string m_helloText;
  std::shared_ptr<Handlers> m_handlers;
  std::atomic<bool> m_running;
  std::thread m_thread;
  bool m_netInitialized = false;
  void *m_listenSocket = nullptr;       // TCPsocket
  void *m_socketSet = nullptr;          // SDLNet_SocketSet

  // Server thread only
  std::vector<std::shared_ptr<CTCPConnection>> m_connections;

  // Connections from Connect() that the server thread did not add yet
  std::mutex m_pendingMutex;
  std::vector<std::shared_ptr<CTCPConnection>> m_pending;
};

#endif  // INCLUDED_NETSERVER_H
