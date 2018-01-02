#include <winsock2.h>
#include <windows.h>
#include "UDPSend.h"
#include <WS2tcpip.h>
#include <stdio.h>

namespace SMUDP
{

UDPSend::UDPSend()
{
	m_socket		= socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);		// create the socket
	m_event			= CreateEvent(NULL, FALSE, FALSE, NULL);
	m_exitEvent		= CreateEvent(NULL, FALSE, FALSE, NULL);
	m_dataReady		= CreateEvent(NULL, FALSE, FALSE, NULL);			
	m_sendComplete	= CreateEvent(NULL, FALSE, TRUE, NULL);			// start off ready

	m_sendThread = std::thread(&UDPSend::SendThread, this);

	WSAEventSelect(m_socket, m_event, FD_READ | FD_WRITE);
}

UDPSend::~UDPSend()
{
	SetEvent(m_exitEvent);		// trigger thread to exit
	m_sendThread.join();		// block until thread has exited
	CloseHandle(m_exitEvent);	// clean up the rest of the resources
	CloseHandle(m_dataReady);
	CloseHandle(m_sendComplete);

	closesocket(m_socket);
	CloseHandle(m_event);
}

bool UDPSend::SendAsync(const char* address, int port, int length, const void *data, int timeout)
{
	WaitForSingleObject(m_sendComplete, INFINITE);	// block until previous sends have completed, don't want overlapping

	m_data.clear();
	m_data.insert(m_data.end(), (UINT8*)data, (UINT8*)data + length);

	m_address	= address;
	m_port		= port;
	m_timeout	= timeout;

	SetEvent(m_dataReady);

	return true;
}

void UDPSend::SendThread()
{
	HANDLE events[2];

	events[0] = m_exitEvent;
	events[1] = m_dataReady;

	while (true) {

		auto result = WaitForMultipleObjects(_countof(events), events, FALSE, INFINITE);

		if (result == WAIT_OBJECT_0) {					// exit event triggered
			break;	
		}
		else if (result == (WAIT_OBJECT_0 + 1)) {		// data ready
			Send(m_address.c_str(), m_port, (int)m_data.size(), m_data.data(), m_timeout);
			SetEvent(m_sendComplete);
		}
	}
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