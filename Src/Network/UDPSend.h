#ifndef _UDPSEND_H_
#define _UDPSEND_H_

#include "UDPPacket.h"
#include "WinSockWrap.h"
#include <vector>
#include <thread>

namespace SMUDP
{
	class UDPSend
	{
	public:
		UDPSend();
		~UDPSend();

		bool Send(const char* address, int port, int length, const void *data, int timeout);		// blocking
		bool SendAsync(const char* address, int port, int length, const void *data, int timeout);	// no blocking call

	private:

		bool WaitForEvent(SOCKET s, HANDLE hEvent, long nEvents, int timeout);
		bool ProcessReply(SOCKET s, HANDLE event, int timeout);
		int SendDataPacket(Packet& p, SOCKET s, SOCKADDR_IN& address, HANDLE events);
		
		SOCKET m_socket;
		HANDLE m_event;
		WinSockWrap m_winsockWrap;

		// async
		void SendThread();

		HANDLE m_exitEvent;
		HANDLE m_dataReady;
		HANDLE m_sendComplete;
		int m_port;
		int m_timeout;
		std::string m_address;
		std::vector<UINT8> m_data;
		std::thread m_sendThread;
	};
}

#endif