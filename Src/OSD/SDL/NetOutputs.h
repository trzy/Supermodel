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
  * NetOutputs.h
  *
  * Implementation of COutputs that sends MAMEHooker compatible messages via TCP and UDP.
  */

#pragma once

#include "Logger.h"
#include "Outputs.h"
#include "SDLIncludes.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

// Default values for configurable settings.
const unsigned int NET_OUTPUTS_DEFAULT_TCP_PORT = 8000;
const unsigned int NET_OUTPUTS_DEFAULT_UDP_BROADCAST_PORT = 8001;
const std::string NET_OUTPUTS_DEFAULT_FRAME_ENDING = std::string("\r");
const std::string NET_OUTPUTS_DEFAULT_SEPARATOR_ID_AND_VALUE = std::string(" = ");

// Maximum number of clients that can be registered with the emulator at once.
// This was chosen somewhat arbitrarily, but seems reasonable given the use
// case of MAMEHooker and similar tools.
const unsigned int NET_OUTPUTS_MAX_CLIENTS = 10;

// Struct that represents a client (eg MAMEHooker) currently registered with the emulator.
struct RegisteredClientTcp
{
	TCPsocket ClientSocket;
};


class CNetOutputs : public COutputs
{
public:
	/*
	 * CNetOutputs():
	 * ~CNetOutputs():
	 *
	 * Constructor and destructor.
	 */
	CNetOutputs();
	virtual ~CNetOutputs();

	/*
	 * Initialize():
	 *
	 * Initializes this class.
	 */
	bool Initialize();

	/*
	 * Attached():
	 *
	 * Lets the class know that it has been attached to the emulator.
	 */
	void Attached();

	void SetFrameEnding(const std::string& ending) { m_frameEnding = ending; }
	void SetTcpPort(unsigned int port) { m_tcpPort = port; }
	void SetUdpBroadcastPort(unsigned int port) { m_udpBroadcastPort = port; }
protected:
	/*
	 * SendOutput():
	 *
	 * Sends the appropriate output message to all registered clients.
	 */
	void SendOutput(EOutputs output, UINT8 prevValue, UINT8 value);

private:
	// Configurable values.
	unsigned int m_tcpPort = NET_OUTPUTS_DEFAULT_TCP_PORT;
	unsigned int m_udpBroadcastPort = NET_OUTPUTS_DEFAULT_UDP_BROADCAST_PORT;
	 // Defaults to "\r", can also be "\r\n", see configuration.
	std::string m_frameEnding = NET_OUTPUTS_DEFAULT_FRAME_ENDING;

	// Internal data.
	std::string m_separatorIdAndValue = NET_OUTPUTS_DEFAULT_SEPARATOR_ID_AND_VALUE;
	unsigned int m_maxClients = NET_OUTPUTS_MAX_CLIENTS;
	std::atomic<bool> m_running = false;
	std::vector<RegisteredClientTcp> m_clients{};
	TCPsocket m_serverSocket;
    SDLNet_SocketSet m_tcpSocketSet;
	std::thread m_tcpServerThread;

	/*
	 * CreateServerThread():
	 *
	 * Wait for new client and handle it appropriately.
	 */
	bool CreateServerThread();

	/*
	 * CreateTcpServerSocket():
	 *
	 * Creates the TCP server socket.
	 */
	bool CreateTcpServerSocket();

	/*
	 * DestroyTcpServerSocket():
	 *
	 * Destroys the TCP server socket.
	 */
	void DestroyTcpServerSocket();

	/*
	 * SendUdpBroadcastWithId():
	 *
	 * Broadcasts a message over UDP to show that the emulator is running and to provide
	 * the TCP port number.
	 */
	bool SendUdpBroadcastWithId();

	/*
	 * RegisterClient(hwnd, id):
	 *
	 * Registers a client (eg MAMEHooker) with the emulator.
	 */
	bool RegisterClient(TCPsocket socket);

	/*
	 * SendAllToClient(client):
	 *
	 * Sends the current state of all the outputs to the given registered client.
	 * Called whenever a client is registered with the emulator.
	 */
	void SendAllDataToClient(RegisteredClientTcp& client);

	/*
	 * UnregisterClient(hwnd, id):
	 *
	 * Unregisters a client from the emulator.
	 */
	bool UnregisterClient(TCPsocket socket);

	/*
	 * UnregisterAllClients():
	 *
	 * Unregisters all clients from the emulator.
	*/
	bool UnregisterAllClients();

	/*
	 * SendGameIdString(hwnd, id):
	 *
	 * Sends the name of the current running game.
	 */
	bool SendGameIdString(RegisteredClientTcp& client);

	/*
	 * SendStopString(hwnd, id):
	 *
	 * Sends the mame stops message.
	 */
	bool SendStopString(RegisteredClientTcp& client, bool stopped);

	/*
	* SendPauseString(hwnd, id):
	 *
	 * Sends the name of the current running game.
	 */
	bool SendPauseString(RegisteredClientTcp& client, bool paused);

	/*
	 * SendTcpData(client, data):
	 *
	 * Sends the given data string to the given client over TCP.
	 */
	bool SendTcpData(RegisteredClientTcp& client, const std::string& data);
};

