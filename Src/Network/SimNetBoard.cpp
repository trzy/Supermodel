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

#include <chrono>
#include <thread>
#include "Supermodel.h"
#include "SimNetBoard.h"

 // these make 16-bit read/writes much neater
#define RAM16 *(uint16_t*)&RAM
#define CommRAM16 *(uint16_t*)&CommRAM

static const uint64_t netGUID = 0x5bf177da34872;

inline bool CSimNetBoard::IsGame(const char* gameName)
{
	return (m_gameInfo.name == gameName) || (m_gameInfo.parent == gameName);
}

CSimNetBoard::CSimNetBoard(const Util::Config::Node& config) : m_config(config)
{
}

CSimNetBoard::~CSimNetBoard(void)
{
	m_quit = true;

	if (m_connectThread.joinable())
		m_connectThread.join();
}

void CSimNetBoard::SaveState(CBlockFile* SaveState)
{
	
}

void CSimNetBoard::LoadState(CBlockFile* SaveState)
{

}

bool CSimNetBoard::Init(uint8_t* netRAMPtr, uint8_t* netBufferPtr)
{
	RAM = netRAMPtr;
	CommRAM = netBufferPtr;

	m_attached = m_gameInfo.netboard_present && m_config["Network"].ValueAs<bool>();

	if (!m_attached)
		return 0;

	if (IsGame("daytona2") || IsGame("harley") || IsGame("scud") || IsGame("srally2") ||
		IsGame("skichamp") || IsGame("spikeout") || IsGame("spikeofe"))
		m_gameType = GameType::one;
	else if (IsGame("lemans24") || IsGame("von2") || IsGame("dirtdvls"))
		m_gameType = GameType::two;
	else
		return ErrorLog("Game not recognized or supported");

	m_state = State::start;

	//netsocks
	port_in = m_config["PortIn"].ValueAs<unsigned>();
	port_out = m_config["PortOut"].ValueAs<unsigned>();
	addr_out = m_config["AddressOut"].ValueAs<std::string>();

	nets = std::make_unique<TCPSend>(addr_out, port_out);
	netr = std::make_unique<TCPReceive>(port_in);

	return 0;
}

void CSimNetBoard::RunFrame(void)
{
	if (!IsRunning())
		return;

	switch (m_state)
	{
	case State::start:
		if (!m_connected && !m_connectThread.joinable())
			m_connectThread = std::thread(&CSimNetBoard::ConnectProc, this);
		m_status0 = 0;
		m_status1 = IsGame("dirtdvls") ? 0x4004 : 0xe000;
		m_state = State::init;
		break;

	case State::init:
		memset(CommRAM, 0, 0x10000);
		if (m_gameType == GameType::one)
		{
			if (m_status0 & 0x8000)				// has main board changed this register?
			{
				m_IRQ2ack |= 0x01;				// simulate IRQ 2 ack
				if (m_status0 == 0xf000)
				{
					// initialization complete
					m_status1 = 0;
					CommRAM16[0x72] = FLIPENDIAN16(0x1); // is this necessary?
					m_state = State::testing;
				}
				m_status0 = 0;					// 0 should work for all init subroutines
			}
		}
		else
		{
			// type 2 performs initialization on its own
			m_status1 = 0;
			m_state = State::testing;
			m_counter = 0;
		}
		break;

	case State::testing:
		if (m_gameType == GameType::one)
		{
			m_status0 += 1; // type 1 games require this to be incremented every frame

			if (!m_connected)
				break;

			uint8_t numMachines, machineIndex;

			if (RAM16[0x400] == 0)	// master
			{
				// flush receive buffer
				while (netr->CheckDataAvailable())
				{
					netr->Receive();
				}

				// check all linked instances have the same GUID
				nets->Send(&netGUID, sizeof(netGUID));
				auto& recv_data = netr->Receive();
				if (recv_data.empty())
					break;
				uint64_t testGUID;
				memcpy(&testGUID, &recv_data[0], recv_data.size());
				if (testGUID != netGUID)
					testGUID = 0;

				// send the GUID for one more loop
				nets->Send(&testGUID, sizeof(testGUID));
				netr->Receive();
				
				if (testGUID != netGUID)
				{
					ErrorLog("unable to verify connection. Make sure all machines are using same build!");
					m_state = State::error;
					break;
				}

				// master has an index of zero
				machineIndex = 0;
				nets->Send(&machineIndex, sizeof(machineIndex));

				// receive back the number of other linked machines
				recv_data = netr->Receive();
				if (recv_data.empty())
					break;
				numMachines = recv_data[0];

				// send the number of other linked machines
				nets->Send(&numMachines, sizeof(numMachines));
				netr->Receive();
			}
			else
			{
				// receive GUID from the previous machine and check it matches
				auto& recv_data = netr->Receive();
				if (recv_data.empty())
					break;
				uint64_t testGUID;
				memcpy(&testGUID, &recv_data[0], recv_data.size());
				if (testGUID != netGUID)
					testGUID = 0;
				nets->Send(&testGUID, sizeof(testGUID));

				// one more time, in case a later machine has a GUID mismatch
				recv_data = netr->Receive();
				if (recv_data.empty())
					break;
				memcpy(&testGUID, &recv_data[0], recv_data.size());
				if (testGUID != netGUID)
					testGUID = 0;
				nets->Send(&testGUID, sizeof(testGUID));

				if (testGUID != netGUID)
				{
					ErrorLog("unable to verify connection. Make sure all machines are using same build!");
					m_state = State::error;
					break;
				}

				// receive the previous machine's index, increment it, send it to the next machine
				recv_data = netr->Receive();
				if (recv_data.empty())
					break;
				machineIndex = recv_data[0] + 1;
				nets->Send(&machineIndex, sizeof(machineIndex));

				// receive the number of other linked machines and forward it on
				recv_data = netr->Receive();
				if (recv_data.empty())
					break;
				numMachines = recv_data[0];
				nets->Send(&numMachines, sizeof(numMachines));
			}

			// if there are no other linked machines, only continue if Supermodel is linked to itself
			// there might be more than one machine set to master which would cause glitches
			if ((numMachines == 0) && ((port_in != port_out) || (addr_out.compare("127.0.0.1") != 0)))
			{
				ErrorLog("no slave machines detected. Make sure only one machine is set to master!");
				m_state = State::error;
				break;
			}

			m_numMachines = numMachines + 1;

			m_status0 = 0;		// supposed to cycle between 0 and 1 (also 2 for Daytona 2); doesn't seem to matter
			m_status1 = 0x2021 + (numMachines * 0x20) + machineIndex;

			CommRAM16[0x0] = RAM16[0x400];	// 0 if master, 1 if slave
			CommRAM16[0x2] = numMachines;
			CommRAM16[0x4] = machineIndex;

			m_counter = 0;
			CommRAM16[0x6] = 0;

			m_segmentSize = RAM16[0x404];

			// don't know if these are actually required, but it never hurts to include them
			CommRAM16[0x8] = FLIPENDIAN16(0x100 + m_segmentSize);
			CommRAM16[0xa] = FLIPENDIAN16(RAM16[0x402] - m_segmentSize - 1);
			CommRAM16[0xc] = FLIPENDIAN16(0x100);
			CommRAM16[0xe] = FLIPENDIAN16(RAM16[0x402] - m_segmentSize + 0x200);

			m_state = State::ready;
		}
		else
		{
			if (!m_connected)
				break;

			// we have to track both playable and non-playable machines for type 2
			struct
			{
				uint8_t total;
				uint8_t playable;
			} numMachines, machineIndex;

			if (RAM16[0x200] == 0)	// master
			{
				// flush receive buffer
				while (netr->CheckDataAvailable())
					netr->Receive();

				// check all linked instances have the same GUID
				nets->Send(&netGUID, sizeof(netGUID));
				auto& recv_data = netr->Receive();
				if (recv_data.empty())
					break;

				uint64_t testGUID;
				memcpy(&testGUID, &recv_data[0], recv_data.size());
				if (testGUID != netGUID)
					testGUID = 0;

				// send the GUID for one more loop
				nets->Send(&testGUID, sizeof(testGUID));
				netr->Receive();

				if (testGUID != netGUID)
				{
					ErrorLog("unable to verify connection. Make sure all machines are using same build!");
					m_state = State::error;
					break;
				}

				// master has indices set to zero
				machineIndex.total = 0, machineIndex.playable = 0;
				nets->Send(&machineIndex, sizeof(machineIndex));

				// receive back the number of other linked machines
				recv_data = netr->Receive();
				if (recv_data.empty())
					break;
				memcpy(&numMachines, &recv_data[0], recv_data.size());

				// send the number of other linked machines
				nets->Send(&numMachines, sizeof(numMachines));
				netr->Receive();
			}
			else if (RAM16[0x200] < 0x8000)	// slave
			{
				// receive GUID from the previous machine and check it matches
				auto& recv_data = netr->Receive();
				if (recv_data.empty())
					break;
				uint64_t testGUID;
				memcpy(&testGUID, &recv_data[0], recv_data.size());
				if (testGUID != netGUID)
					testGUID = 0;
				nets->Send(&testGUID, sizeof(testGUID));

				// one more time, in case a later machine has a GUID mismatch
				recv_data = netr->Receive();
				if (recv_data.empty())
					break;
				memcpy(&testGUID, &recv_data[0], recv_data.size());
				if (testGUID != netGUID)
					testGUID = 0;
				nets->Send(&testGUID, sizeof(testGUID));

				if (testGUID != netGUID)
				{
					ErrorLog("unable to verify connection. Make sure all machines are using same build!");
					m_state = State::error;
					break;
				}

				// receive the indices of the previous machine and increment them
				recv_data = netr->Receive();
				if (recv_data.empty())
					break;
				memcpy(&machineIndex, &recv_data[0], recv_data.size());
				machineIndex.total++, machineIndex.playable++;

				// send our indices to the next machine
				nets->Send(&machineIndex, sizeof(machineIndex));

				// receive the number of machines
				recv_data = netr->Receive();
				if (recv_data.empty())
					break;
				memcpy(&numMachines, &recv_data[0], recv_data.size());

				// forward the number of machines
				nets->Send(&numMachines, sizeof(numMachines));
			}
			else
			{
				// relay/satellite
				
				// receive GUID from the previous machine and check it matches
				auto& recv_data = netr->Receive();
				if (recv_data.empty())
					break;
				uint64_t testGUID;
				memcpy(&testGUID, &recv_data[0], recv_data.size());
				if (testGUID != netGUID)
					testGUID = 0;
				nets->Send(&testGUID, sizeof(testGUID));

				// one more time, in case a later machine has a GUID mismatch
				recv_data = netr->Receive();
				if (recv_data.empty())
					break;
				memcpy(&testGUID, &recv_data[0], recv_data.size());
				if (testGUID != netGUID)
					testGUID = 0;
				nets->Send(&testGUID, sizeof(testGUID));

				if (testGUID != netGUID)
				{
					ErrorLog("unable to verify connection. Make sure all machines are using same build!");
					m_state = State::error;
					break;
				}

				// receive the indices of the previous machine; don't increment the playable index
				recv_data = netr->Receive();
				if (recv_data.empty())
					break;
				memcpy(&machineIndex, &recv_data[0], recv_data.size());
				machineIndex.total++;

				// send our indices to the next machine
				nets->Send(&machineIndex, sizeof(machineIndex));

				// receive the number of machines
				recv_data = netr->Receive();
				if (recv_data.empty())
					break;
				memcpy(&numMachines, &recv_data[0], recv_data.size());

				// forward the number of machines
				nets->Send(&numMachines, sizeof(numMachines));

				// indicate that this machine is a relay/satellite
				if (!IsGame("dirtdvls"))
					machineIndex.playable |= 0x80;
			}

			// if there are no other linked machines, only continue if Supermodel is linked to itself
			// there might be more than one machine set to master which would cause glitches
			if ((numMachines.total == 0) && ((port_in != port_out) || (addr_out.compare("127.0.0.1") != 0)))
			{
				ErrorLog("no slave machines detected. Make sure only one machine is set to master!");
				if (IsGame("dirtdvls"))
					m_status1 = 0x8085;	// seems like the netboard code writers really liked their CPU model numbers
				m_state = State::error;
				break;
			}

			m_numMachines = numMachines.total + 1;

			m_status0 = 5;			// probably not necessary
			if (IsGame("dirtdvls"))
				m_status1 = (numMachines.playable << 4) | machineIndex.playable | 0x7400;
			else
				m_status1 = (numMachines.playable << 8) | machineIndex.playable;

			CommRAM16[0x0] = RAM16[0x200];	// master/slave/relay status
			CommRAM16[0x2] = (numMachines.playable << 8) | numMachines.total;
			CommRAM16[0x4] = (machineIndex.playable << 8) | machineIndex.total;

			m_counter = 0;
			CommRAM16[0x6] = 0;

			m_segmentSize = RAM16[0x204];

			// don't know if these are actually required, but it never hurts to include them
			CommRAM16[0x8] = FLIPENDIAN16(0x100 + m_segmentSize);
			CommRAM16[0xa] = FLIPENDIAN16(RAM16[0x206]);
			CommRAM16[0xc] = FLIPENDIAN16(0x100);
			CommRAM16[0xe] = FLIPENDIAN16(RAM16[0x206] + 0x80);

			m_state = State::ready;
		}
		break;

	case State::ready:
		m_counter++;
		CommRAM16[0x6] = FLIPENDIAN16(m_counter);
		
		if (IsGame("spikeofe"))	// temporary hack for spikeout final edition (avoids comm error)
		{
			nets->Send(CommRAM + 0x100, m_segmentSize * m_numMachines);
			auto& recv_data = netr->Receive();
			if (recv_data.size() == 0)
			{
				// link broken - send an "empty" packet to alert other machines
				nets->Send(nullptr, 0);
				m_state = State::error;
				m_status1 = 0x40;		// send "link broken" message to mainboard
				break;
			}
			memcpy(CommRAM + 0x100 + m_segmentSize, recv_data.data(), recv_data.size());
		}
		else
		{
			// we only send what we need to; helps cut down on bandwidth
			// each machine has to receive back its own data (TODO: copy this data manually?)
			for (int i = 0; i < m_numMachines; i++)
			{
				nets->Send(CommRAM + 0x100 + i * m_segmentSize, m_segmentSize);
				auto& recv_data = netr->Receive();
				if (recv_data.size() == 0)
				{
					// link broken - send an "empty" packet to alert other machines
					nets->Send(nullptr, 0);
					m_state = State::error;
					if (m_gameType == GameType::one)
						m_status1 = 0x40;			// send "link broken" message to mainboard
					break;
				}
				memcpy(CommRAM + 0x100 + (i + 1) * m_segmentSize, recv_data.data(), recv_data.size());
			}
		}
		
		break;

	case State::error:
		// do nothing
		break;
	}
}

void CSimNetBoard::Reset(void)
{
	// if netboard was active, send an "empty" packet so the other machines don't get stuck waiting for data
	if (m_state == State::ready)
	{
		nets->Send(nullptr, 0);
		netr->Receive();
	}

	m_running = false;
	m_state = State::start;
}

bool CSimNetBoard::IsAttached(void)
{
	return m_attached;
}

bool CSimNetBoard::IsRunning(void)
{
	return m_attached && m_running;
}

void CSimNetBoard::GetGame(Game gameInfo)
{
	m_gameInfo = gameInfo;
}

void CSimNetBoard::ConnectProc(void)
{
	using namespace std::chrono_literals;

	if (m_connected)
		return;

	printf("Connecting to %s:%i ..\n", addr_out.c_str(), port_out);

	// wait until TCPSend has connected to the next machine
	while (!nets->Connect())
	{
		if (m_quit)
			return;
	}

	// wait until TCPReceive has accepted a connection from the previous machine
	while (!netr->Connected())
	{
		if (m_quit)
			return;
		std::this_thread::sleep_for(1ms);
	}

	printf("Successfully connected.\n");

	m_connected = true;
}

uint16_t CSimNetBoard::ReadIORegister(unsigned reg)
{
	if (!IsRunning())
		return 0;

	switch (reg)
	{
	case 0x00:
		return m_IRQ2ack;
	case 0x88:
		return m_status0;
	case 0x8a:
		return m_status1;
	default:
		ErrorLog("read from unknown IO register 0x%02x", reg);
		return 0;
	}
}

void CSimNetBoard::WriteIORegister(unsigned reg, uint16_t data)
{
	switch (reg)
	{
	case 0x00:
		m_IRQ2ack = data;
		break;
	case 0x88:
		m_status0 = data;
		break;
	case 0x8a:
		m_status1 = data;
		break;
	case 0xc0:
		if (data == 0)
			Reset();
		m_running = (data != 0);
		break;
	default:
		ErrorLog("write to unknown IO register 0x%02x", reg);
	}
}