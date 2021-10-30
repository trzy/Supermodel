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

#ifndef INCLUDED_SIMNETBOARD_H
#define INCLUDED_SIMNETBOARD_H

#include <cstdint>
#include "TCPSend.h"
#include "TCPReceive.h"
#include "INetBoard.h"

enum class State
{
	start,
	init,
	testing,
	ready,
	error
};

enum class GameType
{
	unknown,
	one,		// all games except those listed below
	two			// Le Mans 24, Virtual-On, Dirt Devils
};

class CSimNetBoard : public INetBoard
{
public:
	CSimNetBoard(const Util::Config::Node& config);
	~CSimNetBoard(void);

	void SaveState(CBlockFile* SaveState);
	void LoadState(CBlockFile* SaveState);

	bool Init(uint8_t* netRAMPtr, uint8_t* netBufferPtr);
	void RunFrame(void);
	void Reset(void);

	bool IsAttached(void);
	bool IsRunning(void);

	void GetGame(Game gameInfo);

	uint16_t ReadIORegister(unsigned reg);
	void WriteIORegister(unsigned reg, uint16_t data);

private:
	// Config
	const Util::Config::Node& m_config;

	uint8_t* RAM = nullptr;
	uint8_t* CommRAM = nullptr;

	// netsock
	uint16_t port_in = 0;
	uint16_t port_out = 0;
	std::string addr_out = "";
	std::thread m_connectThread;
	std::atomic_bool m_quit = false;
	std::atomic_bool m_connected = false;

	std::unique_ptr<TCPSend> nets = nullptr;
	std::unique_ptr<TCPReceive> netr = nullptr;

	Game m_gameInfo;
	GameType m_gameType = GameType::unknown;
	State m_state = State::start;

	uint8_t m_numMachines = 0;
	uint16_t m_counter = 0;

	uint16_t m_segmentSize = 0;

	bool m_attached = false;
	bool m_running = false;

	uint16_t m_IRQ2ack = 0;
	uint16_t m_status0 = 0;	// ioreg 0x88
	uint16_t m_status1 = 0;	// ioreg 0x8a

	inline bool IsGame(const char* gameName);
	void ConnectProc(void);
};

#endif