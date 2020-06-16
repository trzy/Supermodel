#include "TCPSend.h"

#if defined(_DEBUG)
#include <stdio.h>
#define DPRINTF printf
#else
#define DPRINTF(a, ...)
#endif

static const int RETRY_COUNT = 10;			// shrugs

TCPSend::TCPSend(std::string& ip, int port) :
	m_ip(ip),
	m_port(port),
	m_socket(nullptr)
{
	SDLNet_Init();
}

TCPSend::~TCPSend()
{
	if (m_socket) {
		SDLNet_TCP_Close(m_socket);
		m_socket = nullptr;
	}

	SDLNet_Quit();	// unload lib (winsock dll for windows)
}

bool TCPSend::Send(const void * data, int length)
{
	// If we failed bail out
	if (!Connected()) {
		DPRINTF("Not connected\n");
		return false;
	}

	DPRINTF("Sending %i bytes\n", length);

	if (!length) {
		return true;		// 0 sized packet will blow our connex
	}

	int sent = 0;

	sent = SDLNet_TCP_Send(m_socket, &length, sizeof(int));		// pack the length at the start of transmission.
	sent = SDLNet_TCP_Send(m_socket, data, length);

	if (sent < length) {
		SDLNet_TCP_Close(m_socket);
		m_socket = nullptr;
	}

	return true;
}

bool TCPSend::Connected()
{
	return m_socket != 0;
}

bool TCPSend::Connect()
{
	IPaddress ip;
	int result = SDLNet_ResolveHost(&ip, m_ip.c_str(), m_port);

	if (result == 0) {
		m_socket = SDLNet_TCP_Open(&ip);
	}

	return Connected();
}
