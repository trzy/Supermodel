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
 * replacement wnaspi32.dll. This file does not use other Supermodel files, so
 * clients can also use it.
 *
 * Connection
 * ----------
 * The client connects to the bridge with TCP. The bridge accepts connections
 * only from the same computer. The bridge accepts one client at a time.
 *
 * Format
 * ------
 * The structures below give the format of the data. They are packed (no
 * padding). All numbers are little endian. The code assumes a little endian
 * computer.
 *
 * Each message is sent as a frame: a FrameHeader, then the message. Each
 * message starts with a MessageHeader. Some messages have parts with a
 * variable length after the structure.
 *
 * The client can send more than one ExecuteCommand before it receives the
 * results. The bridge does the commands in the sequence received. A result
 * has the sequence number of its command.
 *
 * To read a message, copy its bytes into the structure with memcpy(). Do not
 * use a pointer to a structure in a buffer, because the buffer can have an
 * incorrect alignment.
 */

#ifndef INCLUDED_SCSITCPBRIDGEPROTOCOL_H
#define INCLUDED_SCSITCPBRIDGEPROTOCOL_H

#include <cstdint>

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__,
  "The SCSI TCP bridge protocol is little endian and the code assumes a little endian computer");
#endif

namespace SCSITCPBridgeProtocol
{
  static constexpr uint16_t VERSION = 1;

  // TEMPORARY: the TCP port is fixed until the bridge has a configuration
  // option
  static constexpr uint16_t DEFAULT_PORT = 31000;

  // Maximum size of a message (not including the FrameHeader). A larger
  // frame is an error.
  static constexpr uint32_t MAX_MESSAGE_SIZE = 16 * 1024 * 1024 + 1024;

  static constexpr unsigned MAX_CDB_LENGTH = 16;

  enum class MessageType : uint16_t
  {
    ExecuteCommand = 1,   // client to bridge
    CommandResult = 2     // bridge to client
  };

#pragma pack(push, 1)

  // Starts each frame
  struct FrameHeader
  {
    uint32_t length;          // number of bytes in the message that follows
  };

  // Starts each message
  struct MessageHeader
  {
    uint16_t version;         // VERSION
    uint16_t type;            // MessageType
    uint32_t sequence;        // set by the client; a result has the number of its command
  };

  // Client to bridge: do a SCSI command. Then follow:
  //   cdbLength bytes      command descriptor block (CDB)
  //   dataOutLength bytes  data to send to the target
  struct ExecuteCommand
  {
    MessageHeader header;
    uint8_t  targetID;
    uint8_t  lun;             // logical unit number
    uint8_t  cdbLength;       // 1 to MAX_CDB_LENGTH
    uint8_t  reserved;        // 0
    uint32_t dataOutLength;   // number of bytes to send to the target
    uint32_t dataInLength;    // maximum number of bytes to receive from the target
  };

  // Bridge to client: the result of an ExecuteCommand. Then follow:
  //   dataInLength bytes   data received from the target
  //   errorLength bytes    error text (not terminated)
  struct CommandResult
  {
    MessageHeader header;
    uint8_t  completed;       // 1 if the target completed the command, 0 if the command failed (protocol error or reset)
    uint8_t  status;          // SCSI status byte (0x00 = GOOD, 0x02 = CHECK CONDITION)
    uint16_t errorLength;     // number of bytes of error text
    uint32_t dataInLength;    // number of bytes received from the target
  };

#pragma pack(pop)

  static_assert(sizeof(FrameHeader) == 4, "FrameHeader must be 4 bytes");
  static_assert(sizeof(MessageHeader) == 8, "MessageHeader must be 8 bytes");
  static_assert(sizeof(ExecuteCommand) == 20, "ExecuteCommand must be 20 bytes");
  static_assert(sizeof(CommandResult) == 16, "CommandResult must be 16 bytes");
}

#endif  // INCLUDED_SCSITCPBRIDGEPROTOCOL_H
