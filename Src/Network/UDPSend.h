#ifndef _UDPSEND_H_
#define _UDPSEND_H_

#include "UDPPacket.h"
#include "WinSockWrap.h"

namespace SMUDP
{
	class UDPSend
	{
	public:
		UDPSend();
		~UDPSend();

		bool Send(const char* address, int port, int length, const void *data, int timeout);

	private:

		bool WaitForEvent(SOCKET s, HANDLE hEvent, long nEvents, int timeout);
		bool ProcessReply(SOCKET s, HANDLE event, int timeout);
		int SendDataPacket(Packet& p, SOCKET s, SOCKADDR_IN& address, HANDLE events);
		
		SOCKET m_socket;
		HANDLE m_event;
		WinSockWrap m_winsockWrap;
	};
}

#endif