#include "TCPSend.h"

static const int RETRY_COUNT = 10;			// shrugs

TCPSend::TCPSend(std::string& ip, int port) :
	m_ip(ip),
	m_port(port),
	m_tryCount(0),
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
	printf("trying to send data %i\n", length);
	// If we aren't connected make a connection
	if (!Connected()) {
		Connect();
		printf("trying to connect\n");
	}

	// If we failed bail out
	if (!Connected()) {
		printf("not connected\n");
		return false;
	}

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

	return false;
}

bool TCPSend::Connected()
{
	return m_socket != 0;
}

void TCPSend::Connect()
{
	if (m_tryCount >= RETRY_COUNT) {
		return;					// already tried and failed so bail out. We do this as instances might load diff times
	}

	IPaddress ip;
	int result = SDLNet_ResolveHost(&ip, m_ip.c_str(), m_port);

	if (result == 0) {
		m_socket = SDLNet_TCP_Open(&ip);
	}

	m_tryCount++;
}
