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
 * SCSITCPBridge.h
 *
 * Header file for the CSCSITCPBridge class: the SCSI TCP bridge. It is a TCP
 * server that connects programs on a host computer to the virtual SCSI host
 * adapter.
 *
 * Purpose
 * -------
 * Programs that use the Pro-1000 send SCSI commands through ASPI
 * (wnaspi32.dll). A replacement wnaspi32.dll connects to this bridge and
 * sends each command as a message. The bridge gives each command to the
 * virtual SCSI host adapter (CSCSIHostAdapter). When the command completes,
 * the bridge sends the result to the client. SCSITCPBridgeProtocol.h gives
 * the messages.
 *
 * Operation
 * ---------
 * The bridge has one thread. The thread accepts a client, receives
 * messages, and sends results. The bridge accepts one client at a time, and
 * only from the same computer. When a client disconnects, the bridge accepts
 * a new client.
 *
 * The host adapter calls the completion function on the emulator thread.
 * This function puts the result in a queue. The bridge thread sends the
 * results from the queue. If the client disconnects before its command
 * completes, the bridge discards the result.
 *
 * The bridge uses SDL_net, so it operates on all platforms.
 */

#ifndef INCLUDED_SCSITCPBRIDGE_H
#define INCLUDED_SCSITCPBRIDGE_H

#include "Pro1000/SCSIHostAdapter.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

class CSCSITCPBridge
{
public:
  /*
   * Start(void):
   *
   * Starts to accept clients.
   *
   * Returns:
   *    True if the bridge is ready for clients. False if it cannot listen on
   *    the TCP port.
   */
  bool Start(void);

  /*
   * CSCSITCPBridge(hostAdapter, port):
   *
   * Parameters:
   *    hostAdapter   The host adapter that does the commands. It must stay
   *                  available while the bridge exists.
   *    port          TCP port.
   */
  CSCSITCPBridge(CSCSIHostAdapter &hostAdapter, uint16_t port);
  ~CSCSITCPBridge(void);

  struct Shared;

private:
  void ThreadFunction(void);
  bool ReceiveAll(void *socket, uint8_t *dest, size_t length);
  bool ReceiveMessage(void *socket, std::vector<uint8_t> *message);
  bool SendFrame(void *socket, const std::vector<uint8_t> &frame);
  bool ProcessMessage(const std::vector<uint8_t> &message, uint32_t connectionID);
  bool SendResults(void *socket, uint32_t connectionID);

  CSCSIHostAdapter &m_hostAdapter;
  const uint16_t m_port;
  std::shared_ptr<Shared> m_shared;   // result queue, shared with completion functions
  std::atomic<bool> m_running;
  std::thread m_thread;
  bool m_netInitialized = false;
  void *m_listenSocket = nullptr;     // TCPsocket
  void *m_socketSet = nullptr;        // SDLNet_SocketSet
};

#endif  // INCLUDED_SCSITCPBRIDGE_H
