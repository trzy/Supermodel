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
 * NetBoardLink.h
 *
 * Header file for the CNetBoardLink class: the network connections of a net
 * board (CNetBoard or CSimNetBoard).
 *
 * Linked machines make a ring. Each machine sends data to the next machine
 * and receives data from the previous machine. The output connection goes
 * from this machine to the CNetServer of the next machine. The input
 * connection comes from the previous machine to the CNetServer of this
 * machine. One machine can be its own next and previous machine.
 *
 * Messages
 * --------
 *    Join    The previous machine sends it when the output connection opens.
 *            Thus, this machine knows which connection is the input.
 *    Data    One block of net board data. An empty block tells the other
 *            machines that the link is broken.
 *
 * Threads
 * -------
 * Receive() waits until data arrives or the link is broken. The net boards
 * operate in lock step, so this is necessary. The CNetServer thread receives
 * the messages and puts them in a queue for Receive().
 */

#ifndef INCLUDED_NETBOARDLINK_H
#define INCLUDED_NETBOARDLINK_H

#include "Network/NetServer.h"
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class CNetBoardLink
{
public:
  /*
   * Connect(void):
   *
   * Tries one time to open the output connection to the next machine.
   *
   * Returns:
   *    True if the output connection is open.
   */
  bool Connect(void);

  // True if the input and the output connections are open
  bool Connected(void);

  /*
   * Send(data, length):
   *
   * Sends a block of data to the next machine. Does not wait. A length of
   * zero tells the other machines that the link is broken.
   */
  void Send(const void *data, size_t length);

  /*
   * Receive(void):
   *
   * Waits for a block of data from the previous machine.
   *
   * Returns:
   *    The data. Empty if the link is broken (or if the previous machine sent
   *    an empty block). The reference is valid until the next call.
   */
  std::vector<uint8_t> &Receive(void);

  // True if Receive() can return data immediately
  bool CheckDataAvailable(void);

  /*
   * CNetBoardLink(server, address, port):
   *
   * Parameters:
   *    server    Network server of this machine.
   *    address   Address of the next machine.
   *    port      Port of the next machine.
   */
  CNetBoardLink(std::shared_ptr<CNetServer> server, const std::string &address, uint16_t port);
  ~CNetBoardLink(void);

private:
  bool LinkBroken(void) const;
  bool OnJoin(const std::shared_ptr<CTCPConnection> &connection);
  bool OnData(const std::shared_ptr<CTCPConnection> &connection, const uint8_t *message, size_t size);
  void OnDisconnect(const std::shared_ptr<CTCPConnection> &connection);

  std::shared_ptr<CNetServer> m_server;
  const std::string m_address;
  const uint16_t m_port;

  std::mutex m_mutex;
  std::condition_variable m_condition;
  std::shared_ptr<CTCPConnection> m_output;
  std::shared_ptr<CTCPConnection> m_input;
  std::deque<std::vector<uint8_t>> m_queue;
  std::vector<uint8_t> m_received;      // last block that Receive() returned

  CNetServer::Registration m_joinHandler;
  CNetServer::Registration m_dataHandler;
  CNetServer::Registration m_disconnectHandler;
};

#endif  // INCLUDED_NETBOARDLINK_H
