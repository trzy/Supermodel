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
 * NetServer.cpp
 *
 * Implementation of the CNetServer class. Refer to NetServer.h.
 */

#include "NetServer.h"

#include "OSD/Logger.h"
#include "SDLIncludes.h"
#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>

// Time to wait for socket activity before the thread checks for other work
static constexpr int POLL_MILLISECONDS = 10;

static constexpr size_t MAX_CONNECTIONS = 16;

struct CNetServer::Handlers
{
  struct Message
  {
    uint64_t registrationID;
    uint32_t id;
    MessageHandler handler;
  };

  struct Disconnect
  {
    uint64_t registrationID;
    DisconnectHandler handler;
  };

  // Recursive, so that a handler can register and remove handlers
  std::recursive_mutex mutex;
  uint64_t nextRegistrationID = 1;
  std::vector<Message> messageHandlers;
  std::vector<Disconnect> disconnectHandlers;
};


/******************************************************************************
 Handlers
******************************************************************************/

void CNetServer::Registration::Reset(void)
{
  if (m_id == 0)
    return;
  if (std::shared_ptr<Handlers> handlers = m_handlers.lock())
  {
    // Waits for a handler that operates on the server thread
    std::lock_guard<std::recursive_mutex> lock(handlers->mutex);
    uint64_t id = m_id;
    auto &messageHandlers = handlers->messageHandlers;
    messageHandlers.erase(std::remove_if(messageHandlers.begin(), messageHandlers.end(), [id](const Handlers::Message &h) { return h.registrationID == id; }), messageHandlers.end());
    auto &disconnectHandlers = handlers->disconnectHandlers;
    disconnectHandlers.erase(std::remove_if(disconnectHandlers.begin(), disconnectHandlers.end(), [id](const Handlers::Disconnect &h) { return h.registrationID == id; }), disconnectHandlers.end());
  }
  m_handlers.reset();
  m_id = 0;
}

CNetServer::Registration::Registration(std::weak_ptr<Handlers> handlers, uint64_t id)
  : m_handlers(std::move(handlers)),
    m_id(id)
{
}

CNetServer::Registration::Registration(Registration &&other) noexcept
  : m_handlers(std::move(other.m_handlers)),
    m_id(other.m_id)
{
  other.m_id = 0;
}

CNetServer::Registration &CNetServer::Registration::operator=(Registration &&other) noexcept
{
  if (this != &other)
  {
    Reset();
    m_handlers = std::move(other.m_handlers);
    m_id = other.m_id;
    other.m_id = 0;
  }
  return *this;
}

CNetServer::Registration::~Registration(void)
{
  Reset();
}

CNetServer::Registration CNetServer::RegisterHandler(uint32_t id, MessageHandler handler)
{
  std::lock_guard<std::recursive_mutex> lock(m_handlers->mutex);
  uint64_t registrationID = m_handlers->nextRegistrationID++;
  m_handlers->messageHandlers.push_back(Handlers::Message{ registrationID, id, std::move(handler) });
  return Registration(m_handlers, registrationID);
}

CNetServer::Registration CNetServer::RegisterDisconnectHandler(DisconnectHandler handler)
{
  std::lock_guard<std::recursive_mutex> lock(m_handlers->mutex);
  uint64_t registrationID = m_handlers->nextRegistrationID++;
  m_handlers->disconnectHandlers.push_back(Handlers::Disconnect{ registrationID, std::move(handler) });
  return Registration(m_handlers, registrationID);
}

bool CNetServer::Dispatch(const std::shared_ptr<CTCPConnection> &connection, uint32_t id, const uint8_t *message, size_t size)
{
  std::lock_guard<std::recursive_mutex> lock(m_handlers->mutex);

  // Copies, because a handler can change the list
  std::vector<MessageHandler> handlers;
  for (auto &h: m_handlers->messageHandlers)
  {
    if (h.id == id)
      handlers.push_back(h.handler);
  }
  if (handlers.empty())
  {
    ErrorLog("Network: %s sent message %s, which is not known. Connection closed.", connection->GetPeerName().c_str(), IDToText(id).c_str());
    return false;
  }

  bool correct = true;
  for (auto &handler: handlers)
    correct = handler(connection, message, size) && correct;
  if (!correct)
    ErrorLog("Network: %s sent an incorrect %s message. Connection closed.", connection->GetPeerName().c_str(), IDToText(id).c_str());
  return correct;
}


/******************************************************************************
 Connections
******************************************************************************/

bool CNetServer::AddConnection(const std::shared_ptr<CTCPConnection> &connection)
{
  if (m_connections.size() >= MAX_CONNECTIONS || SDLNet_TCP_AddSocket(static_cast<SDLNet_SocketSet>(m_socketSet), static_cast<TCPsocket>(connection->GetSocket())) < 0)
  {
    ErrorLog("Network: too many connections. Connection to %s refused.", connection->GetPeerName().c_str());
    return false;
  }
  m_connections.push_back(connection);
  LogInfo(connection->IsOutgoing() ? "Network: connected to %s." : "Network: connection from %s.", connection->GetPeerName().c_str());
  return true;
}

// Closes a connection that is not in m_connections and calls the disconnect
// handlers
void CNetServer::RemoveConnection(const std::shared_ptr<CTCPConnection> &connection)
{
  if (connection->GetSocket())
    SDLNet_TCP_DelSocket(static_cast<SDLNet_SocketSet>(m_socketSet), static_cast<TCPsocket>(connection->GetSocket()));
  connection->Shutdown();
  LogInfo("Network: %s disconnected.", connection->GetPeerName().c_str());

  std::lock_guard<std::recursive_mutex> lock(m_handlers->mutex);
  std::vector<DisconnectHandler> handlers;
  for (auto &h: m_handlers->disconnectHandlers)
    handlers.push_back(h.handler);
  for (auto &handler: handlers)
    handler(connection);
}

void CNetServer::Accept(void)
{
  TCPsocket socket = SDLNet_TCP_Accept(static_cast<TCPsocket>(m_listenSocket));
  if (!socket)
    return;
  std::shared_ptr<CTCPConnection> connection(new CTCPConnection(socket, false, m_helloText));
  if (!AddConnection(connection))
    connection->Shutdown();
}

std::shared_ptr<CTCPConnection> CNetServer::Connect(const std::string &host, uint16_t port)
{
  if (!m_running)
    return nullptr;
  IPaddress address;
  if (SDLNet_ResolveHost(&address, host.c_str(), port) != 0)
    return nullptr;
  TCPsocket socket = SDLNet_TCP_Open(&address);
  if (!socket)
    return nullptr;
  std::shared_ptr<CTCPConnection> connection(new CTCPConnection(socket, true, m_helloText));
  std::lock_guard<std::mutex> lock(m_pendingMutex);
  m_pending.push_back(connection);
  return connection;
}


/******************************************************************************
 Thread
******************************************************************************/

void CNetServer::ThreadFunction(void)
{
  SDLNet_SocketSet set = static_cast<SDLNet_SocketSet>(m_socketSet);
  TCPsocket listenSocket = static_cast<TCPsocket>(m_listenSocket);

  while (m_running)
  {
    std::vector<std::shared_ptr<CTCPConnection>> pending;
    {
      std::lock_guard<std::mutex> lock(m_pendingMutex);
      pending.swap(m_pending);
    }
    for (auto &connection: pending)
    {
      if (!AddConnection(connection))
        RemoveConnection(connection);
    }

    int ready = 0;
    if (m_connections.empty() && !listenSocket)
      std::this_thread::sleep_for(std::chrono::milliseconds(POLL_MILLISECONDS));
    else
    {
      ready = SDLNet_CheckSockets(set, POLL_MILLISECONDS);
      if (ready < 0)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(POLL_MILLISECONDS));
        ready = 0;
      }
    }

    if (ready > 0)
    {
      if (listenSocket && SDLNet_SocketReady(listenSocket))
        Accept();
      for (auto &connection: m_connections)
      {
        TCPsocket socket = static_cast<TCPsocket>(connection->GetSocket());
        if (!connection->CloseRequested() && SDLNet_SocketReady(socket) && !connection->Receive(*this))
          connection->Close();
      }
    }

    for (size_t i = 0; i < m_connections.size(); )
    {
      if (m_connections[i]->CloseRequested())
      {
        std::shared_ptr<CTCPConnection> connection = m_connections[i];
        m_connections.erase(m_connections.begin() + i);
        RemoveConnection(connection);
      }
      else
        ++i;
    }
  }
}


/******************************************************************************
 Start and Stop
******************************************************************************/

bool CNetServer::Start(void)
{
  if (m_thread.joinable())
    return m_listenSocket != nullptr;

  if (SDLNet_Init() != 0)
  {
    ErrorLog("Network: unable to initialize SDL_net: %s", SDLNet_GetError());
    return false;
  }
  m_netInitialized = true;
  m_socketSet = SDLNet_AllocSocketSet(int(MAX_CONNECTIONS + 1));

  IPaddress address;
  TCPsocket listenSocket = nullptr;
  if (SDLNet_ResolveHost(&address, nullptr, m_port) == 0)
    listenSocket = SDLNet_TCP_Open(&address);
  if (listenSocket)
  {
    m_listenSocket = listenSocket;
    SDLNet_TCP_AddSocket(static_cast<SDLNet_SocketSet>(m_socketSet), listenSocket);
    LogInfo("Network: listening on TCP port %u.", m_port);
  }
  else
    ErrorLog("Network: unable to listen on TCP port %u: %s", m_port, SDLNet_GetError());

  m_running = true;
  m_thread = std::thread(&CNetServer::ThreadFunction, this);
  return listenSocket != nullptr;
}

CNetServer::CNetServer(uint16_t port, const std::string &helloText)
  : m_port(port),
    m_helloText(helloText),
    m_handlers(std::make_shared<Handlers>()),
    m_running(false)
{
}

CNetServer::~CNetServer(void)
{
  m_running = false;
  if (m_thread.joinable())
    m_thread.join();

  {
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    for (auto &connection: m_pending)
      RemoveConnection(connection);
    m_pending.clear();
  }
  while (!m_connections.empty())
  {
    std::shared_ptr<CTCPConnection> connection = m_connections.back();
    m_connections.pop_back();
    RemoveConnection(connection);
  }

  if (m_socketSet)
    SDLNet_FreeSocketSet(static_cast<SDLNet_SocketSet>(m_socketSet));
  if (m_listenSocket)
    SDLNet_TCP_Close(static_cast<TCPsocket>(m_listenSocket));
  if (m_netInitialized)
    SDLNet_Quit();
}


/******************************************************************************
 Log
******************************************************************************/

void CNetServer::LogInfo(const char *fmt, ...)
{
  char text[1536];
  va_list vl;
  va_start(vl, fmt);
  vsnprintf(text, sizeof(text), fmt, vl);
  va_end(vl);
  printf("%s\n", text);
  InfoLog("%s", text);
}

std::string CNetServer::IDToText(uint32_t id)
{
  char c[4];
  bool printable = true;
  for (int i = 0; i < 4; i++)
  {
    c[i] = char((id >> (8 * i)) & 0xFF);
    printable &= uint8_t(c[i]) >= 0x20 && uint8_t(c[i]) <= 0x7E;
  }
  char text[16];
  if (printable)
    snprintf(text, sizeof(text), "'%c%c%c%c'", c[0], c[1], c[2], c[3]);
  else
    snprintf(text, sizeof(text), "0x%08X", id);
  return text;
}
