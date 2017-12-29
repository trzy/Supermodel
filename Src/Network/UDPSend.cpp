#include <winsock2.h>
#include <windows.h>
#include "UDPSend.h"
#include <WS2tcpip.h>
#include <stdio.h>

namespace SMUDP
{

UDPSend::UDPSend()
{
	m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);		// create the socket
	m_event = CreateEvent(NULL, FALSE, FALSE, NULL);

	WSAEventSelect(m_socket, m_event, FD_READ | FD_WRITE);
}

UDPSend::~UDPSend()
{
	closesocket(m_socket);
	CloseHandle(m_event);
}

bool UDPSend::Send(const char* ip, int port, int length, const void *data, int timeout)
{
	UINT8* pData = (UINT8*)data;

	Packet packet;
	packet.CalcTotalIDs(length);

	SOCKADDR_IN address = {};
	address.sin_family	= AF_INET;			// address family Internet
	address.sin_port	= htons(port);		// set server’s port number
	inet_pton(AF_INET, ip, &address.sin_addr);

	while (length > 0) {

		packet.flags = 0;	// reset the flags (not used currently)

		if (length > packet.BUFFER_SIZE) {
			packet.length = packet.BUFFER_SIZE;
		}
		else {
			packet.length = length;
		}

		memcpy(packet.data, pData, packet.length);

		int sent = SendDataPacket(packet, m_socket, address, m_event);

		if (sent == SOCKET_ERROR) {
			return false;		// send failure
		}

		if (!ProcessReply(m_socket, m_event, timeout)) {
			return false;		// reply failure
		}

		length -= packet.length;
		pData += packet.length;

		packet.currentID++;
	}

	return true;
}

bool UDPSend::WaitForEvent(SOCKET s, HANDLE hEvent, long nEvents, int timeout)
{
	//========
	DWORD res;
	//========

	res = WaitForSingleObject(hEvent, timeout);

	if (res == WAIT_OBJECT_0) {

		WSANETWORKEVENTS events = {};
		WSAEnumNetworkEvents(s, hEvent, &events);

		if (events.lNetworkEvents & nEvents) {
			return true;
		}
	}

	return false;
}

int UDPSend::SendDataPacket(Packet &p, SOCKET s, SOCKADDR_IN& address, HANDLE events)
{
	while (true) {

		int sent = sendto(s, p, p.Size(), 0, (struct sockaddr *)&address, sizeof(SOCKADDR_IN));

		if (sent == SOCKET_ERROR) {

			int error = WSAGetLastError();	// clear error code

			if (error == WSAEWOULDBLOCK) {
				WaitForEvent(s, events, FD_WRITE, INFINITE);		// wait until write event is triggered
				continue;
			}

			return sent;	// send failure
		}

		return p.Size();
	}
}

bool UDPSend::ProcessReply(SOCKET s, HANDLE event, int timeout)
{
	//=================
	int			result;
	PacketReply	rp;
	//=================

	if (WaitForEvent(s, event, FD_READ, timeout)) {

		result = recv(s, &rp, sizeof(rp), 0);

		if (result == SOCKET_ERROR) {
			auto error = WSAGetLastError();	// clear error code
			return false;
		}

		return true;
	}

	return false;
}

}