/*This file is part of Output Blaster.

Output Blaster is free software : you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Output Blaster is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Output Blaster.If not, see < https://www.gnu.org/licenses/>.*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atlbase.h>
#include <iostream>
#include "NetOutputs.h"


CNetOutputs::CNetOutputs()
{
}

CNetOutputs::~CNetOutputs()
{
    if (ServerSocket) {
        Running = false;
        closesocket(ServerSocket);
        Sleep(16);
        CloseHandle(ThreadHandle);
        WSACleanup();
    }
}

bool CNetOutputs::Initialize()
{
    // Create TCP server
    if (CreateTcpServerSocket() != 0) {
        MessageBoxA(NULL, "Unable to start server thread for tcp outputs", NULL, NULL);
        return false;
    }


    // Run tcp server thread
    if (CreateServerThread() != 0) {
        MessageBoxA(NULL, "Unable to start server thread for tcp outputs", NULL, NULL);
        return false;
    }
    return true;
}

void CNetOutputs::Attached()
{
    // Broadcast a startup message
    if (SendUdpBroadcastWithId() != 0) {
        MessageBoxA(NULL, "Unable to notify game with udp broadcast", NULL, NULL);
    }
}

void CNetOutputs::SendOutput(EOutputs output, UINT8 prevValue, UINT8 value)
{
    // Loop through all registered clients and send them new output value
    LPARAM param = (LPARAM)output + 1;
    for (vector<RegisteredClientTcp>::iterator it = m_clients.begin(), end = m_clients.end(); it != end; it++) {
        auto csock = it->ClientSocket;

        std::string name = GetOutputName(output);
        std::string strvalue = std::to_string(value);
        std::string line = name + SeparatorIdAndValue + strvalue + FrameEnding;
        send(csock, line.c_str(), line.length(), 0);
    }

    // Send a broadcast every 10s to show we are here?
    // Will do SendUdpBroadcastWithId();
}

LRESULT CNetOutputs::CreateServerThread()
{
    if (s_createdClass)
        return 0;

    // Setup description of window class
    // Allocate memory for thread data.
    Running = true;
    ThreadHandle = CreateThread(
        NULL,                   // default security attributes
        0,                      // use default stack size  
        ThreadEntry,        // thread function name
        this,		            // argument to thread function 
        0,                      // use default creation flags 
        &ThreadId);   // returns the thread identifier 
    if (ThreadHandle == NULL)
        return 1;
    s_createdClass = true;
    return 0;
}


DWORD CNetOutputs::TcpServerThread()
{
    SOCKADDR_IN csin;
    while (Running) {
        SOCKET csock;
        int sinsize = sizeof(csin);
        if ((csock = accept(ServerSocket, (SOCKADDR*)&csin, &sinsize)) != INVALID_SOCKET) {
            RegisterClient(csock);
        }
        Sleep(16);
    }
    return 0;
}

LRESULT CNetOutputs::CreateTcpServerSocket()
{
    /* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
    WORD wVersionRequested = MAKEWORD(2, 2);
    int err = WSAStartup(wVersionRequested, &this->WSAData);
    if (err != 0) {
        /* Tell the user that we could not find a usable */
        /* Winsock DLL.                                  */
        printf("WSAStartup failed with error: %d\n", err);
        perror("WASStartup");
        return err;
    }
    ServerSocket = socket(AF_INET, SOCK_STREAM, 0);
    server_sin.sin_addr.s_addr = INADDR_ANY;
    server_sin.sin_family = AF_INET;
    server_sin.sin_port = htons(TcpPort);
    if ((err = bind(ServerSocket, (SOCKADDR*)&server_sin, sizeof(server_sin))) != 0) {
        perror("bind");
        closesocket(ServerSocket);
        return err;
    }
    if ((err = listen(ServerSocket, 0)) != 0) {
        perror("listen");
        closesocket(ServerSocket);
        return err;
    }
    return 0;
}
LRESULT CNetOutputs::SendUdpBroadcastWithId()
{
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    char broadcast = '1';
    int err;
    if ((err = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast))) != 0) {
        perror("broadcast options");
        closesocket(sock);
        return err;
    }

    /*
    // UDP server
    SOCKADDR_IN Recv_addr;
    Recv_addr.sin_family = AF_INET;
    Recv_addr.sin_port = htons(UdpBroadcastPort);
    Recv_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (sockaddr*)&Recv_addr, sizeof(Recv_addr)) < 0) {
        perror("bind");
        closesocket(sock);
        return 2;
    }*/

    SOCKADDR_IN Sender_addr;
    Sender_addr.sin_family = AF_INET;
    Sender_addr.sin_port = htons(UdpBroadcastPort);
    Sender_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    std::string lines = "mame_start" + SeparatorIdAndValue + GetGame().name + FrameEnding + "tcp" + SeparatorIdAndValue + std::to_string(TcpPort) + FrameEnding;
    if (sendto(sock, lines.c_str(), lines.length(), 0, (sockaddr*)&Sender_addr, sizeof(Sender_addr)) != lines.length()) {
        perror("broadcast send");
        closesocket(sock);
        return 1;
    }

    closesocket(sock);
    return 0;
}



bool CNetOutputs::AllocateMessageId(UINT& regId, LPCSTR str)
{
    USES_CONVERSION;
    LPCSTR a = str;
    LPCWSTR w = A2W(a);
    std::wcout << w << std::endl;
    return false;
}



LRESULT CNetOutputs::RegisterClient(SOCKET socket)
{
    // Check that given client is not already registered
    for (vector<RegisteredClientTcp>::iterator it = m_clients.begin(), end = m_clients.end(); it != end; it++) {
        if (it->ClientSocket = socket) {
            // If so, just send it current state of all outputs
            SendAllToClient(*it);
            return 1;
        }
    }

    // If not, store details about client and send it current state of all outputs
    RegisteredClientTcp client;
    client.ClientSocket = socket;
    m_clients.push_back(client);
    SendGameIdString(client);
    SendAllToClient(client);

    return 0;
}

void CNetOutputs::SendAllToClient(RegisteredClientTcp& client)
{
    // Loop through all known outputs and send their current state to given client
    for (unsigned i = 0; i < NUM_OUTPUTS; i++) {
        EOutputs output = (EOutputs)i;
        if (HasValue(output)) {
            std::string name = GetOutputName(output);
            std::string strvalue = std::to_string(GetValue(output));
            std::string line = name + SeparatorIdAndValue + strvalue + FrameEnding;
            send(client.ClientSocket, line.c_str(), line.length(), 0);
        }
    }
}

LRESULT CNetOutputs::UnregisterClient(SOCKET socket)
{
    // Find any matching clients and remove them
    bool found = false;
    vector<RegisteredClientTcp>::iterator it = m_clients.begin();
    while (it != m_clients.end()) {
        if (it->ClientSocket == socket) {
            SendStopString(*it, true);
            it = m_clients.erase(it);
            found = true;
            closesocket(it->ClientSocket);
        } else {
            it++;
        }
    }

    // Return error if no matches found
    return (found ? 0 : 1);
    return 0;
}

LRESULT CNetOutputs::SendGameIdString(RegisteredClientTcp& client)
{
    // Id 0 is the name of the game
    std::string line = "mame_start" + SeparatorIdAndValue + GetGame().name + FrameEnding;
    return send(client.ClientSocket, line.c_str(), line.length(), 0);
}
LRESULT CNetOutputs::SendStopString(RegisteredClientTcp& client, bool stopped)
{
    // Id 0 is the name of the game
    std::string line = "mame_stop" + SeparatorIdAndValue + std::string((stopped ? "1" : "0")) + FrameEnding;
    return send(client.ClientSocket, line.c_str(), line.length(), 0);
}
LRESULT CNetOutputs::SendPauseString(RegisteredClientTcp& client, bool paused)
{
    // Id 0 is the name of the game
    std::string line = "pause" + SeparatorIdAndValue + std::string((paused ? "1" : "0")) + FrameEnding;
    return send(client.ClientSocket, line.c_str(), line.length(), 0);
}
LRESULT CNetOutputs::SendValueIdString(RegisteredClientTcp& client, EOutputs output)
{
    // Id 0 is the name of the game
    std::string  name = GetOutputName(output);

    std::string line = "id:" + name + FrameEnding;
    return send(client.ClientSocket, line.c_str(), line.length(), 0);
}

