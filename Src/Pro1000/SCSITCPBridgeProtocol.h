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
 * SCSITCPBridgeProtocol.h
 *
 * Messages between the SCSI TCP bridge in Supermodel (CSCSITCPBridge) and
 * its clients. A client is a program on the host computer, for example a
 * replacement wnaspi32.dll. This file uses only Network/NetProtocol.h, so
 * clients can also use it.
 *
 * Connection
 * ----------
 * The client connects to the network server of Supermodel (CNetServer). The
 * connection operates as NetProtocol.h gives: each side sends Hello first,
 * and the client must reply to Ping with Pong. There is no authentication.
 *
 * Format
 * ------
 * The messages start with a NetProtocol::MessageHeader. Some messages have
 * parts with a variable length after the structure.
 *
 * The client can send more than one ExecuteCommandMessage before it receives
 * the results. The bridge does the commands in the sequence received. A
 * result has the sequence number of its command.
 */

#ifndef INCLUDED_SCSITCPBRIDGEPROTOCOL_H
#define INCLUDED_SCSITCPBRIDGEPROTOCOL_H

#include "Network/NetProtocol.h"
#include <cstdint>

namespace SCSITCPBridgeProtocol
{
  static constexpr unsigned MAX_CDB_LENGTH = 16;

  // Maximum number of bytes to send to or receive from the target
  static constexpr uint32_t MAX_DATA_LENGTH = 16 * 1024 * 1024;

  // Maximum number of bytes of error text in a result
  static constexpr uint32_t MAX_ERROR_LENGTH = 512;

#pragma pack(push, 1)

  // Client to bridge: do a SCSI command. Then follow:
  //   cdbLength bytes      command descriptor block (CDB)
  //   dataOutLength bytes  data to send to the target
  struct ExecuteCommandMessage: public NetProtocol::MessageHeader
  {
    static constexpr uint32_t ID = NetProtocol::ID("SCX1");

    uint32_t sequence;        // set by the client; the result has the same number
    uint8_t  targetID;
    uint8_t  lun;             // logical unit number
    uint8_t  cdbLength;       // 1 to MAX_CDB_LENGTH
    uint8_t  reserved;        // 0
    uint32_t dataOutLength;   // number of bytes to send to the target
    uint32_t dataInLength;    // maximum number of bytes to receive from the target

    ExecuteCommandMessage(uint32_t sequence = 0, uint8_t targetID = 0, uint8_t lun = 0, uint8_t cdbLength = 0, uint32_t dataOutLength = 0, uint32_t dataInLength = 0)
      : MessageHeader(ID, sizeof(ExecuteCommandMessage) + size_t(cdbLength) + size_t(dataOutLength)),
        sequence(sequence),
        targetID(targetID),
        lun(lun),
        cdbLength(cdbLength),
        reserved(0),
        dataOutLength(dataOutLength),
        dataInLength(dataInLength)
    {
    }
  };

  static_assert(sizeof(ExecuteCommandMessage) == 24, "ExecuteCommandMessage must be 24 bytes");
  static_assert(sizeof(ExecuteCommandMessage) + MAX_CDB_LENGTH + MAX_DATA_LENGTH <= NetProtocol::MAX_MESSAGE_SIZE, "ExecuteCommandMessage can be too large");

  // Bridge to client: the result of an ExecuteCommandMessage. Then follow:
  //   dataInLength bytes   data received from the target
  //   errorLength bytes    error text (not terminated)
  struct CommandResultMessage: public NetProtocol::MessageHeader
  {
    static constexpr uint32_t ID = NetProtocol::ID("SCR1");

    uint32_t sequence;        // sequence number of the command
    uint8_t  completed;       // 1 if the target completed the command, 0 if the command failed (protocol error or reset)
    uint8_t  status;          // SCSI status byte (0x00 = GOOD, 0x02 = CHECK CONDITION)
    uint16_t errorLength;     // number of bytes of error text
    uint32_t dataInLength;    // number of bytes received from the target

    CommandResultMessage(uint32_t sequence = 0, bool completed = false, uint8_t status = 0, uint32_t dataInLength = 0, uint16_t errorLength = 0)
      : MessageHeader(ID, sizeof(CommandResultMessage) + size_t(dataInLength) + size_t(errorLength)),
        sequence(sequence),
        completed(completed ? 1 : 0),
        status(status),
        errorLength(errorLength),
        dataInLength(dataInLength)
    {
    }
  };

  static_assert(sizeof(CommandResultMessage) == 20, "CommandResultMessage must be 20 bytes");
  static_assert(sizeof(CommandResultMessage) + MAX_DATA_LENGTH + MAX_ERROR_LENGTH <= NetProtocol::MAX_MESSAGE_SIZE, "CommandResultMessage can be too large");

#pragma pack(pop)
}

#endif  // INCLUDED_SCSITCPBRIDGEPROTOCOL_H
