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
	bool Connected();
private:

	void Connect();

	std::string	m_ip;
	int			m_port;
	bool		m_connected;
	int			m_tryCount;
	TCPsocket	m_socket;		// sdl socket

};

#endif
