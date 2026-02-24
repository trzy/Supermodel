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

#include "NetOutputs.h"


CNetOutputs::CNetOutputs()
{
}

CNetOutputs::~CNetOutputs()
{
    // Unregister all clients and destroy the TCP server socket.
    UnregisterAllClients();
    DestroyTcpServerSocket();

    // Stop the server thread.
    m_running.store(false);
    if (m_tcpServerThread.joinable()) {
        m_tcpServerThread.join();
    }

    // Free up resources.
    if (m_tcpSocketSet) {
        SDLNet_FreeSocketSet(m_tcpSocketSet);
    }
    if (m_serverSocket) {
        SDLNet_TCP_Close(m_serverSocket);
    }

    SDLNet_Quit();
}

bool CNetOutputs::Initialize()
{
    // Create the TCP server socket.
    if (!CreateTcpServerSocket()) {
        ErrorLog("Unable to create TCP server socket.");
        return false;
    }

    // Run TCP server thread.
    if (!CreateServerThread()) {
        ErrorLog("Unable to start thread for TCP server.");
        return false;
    }

    return true;
}

void CNetOutputs::Attached()
{
    // Broadcast a startup message.
    if (!SendUdpBroadcastWithId()) {
        ErrorLog("Unable to notify with udp broadcast.");
    }
}

void CNetOutputs::SendOutput(EOutputs output, UINT8 prevValue, UINT8 value)
{
    // Loop through all registered clients and send them new output values.
    for (auto& client : m_clients) {
        std::string name = GetOutputName(output);
        std::string strvalue = std::to_string(value);
        std::string line = name + m_separatorIdAndValue + strvalue + m_frameEnding;
        if (!SendTcpData(client, line)) {
            ErrorLog("Failed to send output data to client. Unregistering client.");
            UnregisterClient(client.ClientSocket);
        }
    }
}

bool CNetOutputs::CreateServerThread()
{
    m_running.store(true);
    m_tcpServerThread = std::thread([this] {
        while (m_running.load()) {
            TCPsocket clientSocket = SDLNet_TCP_Accept(m_serverSocket);
            if (clientSocket) {
                // Handle new client connection (e.g., add to list of clients, send initial state, etc.).
                IPaddress* clientIP = SDLNet_TCP_GetPeerAddress(clientSocket);
                if (clientIP) {
                    const char* host = SDLNet_ResolveIP(clientIP);
                    InfoLog("Client connected from %s:%d", host, clientIP->port);
                } else {
                    ErrorLog("Failed to get client IP address: %s", SDLNet_GetError());
                }
                RegisterClient(clientSocket);
            }
            // Sleep to prevent busy waiting.
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    });
    if (!m_tcpServerThread.joinable()) {
        ErrorLog("Failed to create thread for TCP server.");
        return false;
    }

    return true;

}

bool CNetOutputs::CreateTcpServerSocket()
{
    SDLNet_Init();

	// Resolve the host (NULL means any host) and port for the server to listen on.
    IPaddress ip;
    if (SDLNet_ResolveHost(&ip, NULL, m_tcpPort) == -1) {
        ErrorLog("ResolveHost error: %s", SDLNet_GetError());
        return false;
    }

    // Create the TCP socket and bind it to the specified port.
    m_serverSocket = SDLNet_TCP_Open(&ip);
    if (!m_serverSocket) {
        ErrorLog("TCP_Open error: %s", SDLNet_GetError());
        return false;
    }

    // Create the socket set to manage multiple clients.
    // It is +1 to include the server socket itself.
    m_tcpSocketSet = SDLNet_AllocSocketSet(m_maxClients + 1);
    SDLNet_TCP_AddSocket(m_tcpSocketSet, m_serverSocket);

    return true;

}

void CNetOutputs::DestroyTcpServerSocket()
{
    if (m_serverSocket) {
        SDLNet_TCP_Close(m_serverSocket);
        m_serverSocket = nullptr;
    }
}

bool CNetOutputs::SendUdpBroadcastWithId()
{
    // Open a UDP socket on any available port.
    UDPsocket udpSocket = SDLNet_UDP_Open(0);
    if (!udpSocket) {
        ErrorLog("UDP_Open error: %s", SDLNet_GetError());
        return false;
    }

    // Set up the broadcast address.
    IPaddress broadcastAddr;
    if (SDLNet_ResolveHost(&broadcastAddr, "255.255.255.255", m_udpBroadcastPort) == -1) {
        ErrorLog("ResolveHost error: %s", SDLNet_GetError());
        return false;
    }

    // Build the broadcast packet.
    UDPpacket* packet = SDLNet_AllocPacket(512);
    std::string lines = "mame_start" + m_separatorIdAndValue + GetGame().name + m_frameEnding + "tcp" + m_separatorIdAndValue + std::to_string(m_tcpPort) + m_frameEnding;

    memcpy(packet->data, lines.c_str(), lines.length());
    packet->len = lines.length();
    packet->address = broadcastAddr;

    // Send the broadcast packet.
    if (SDLNet_UDP_Send(udpSocket, -1, packet) == 0) {
        ErrorLog("UDP_Send error: %s", SDLNet_GetError());
    } else {
        DebugLog("Broadcast sent!");
    }

    // Clean up.
    SDLNet_FreePacket(packet);
    SDLNet_UDP_Close(udpSocket);
    return true;
}

bool CNetOutputs::RegisterClient(TCPsocket socket)
{
    // Check that given client is not already registered.
    for (auto& client : m_clients) {
        if (client.ClientSocket == socket) {
            // If so, just send it current state of all outputs.
            SendAllDataToClient(client);
            return false;
        }
    }

    // Add the new client socket to the socket set.
    SDLNet_TCP_AddSocket(m_tcpSocketSet, socket);

    // If not, store details about client and send it current state of all outputs.
    RegisteredClientTcp client;
    client.ClientSocket = socket;
    m_clients.push_back(client);

    // Send the data to the new client.
    if (!SendGameIdString(client)) {
        ErrorLog("Failed to send game id string to client. Unregistering client.");
        UnregisterClient(socket);
        return false;
    }
    SendAllDataToClient(client);

    return true;
}

void CNetOutputs::SendAllDataToClient(RegisteredClientTcp& client)
{
    // Loop through all known outputs and send their current state to given client.
    for (unsigned i = 0; i < NUM_OUTPUTS; i++) {
        EOutputs output = (EOutputs)i;
        if (HasValue(output)) {
            std::string name = GetOutputName(output);
            std::string strvalue = std::to_string(GetValue(output));
            std::string line = name + m_separatorIdAndValue + strvalue + m_frameEnding;
            if (!SendTcpData(client, line)) {
                ErrorLog("Failed to send all output data to client. Unregistering client.");
                UnregisterClient(client.ClientSocket);
                break;
            }
        }
    }
}

bool CNetOutputs::UnregisterAllClients()
{
    for (auto& client : m_clients) {
        UnregisterClient(client.ClientSocket);
    }
    m_clients.clear();
    return true;
}

bool CNetOutputs::UnregisterClient(TCPsocket socket)
{
    // Find any matching clients and remove them.
    bool found = false;
    std::vector<RegisteredClientTcp>::iterator it = m_clients.begin();
    while (it != m_clients.end()) {
        if (it->ClientSocket == socket) {
            // Client found, send stop message and remove it.
            SendStopString(*it, true);
            it = m_clients.erase(it);
            found = true;
            // Remove from socket set and close the socket.
            SDLNet_TCP_DelSocket(m_tcpSocketSet, socket);
            SDLNet_TCP_Close(socket);
        } else {
            it++;
        }
    }

    // Return error if no matches found.
    return found;
}

bool CNetOutputs::SendGameIdString(RegisteredClientTcp& client)
{
    std::string line = "mame_start" + m_separatorIdAndValue + GetGame().name + m_frameEnding;
    return SendTcpData(client, line);
}

bool CNetOutputs::SendStopString(RegisteredClientTcp& client, bool stopped)
{
    std::string line = "mame_stop" + m_separatorIdAndValue + std::string((stopped ? "1" : "0")) + m_frameEnding;
    return SendTcpData(client, line);
}

bool CNetOutputs::SendPauseString(RegisteredClientTcp& client, bool paused)
{
    std::string line = "pause" + m_separatorIdAndValue + std::string((paused ? "1" : "0")) + m_frameEnding;
    return SendTcpData(client, line);
}

bool CNetOutputs::SendTcpData(RegisteredClientTcp& client, const std::string& data)
{
    auto sentDataLen = SDLNet_TCP_Send(client.ClientSocket, data.c_str(), data.length());
    DebugLog("Sent to client: %s", data.c_str());
    DebugLog("Bytes sent: %d", sentDataLen);
    DebugLog("Expected bytes: %zu", data.length());
    // Check for errors in sending data.
    if (sentDataLen < static_cast<int>(data.length())) {
        ErrorLog("Failed to send TCP data to client: %s (tried to send %zu bytes, sent %d bytes).",
            SDLNet_GetError(),
            data.length(),
            sentDataLen
        );
        return false;
    }
    return true;
}
