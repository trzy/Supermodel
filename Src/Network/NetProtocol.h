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
 * NetProtocol.h
 *
 * Message format for all TCP connections to and from Supermodel (CNetServer
 * and CTCPConnection). This file does not use other Supermodel files, so
 * external programs (for example, wnaspi32.dll) can also use it.
 *
 * Format
 * ------
 * Each message starts with a MessageHeader:
 *
 *    size    Number of bytes in the message, including the header.
 *    id      Message ID. Gives the type and format of the message.
 *
 * The structures are packed (no padding). All numbers are little endian. The
 * code assumes a little endian computer. Some messages have parts with a
 * variable length after the structure. To read a message, copy its bytes into
 * the structure with memcpy(). Do not use a pointer to a structure in a
 * buffer, because the buffer can have an incorrect alignment.
 *
 * Message IDs
 * -----------
 * An ID is four characters (ID()), so it is easy to read in a hex dump.
 * The first characters identify the subsystem (for example, "SC" for the SCSI
 * TCP bridge and "NB" for the net board). The last character is the version.
 * If the format of a message changes, change its ID.
 *
 * Connection
 * ----------
 * When a connection opens, each side sends Hello as its first message. Then
 * each side can send Ping. The other side replies with Pong. A side closes
 * the connection if:
 *
 *    - The first message is not Hello.
 *    - The message ID is not known.
 *    - The size is less than sizeof(MessageHeader) or more than
 *      MAX_MESSAGE_SIZE.
 */

#ifndef INCLUDED_NETPROTOCOL_H
#define INCLUDED_NETPROTOCOL_H

#include <cstddef>
#include <cstdint>

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__,
  "The Supermodel network protocol is little endian and the code assumes a little endian computer");
#endif

namespace NetProtocol
{
  // Message ID from a string literal of 1 to 4 characters, for example
  // ID("PIN1"). The first character is the first byte sent. If there are
  // less than 4 characters, the remaining bytes are 0.
  template <size_t N>
  constexpr uint32_t ID(const char (&text)[N])
  {
    static_assert(N >= 2 && N <= 5, "A message ID must have 1 to 4 characters");
    uint32_t id = 0;
    for (size_t i = 0; i < N - 1; i++)
      id |= uint32_t(uint8_t(text[i])) << (8 * i);
    return id;
  }

  static constexpr uint16_t DEFAULT_PORT = 1970;

  // Maximum size of a message, including the header
  static constexpr uint32_t MAX_MESSAGE_SIZE = 16 * 1024 * 1024 + 1024;

  // Maximum length of Hello text
  static constexpr uint32_t MAX_HELLO_TEXT = 1024;

#pragma pack(push, 1)

  // Starts each message. A message is a structure derived from MessageHeader.
  // It has a static member ID, and its constructors set the ID and the size.
  // The members are not const, because a receiver copies the bytes of a
  // message into the structure.
  struct MessageHeader
  {
    uint32_t size;            // number of bytes in the message, including this header
    uint32_t id;              // message ID

    MessageHeader()
      : size(sizeof(MessageHeader)),
        id(0)
    {
    }

    MessageHeader(uint32_t id, size_t size)
      : size(uint32_t(size)),
        id(id)
    {
    }
  };

  static_assert(sizeof(MessageHeader) == 8, "MessageHeader must be 8 bytes");

  // First message from each side. Then follows text (not terminated) that
  // identifies the sender, for the log of the other side.
  struct HelloMessage: public MessageHeader
  {
    static constexpr uint32_t ID = NetProtocol::ID("HEL1");

    HelloMessage(size_t textLength = 0)
      : MessageHeader(ID, sizeof(HelloMessage) + textLength)
    {
    }
  };

  static_assert(sizeof(HelloMessage) == 8, "HelloMessage must be 8 bytes");

  // Request for Pong
  struct PingMessage: public MessageHeader
  {
    static constexpr uint32_t ID = NetProtocol::ID("PIN1");

    uint64_t sendTime;        // time of the sender, in microseconds

    PingMessage(uint64_t sendTime = 0)
      : MessageHeader(ID, sizeof(PingMessage)),
        sendTime(sendTime)
    {
    }
  };

  static_assert(sizeof(PingMessage) == 16, "PingMessage must be 16 bytes");

  // Reply to Ping
  struct PongMessage: public MessageHeader
  {
    static constexpr uint32_t ID = NetProtocol::ID("PON1");

    uint64_t pingSendTime;    // sendTime from the Ping
    uint64_t receiveTime;     // time of the replier when it received the Ping, in microseconds

    PongMessage(uint64_t pingSendTime = 0, uint64_t receiveTime = 0)
      : MessageHeader(ID, sizeof(PongMessage)),
        pingSendTime(pingSendTime),
        receiveTime(receiveTime)
    {
    }
  };

  static_assert(sizeof(PongMessage) == 24, "PongMessage must be 24 bytes");

#pragma pack(pop)
}

#endif  // INCLUDED_NETPROTOCOL_H
