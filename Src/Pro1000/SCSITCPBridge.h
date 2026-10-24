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
 * Header file for the CSCSITCPBridge class: the SCSI TCP bridge. It connects
 * programs on a host computer to the virtual SCSI host adapter through the
 * network server (CNetServer).
 *
 * Purpose
 * -------
 * Programs that use the Pro-1000 send SCSI commands through ASPI
 * (wnaspi32.dll). A replacement wnaspi32.dll connects to the network server
 * and sends each command as a message. The bridge gives each command to the
 * virtual SCSI host adapter (CSCSIHostAdapter). When the command completes,
 * the bridge sends the result to the client. SCSITCPBridgeProtocol.h gives
 * the messages.
 *
 * Operation
 * ---------
 * The bridge registers a handler for ExecuteCommandMessage with the network server.
 * The handler operates on the server thread and submits the command to the
 * host adapter. More than one client can connect. The host adapter does the
 * commands in the sequence received.
 *
 * The host adapter calls the completion function on the emulator thread. This
 * function puts the result in the send queue of the connection. If the client
 * disconnects before its command completes, the bridge discards the result.
 */

#ifndef INCLUDED_SCSITCPBRIDGE_H
#define INCLUDED_SCSITCPBRIDGE_H

#include "Pro1000/SCSIHostAdapter.h"
#include "Network/NetServer.h"
#include <cstdint>
#include <memory>

class CSCSITCPBridge
{
public:
  /*
   * CSCSITCPBridge(hostAdapter, server):
   *
   * Parameters:
   *    hostAdapter   The host adapter that does the commands. It must stay
   *                  available while the bridge exists.
   *    server        Network server.
   */
  CSCSITCPBridge(CSCSIHostAdapter &hostAdapter, std::shared_ptr<CNetServer> server);

private:
  bool ProcessCommand(const std::shared_ptr<CTCPConnection> &connection, const uint8_t *message, size_t size);

  CSCSIHostAdapter &m_hostAdapter;
  std::shared_ptr<CNetServer> m_server;
  CNetServer::Registration m_commandHandler;  // last, so that it is removed first
};

#endif  // INCLUDED_SCSITCPBRIDGE_H
