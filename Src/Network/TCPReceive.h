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

#ifndef _TCPRECEIVE_H_
#define _TCPRECEIVE_H_

#include <thread>
#include <atomic>
#include <vector>
#include "SDLIncludes.h"

class TCPReceive
{
public:
	TCPReceive(int port);
	~TCPReceive();

	std::vector<char>& Receive();

private:

	void ListenFunc();

	TCPsocket m_listenSocket;
	std::atomic<TCPsocket> m_receiveSocket;
	std::thread m_listenThread;
	std::atomic_bool m_running;
	std::vector<char> m_recBuffer;
};

#endif