#include <winsock2.h>
#include <windows.h>
#include "UDPReceive.h"
#include "UDPPacket.h"

namespace SMUDP
{

UDPReceive::UDPReceive()
{
	m_socket	= socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);		// create the socket
	m_readEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	WSAEventSelect(m_socket, m_readEvent, FD_READ);
}

UDPReceive::~UDPReceive()
{
	closesocket(m_socket);
	CloseHandle(m_readEvent);
}

bool UDPReceive::Bind(UINT16 port)
{
	//===========================
	int			err;
	SOCKADDR_IN serverInfo = {};
	//===========================

	serverInfo.sin_family		= AF_INET;			// address family Internet
	serverInfo.sin_port			= htons(port);		// set server’s port number
	serverInfo.sin_addr.s_addr	= INADDR_ANY;		// set server’s IP

	err = bind(m_socket, (LPSOCKADDR)&serverInfo, sizeof(struct sockaddr));

	return (err == 0);
}

std::vector<UINT8>& UDPReceive::ReadData(int timeout)
{
	m_data.clear();

	while (true) {

		//========
		DWORD res;
		//========

		res = WaitForSingleObject(m_readEvent, timeout);

		if (res == WAIT_OBJECT_0) {

			//=========================
			int				result;
			int				slen;
			sockaddr_in		si_other;
			Packet			packet;
			//=========================

			slen = sizeof(sockaddr_in);

			result = recvfrom(m_socket, packet, sizeof(packet), 0, (struct sockaddr *) &si_other, &slen);

			if (result == SOCKET_ERROR) {
				auto error = WSAGetLastError();	// clear error code
				m_data.clear();			
				break;	
			}

			// copy data to array
			m_data.insert(m_data.end(), packet.data, packet.data + packet.length);

			PacketReply r = 1;
			result = sendto(m_socket, &r, sizeof(r), 0, (struct sockaddr *)&si_other, sizeof(SOCKADDR_IN));

			if (packet.currentID + 1 == packet.totalIDs) {
				break;			// reached the end
			}
		}
		else {
			m_data.clear();		// reset any memory because we have failed
			break;				// timeout
		}
	}

	return m_data;
}

}
