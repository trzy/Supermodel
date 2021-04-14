/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011-2020 Bart Trzynadlowski, Nik Henson, Ian Curtis,
 **                     Harry Tuttle, and Spindizzi
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

#include "TCPSend.h"
#include "OSD/Logger.h"

#if defined(_DEBUG)
#include <stdio.h>
#define DPRINTF DebugLog
#else
#define DPRINTF(a, ...)
#endif

static const int RETRY_COUNT = 10;			// shrugs

TCPSend::TCPSend(std::string& ip, int port) :
	m_ip(ip),
	m_port(port),
	m_socket(nullptr)
{
	SDLNet_Init();
}

TCPSend::~TCPSend()
{
	if (m_socket) {
		SDLNet_TCP_Close(m_socket);
		m_socket = nullptr;
	}

	SDLNet_Quit();	// unload lib (winsock dll for windows)
}

bool TCPSend::Send(const void * data, int length)
{
	// If we failed bail out
	if (!Connected()) {
		DPRINTF("Not connected\n");
		return false;
	}

	DPRINTF("Sending %i bytes\n", length);

	int sent = 0;

	sent = SDLNet_TCP_Send(m_socket, &length, sizeof(int));		// pack the length at the start of transmission.

	if (!length)
		return true;		// 0 sized packet will blow our connex

	sent = SDLNet_TCP_Send(m_socket, data, length);

	if (sent < length) {
		SDLNet_TCP_Close(m_socket);
		m_socket = nullptr;
	}

	return true;
}

bool TCPSend::Connected()
{
	return m_socket != 0;
}

bool TCPSend::Connect()
{
	IPaddress ip;
	int result = SDLNet_ResolveHost(&ip, m_ip.c_str(), m_port);

	if (result == 0) {
		m_socket = SDLNet_TCP_Open(&ip);
	}

	return Connected();
}
