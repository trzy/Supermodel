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

#ifndef INCLUDED_NETBOARD_H
#define INCLUDED_NETBOARD_H

#include "Types.h"
#include "CPU/Bus.h"
#include "CPU/68K/68K.h"
#include "OSD/Thread.h"
#include <memory>
#include "INetBoard.h"
#include "TCPSend.h"
#include "TCPSendAsync.h"
#include "TCPReceive.h"

//#define NET_BUF_SIZE 32800 // 16384 not enough

class CNetBoard : public IBus, public INetBoard
{
public:

	UINT8 Read8(UINT32 addr);
	UINT16 Read16(UINT32 addr);
	UINT32 Read32(UINT32 addr);

	void Write8(UINT32 addr, UINT8 data);
	void Write16(UINT32 addr, UINT16 data);
	void Write32(UINT32 addr, UINT32 data);

	void SaveState(CBlockFile *SaveState);
	void LoadState(CBlockFile *SaveState);

	void RunFrame(void);
	void Reset(void);

	// Returns a reference to the 68K CPU context
	M68KCtx *GetM68K(void);
	bool IsAttached(void);
	bool IsRunning(void);

	bool Init(UINT8 *netRAMPtr, UINT8 *netBufferPtr);

	void GetGame(Game);

	UINT8 ReadCommRAM8(unsigned addr);
	UINT16 ReadCommRAM16(unsigned addr);
	UINT32 ReadCommRAM32(unsigned addr);

	void WriteCommRAM8(unsigned addr, UINT8 data);
	void WriteCommRAM16(unsigned addr, UINT16 data);
	void WriteCommRAM32(unsigned addr, UINT32 data);

	UINT16 ReadIORegister(unsigned reg);
	void WriteIORegister(unsigned reg, UINT16 data);

	CNetBoard(const Util::Config::Node &config);
	~CNetBoard(void);

private:
	// Config
	const Util::Config::Node &m_config;
	// 68K CPU
	M68KCtx		M68K;

	// Sound board memory
	UINT8		*netRAM;		// 64Kb RAM (passed in from parent object)
	UINT8		*netBuffer;		// 128kb (passed in from parent object)
	UINT8		*memoryPool;	// single allocated region for all net board RAM
	UINT8		*CommRAM;
	UINT8		*ioreg;
	UINT8		*ctrlrw;
	UINT8		*Buffer;
	UINT8		*RAM;
	UINT8		*ct;

	bool		m_attached;		// True if net board is attached
	UINT16		commbank;
	UINT16		recv_offset;
	UINT16		recv_size;
	UINT16		send_offset;
	UINT16		send_size;
	UINT8		slot;

	// netsock
	UINT16 port_in = 0;
	UINT16 port_out = 0;
	std::string addr_out = "";

	std::unique_ptr<TCPSend> nets;
	std::unique_ptr<TCPReceive> netr;

	//game info
	Game Gameinfo;

	// only for some tests
	UINT8 *bank;
	UINT8 *bank2;
	UINT8 test_irq;

	std::thread interrupt5;
	bool int5;
};

void Net_SetCB(int(*Run68k)(int cycles), void(*Int68k)(int irq));

#endif	// INCLUDED_NETBOARD_H


