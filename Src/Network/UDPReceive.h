#ifndef _UDP_RECEIVE_H_
#define _UDP_RECEIVE_H_

#include "UDPPacket.h"
#include "WinSockWrap.h"
#include <vector>

namespace SMUDP
{
	class UDPReceive
	{
	public:
		UDPReceive();
		~UDPReceive();

		bool Bind(UINT16 port);

		std::vector<UINT8>& ReadData(int timeout);

	private:

		std::vector<UINT8> m_data;
		SOCKET		m_socket;
		HANDLE		m_readEvent;
		WinSockWrap	m_winSockWrap;
	};
}

#endif