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
#include "SDLIncludes.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>

using namespace SCSITCPBridgeProtocol;

// Time to wait for socket activity before the thread checks for other work
static constexpr int POLL_MILLISECONDS = 10;

// Results that wait for the bridge thread to send them. Completion functions
// keep a reference, so the queue stays available after the bridge stops.
struct CSCSITCPBridge::Shared
{
  struct Result
  {
    uint32_t connectionID;
    std::vector<uint8_t> frame;   // FrameHeader and message
  };

  std::mutex mutex;
  std::deque<Result> results;
};


/******************************************************************************
 Messages
******************************************************************************/

bool CSCSITCPBridge::ReceiveAll(void *socket, uint8_t *dest, size_t length)
{
  SDLNet_SocketSet set = static_cast<SDLNet_SocketSet>(m_socketSet);
  while (length > 0)
  {
    if (!m_running)
      return false;
    int ready = SDLNet_CheckSockets(set, POLL_MILLISECONDS);
    if (ready < 0)
      return false;
    if (ready == 0)
      continue;
    int received = SDLNet_TCP_Recv(static_cast<TCPsocket>(socket), dest, int(std::min<size_t>(length, 0x10000)));
    if (received <= 0)
      return false;   // the client disconnected
    dest += received;
    length -= received;
  }
  return true;
}

bool CSCSITCPBridge::ReceiveMessage(void *socket, std::vector<uint8_t> *message)
{
  FrameHeader frame;
  if (!ReceiveAll(socket, reinterpret_cast<uint8_t *>(&frame), sizeof(frame)))
    return false;
  if (frame.length > MAX_MESSAGE_SIZE)
  {
    ErrorLog("SCSI TCP bridge: message of %u bytes is too large.", frame.length);
    return false;
  }
  message->resize(frame.length);
  return ReceiveAll(socket, message->data(), frame.length);
}

bool CSCSITCPBridge::SendFrame(void *socket, const std::vector<uint8_t> &frame)
{
  int length = int(frame.size());
  return SDLNet_TCP_Send(static_cast<TCPsocket>(socket), frame.data(), length) == length;
}

// Processes a message from the client. Returns false if the message is not
// correct; the bridge then disconnects the client.
bool CSCSITCPBridge::ProcessMessage(const std::vector<uint8_t> &message, uint32_t connectionID)
{
  MessageHeader header;
  if (message.size() < sizeof(header))
  {
    ErrorLog("SCSI TCP bridge: message of %u bytes is too short.", unsigned(message.size()));
    return false;
  }
  memcpy(&header, message.data(), sizeof(header));
  if (header.version != VERSION)
  {
    ErrorLog("SCSI TCP bridge: client uses protocol version %u (version %u is necessary).", header.version, VERSION);
    return false;
  }
  if (header.type != uint16_t(MessageType::ExecuteCommand))
  {
    ErrorLog("SCSI TCP bridge: unknown message type %u.", header.type);
    return false;
  }

  // ExecuteCommand, then the CDB and the data out
  ExecuteCommand command;
  if (message.size() < sizeof(command))
  {
    ErrorLog("SCSI TCP bridge: ExecuteCommand message is too short.");
    return false;
  }
  memcpy(&command, message.data(), sizeof(command));
  size_t expectedSize = sizeof(command) + size_t(command.cdbLength) + size_t(command.dataOutLength);
  if (command.cdbLength == 0 || command.cdbLength > MAX_CDB_LENGTH || command.dataInLength > MAX_MESSAGE_SIZE || message.size() != expectedSize)
  {
    ErrorLog("SCSI TCP bridge: incorrect ExecuteCommand message.");
    return false;
  }
  const uint8_t *cdb = message.data() + sizeof(command);
  const uint8_t *dataOut = cdb + command.cdbLength;

  SCSIRequest request;
  request.targetID = command.targetID;
  request.lun = command.lun;
  request.cdb.assign(cdb, cdb + command.cdbLength);
  request.dataOut.assign(dataOut, dataOut + command.dataOutLength);
  request.dataInLength = command.dataInLength;
  DebugLog("SCSI TCP bridge: command %u: target %u, LUN %u, CDB opcode %02X, %u bytes out, %u bytes in\n", header.sequence, command.targetID, command.lun, cdb[0], command.dataOutLength, command.dataInLength);

  // The completion function makes the result frame and puts it in the
  // queue. The bridge thread sends it.
  std::shared_ptr<Shared> shared = m_shared;
  uint32_t sequence = header.sequence;
  m_hostAdapter.Submit(request, [shared, sequence, connectionID](const SCSIResult &result)
  {
    CommandResult reply;
    reply.header.version = VERSION;
    reply.header.type = uint16_t(MessageType::CommandResult);
    reply.header.sequence = sequence;
    reply.completed = result.completed ? 1 : 0;
    reply.status = result.status;
    reply.errorLength = uint16_t(std::min<size_t>(result.error.size(), 0xFFFF));
    reply.dataInLength = uint32_t(result.dataIn.size());

    FrameHeader frameHeader;
    frameHeader.length = uint32_t(sizeof(reply) + reply.dataInLength + reply.errorLength);

    std::vector<uint8_t> frame(sizeof(frameHeader) + frameHeader.length);
    uint8_t *p = frame.data();
    memcpy(p, &frameHeader, sizeof(frameHeader));
    p += sizeof(frameHeader);
    memcpy(p, &reply, sizeof(reply));
    p += sizeof(reply);
    if (reply.dataInLength > 0)
      memcpy(p, result.dataIn.data(), reply.dataInLength);
    p += reply.dataInLength;
    if (reply.errorLength > 0)
      memcpy(p, result.error.data(), reply.errorLength);

    std::lock_guard<std::mutex> lock(shared->mutex);
    shared->results.push_back(Shared::Result{ connectionID, std::move(frame) });
  });
  return true;
}

// Sends the results that are in the queue. Discards results for clients that
// disconnected. Returns false if a send fails.
bool CSCSITCPBridge::SendResults(void *socket, uint32_t connectionID)
{
  std::deque<Shared::Result> results;
  {
    std::lock_guard<std::mutex> lock(m_shared->mutex);
    results.swap(m_shared->results);
  }
  for (auto &result: results)
  {
    if (result.connectionID != connectionID)
      continue;
    if (!SendFrame(socket, result.frame))
      return false;
  }
  return true;
}


/******************************************************************************
 Thread
******************************************************************************/

void CSCSITCPBridge::ThreadFunction(void)
{
  TCPsocket listenSocket = static_cast<TCPsocket>(m_listenSocket);
  SDLNet_SocketSet set = static_cast<SDLNet_SocketSet>(m_socketSet);
  uint32_t connectionID = 0;

  while (m_running)
  {
    // Wait for a client
    TCPsocket client = SDLNet_TCP_Accept(listenSocket);
    if (!client)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(16));
      continue;
    }

    // Clients can be on other computers (the host programs run on Windows,
    // Supermodel possibly does not)
    IPaddress *peer = SDLNet_TCP_GetPeerAddress(client);
    uint32_t host = peer ? SDL_SwapBE32(peer->host) : 0;

    ++connectionID;
    SDLNet_TCP_AddSocket(set, client);
    printf("SCSI TCP bridge: client connected from %u.%u.%u.%u.\n", host >> 24, (host >> 16) & 0xFF, (host >> 8) & 0xFF, host & 0xFF);
    InfoLog("SCSI TCP bridge: client connected from %u.%u.%u.%u.", host >> 24, (host >> 16) & 0xFF, (host >> 8) & 0xFF, host & 0xFF);

    // Receive commands and send results until the client disconnects
    while (m_running)
    {
      if (!SendResults(client, connectionID))
        break;
      int ready = SDLNet_CheckSockets(set, POLL_MILLISECONDS);
      if (ready < 0)
        break;
      if (ready > 0 && SDLNet_SocketReady(client))
      {
        std::vector<uint8_t> message;
        if (!ReceiveMessage(client, &message) || !ProcessMessage(message, connectionID))
          break;
      }
    }

    SDLNet_TCP_DelSocket(set, client);
    SDLNet_TCP_Close(client);
    printf("SCSI TCP bridge: client disconnected.\n");
    InfoLog("SCSI TCP bridge: client disconnected.");
  }
}


/******************************************************************************
 Start and Stop
******************************************************************************/

bool CSCSITCPBridge::Start(void)
{
  if (m_thread.joinable())
    return true;

  if (SDLNet_Init() != 0)
  {
    ErrorLog("SCSI TCP bridge: unable to initialize SDL_net: %s", SDLNet_GetError());
    return false;
  }
  m_netInitialized = true;

  IPaddress address;
  TCPsocket listenSocket = nullptr;
  if (SDLNet_ResolveHost(&address, nullptr, m_port) == 0)
    listenSocket = SDLNet_TCP_Open(&address);
  if (!listenSocket)
  {
    ErrorLog("SCSI TCP bridge: unable to listen on TCP port %u: %s", m_port, SDLNet_GetError());
    return false;
  }
  m_listenSocket = listenSocket;
  m_socketSet = SDLNet_AllocSocketSet(1);

  m_running = true;
  m_thread = std::thread(&CSCSITCPBridge::ThreadFunction, this);
  printf("SCSI TCP bridge: listening on TCP port %u.\n", m_port);
  InfoLog("SCSI TCP bridge: listening on TCP port %u.", m_port);
  return true;
}

CSCSITCPBridge::CSCSITCPBridge(CSCSIHostAdapter &hostAdapter, uint16_t port)
  : m_hostAdapter(hostAdapter),
    m_port(port),
    m_shared(std::make_shared<Shared>()),
    m_running(false)
{
}

CSCSITCPBridge::~CSCSITCPBridge(void)
{
  m_running = false;
  if (m_thread.joinable())
    m_thread.join();
  if (m_socketSet)
    SDLNet_FreeSocketSet(static_cast<SDLNet_SocketSet>(m_socketSet));
  if (m_listenSocket)
    SDLNet_TCP_Close(static_cast<TCPsocket>(m_listenSocket));
  if (m_netInitialized)
    SDLNet_Quit();
}
