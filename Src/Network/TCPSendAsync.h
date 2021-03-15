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

#ifndef _TCPSENDASYNC_H_
#define _TCPSENDASYNC_H_

#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <thread>
#include "SDLIncludes.h"

class TCPSendAsync
{
public:
	TCPSendAsync(std::string& ip, int port);
	~TCPSendAsync();

	bool Send(const void* data, int length);
	bool Connect();
	bool Connected();
private:

	void SendThread();

	std::string				m_ip;
	int						m_port;
	TCPsocket				m_socket;		// sdl socket
	std::atomic<bool>		m_hasData;
	std::condition_variable m_cv;
	std::vector<char>		m_buffer;
	std::mutex				m_mutex;
	std::thread				m_sendThread;

	std::vector<std::unique_ptr<char[]>> m_dataBuffers;	// each pointer is to an array of data. First word is the size of the data

};

#endif
