#ifndef _TCPRECEIVE_H_
#define _TCPRECEIVE_H_

#include <thread>
#include <atomic>
#include <vector>
#include "SDL_net.h"

class TCPReceive
{
public:
	TCPReceive(int port);
	~TCPReceive();

	std::vector<char>& Receive();

private:

	void ListenFunc();

	TCPsocket m_listenSocket;
	std::atomic<TCPsocket> m_receiveSocket;
	std::thread m_listenThread;
	std::atomic_bool m_running;
	std::vector<char> m_recBuffer;
};

#endif