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
 * SCSITCPBridge.cpp
 *
 * Implementation of the CSCSITCPBridge class. Refer to SCSITCPBridge.h for the
 * design and to SCSITCPBridgeProtocol.h for the messages.
 */

#include "SCSITCPBridge.h"

#include "Pro1000/SCSITCPBridgeProtocol.h"
#include "Supermodel.h"
#include <algorithm>
#include <cstring>
#include <vector>

using namespace SCSITCPBridgeProtocol;

// Processes ExecuteCommandMessage. Returns false if the message is not correct; the
// server then disconnects the client.
bool CSCSITCPBridge::ProcessCommand(const std::shared_ptr<CTCPConnection> &connection, const uint8_t *message, size_t size)
{
  // ExecuteCommandMessage, then the CDB and the data out
  ExecuteCommandMessage command;
  if (size < sizeof(command))
  {
    ErrorLog("SCSI TCP bridge: ExecuteCommandMessage is too short.");
    return false;
  }
  memcpy(&command, message, sizeof(command));
  size_t expectedSize = sizeof(command) + size_t(command.cdbLength) + size_t(command.dataOutLength);
  if (command.cdbLength == 0 || command.cdbLength > MAX_CDB_LENGTH || command.dataOutLength > MAX_DATA_LENGTH || command.dataInLength > MAX_DATA_LENGTH || size != expectedSize)
  {
    ErrorLog("SCSI TCP bridge: incorrect ExecuteCommandMessage.");
    return false;
  }
  const uint8_t *cdb = message + sizeof(command);
  const uint8_t *dataOut = cdb + command.cdbLength;

  SCSIRequest request;
  request.targetID = command.targetID;
  request.lun = command.lun;
  request.cdb.assign(cdb, cdb + command.cdbLength);
  request.dataOut.assign(dataOut, dataOut + command.dataOutLength);
  request.dataInLength = command.dataInLength;
  DebugLog("SCSI TCP bridge: command %u: target %u, LUN %u, CDB opcode %02X, %u bytes out, %u bytes in\n", command.sequence, command.targetID, command.lun, cdb[0], command.dataOutLength, command.dataInLength);

  // The completion function sends the result if the client is connected
  std::weak_ptr<CTCPConnection> client = connection;
  uint32_t sequence = command.sequence;
  m_hostAdapter.Submit(request, [client, sequence](const SCSIResult &result)
  {
    std::shared_ptr<CTCPConnection> connection = client.lock();
    if (!connection)
      return;

    uint16_t errorLength = uint16_t(std::min<size_t>(result.error.size(), MAX_ERROR_LENGTH));
    uint32_t dataInLength = uint32_t(std::min<size_t>(result.dataIn.size(), MAX_DATA_LENGTH));
    CommandResultMessage reply(sequence, result.completed, result.status, dataInLength, errorLength);

    std::vector<uint8_t> frame(reply.size);
    uint8_t *p = frame.data();
    memcpy(p, &reply, sizeof(reply));
    p += sizeof(reply);
    if (reply.dataInLength > 0)
      memcpy(p, result.dataIn.data(), reply.dataInLength);
    p += reply.dataInLength;
    if (reply.errorLength > 0)
      memcpy(p, result.error.data(), reply.errorLength);
    connection->Send(std::move(frame));
  });
  return true;
}

CSCSITCPBridge::CSCSITCPBridge(CSCSIHostAdapter &hostAdapter, std::shared_ptr<CNetServer> server)
  : m_hostAdapter(hostAdapter),
    m_server(std::move(server))
{
  m_commandHandler = m_server->RegisterHandler(ExecuteCommandMessage::ID, [this](const std::shared_ptr<CTCPConnection> &connection, const uint8_t *message, size_t size)
  {
    return ProcessCommand(connection, message, size);
  });
}
