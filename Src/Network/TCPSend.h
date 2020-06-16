#ifndef _TCPSEND_H_
#define _TCPSEND_H_

#include <string>
#include "SDL_net.h"

class TCPSend
{
public:
	TCPSend(std::string& ip, int port);
	~TCPSend();

	bool Send(const void* data, int length);
	bool Connect();
	bool Connected();
private:

	std::string	m_ip;
	int			m_port;
	TCPsocket	m_socket;		// sdl socket

};

#endif
