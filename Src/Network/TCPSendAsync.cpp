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

#include "TCPSendAsync.h"
#include "OSD/Logger.h"
#include <utility>

#if defined(_DEBUG)
#include <stdio.h>
#define DPRINTF DebugLog
#else
#define DPRINTF(a, ...)
#endif

static const int RETRY_COUNT = 10;			// shrugs

TCPSendAsync::TCPSendAsync(std::string& ip, int port) :
	m_ip(ip),
	m_port(port),
	m_socket(nullptr),
	m_hasData(false)
{
	SDLNet_Init();

	m_sendThread = std::thread(&TCPSendAsync::SendThread, this);
}

TCPSendAsync::~TCPSendAsync()
{
	if (m_socket) {
		SDLNet_TCP_Close(m_socket);
		m_socket = nullptr;
	}

	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_dataBuffers.clear();
		m_hasData = true;		// must set data ready in case of spurious wake up 
		m_cv.notify_one();		// tell locked thread it can wake up
	}

	if (m_sendThread.joinable()) {
		m_sendThread.join();
	}

	SDLNet_Quit();	// unload lib (winsock dll for windows)
}

bool TCPSendAsync::Send(const void * data, int length)
{
	// If we failed bail out
	if (!Connected()) {
		DPRINTF("Not connected\n");
		return false;
	}

	DPRINTF("Sending %i bytes\n", length);

	if (!length) {
		return true;		// 0 sized packet will blow our connex
	}

	auto dataBuffer = std::unique_ptr<char[]>(new char[length+4]);

	*((int32_t*)dataBuffer.get()) = length;				// set start of buffer to length
	memcpy(dataBuffer.get() + 4, data, length);		// copy the rest of the data

	// lock our array and signal to other thread data is ready
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_dataBuffers.emplace_back(std::move(dataBuffer));
		
		m_hasData = true;	// must set data ready in case of spurious wake up 
		m_cv.notify_one();	// tell locked thread it can wake up
	}

	return true;
}

bool TCPSendAsync::Connected()
{
	return m_socket != 0;
}

void TCPSendAsync::SendThread()
{
	while (true) {

		std::unique_ptr<char[]> sendData;

		{
			std::unique_lock<std::mutex> lock(m_mutex);
			m_cv.wait(lock, [this] { return m_hasData.load(); });

			if (m_dataBuffers.empty()) {
				return;					// if we have woken up with no data assume we need to exit the thread
			}

			auto front = m_dataBuffers.begin();
			sendData = std::move(*front);
			m_dataBuffers.erase(front);
			m_hasData = false;		// potentially we could still have data in pipe, we'll set this at the bottom

			// unlock mutex now so we don't block whilst sending
		}

		if (sendData == nullptr) {
			break;		// shouldn't be able to get here
		}

		// get send size (which is packed at the start of the data
		auto sendSize = *((int32_t*)sendData.get()) + 4;		// send size doesn't include 'header'

		int sent = SDLNet_TCP_Send(m_socket, sendData.get(), sendSize);		// pack the length at the start of transmission.
		if (sent < sendSize) {
			SDLNet_TCP_Close(m_socket);
			m_socket = nullptr;
			break;
		}

		// we have finished with this buffer so release the data
		sendData = nullptr;

		//check if we still have data in the pipe, if so set ready state again
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			if (m_dataBuffers.size()) {
				m_hasData = true;
				m_cv.notify_one();
			}
		}
	}
}

bool TCPSendAsync::Connect()
{
	IPaddress ip;
	int result = SDLNet_ResolveHost(&ip, m_ip.c_str(), m_port);

	if (result == 0) {
		m_socket = SDLNet_TCP_Open(&ip);
	}

	return Connected();
}
