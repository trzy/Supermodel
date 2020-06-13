#include "TCPReceive.h"
#include <chrono> 

using namespace std::chrono_literals;

TCPReceive::TCPReceive(int port) :
	m_listenSocket(nullptr),
	m_receiveSocket(nullptr)
{
	SDLNet_Init();

	IPaddress ip;
	int result = SDLNet_ResolveHost(&ip, nullptr, port);

	if (result == 0) {
		m_listenSocket = SDLNet_TCP_Open(&ip);
		if (m_listenSocket) {
			m_running = true;
			m_listenThread = std::thread(&TCPReceive::ListenFunc, this);
		}
	}
}

TCPReceive::~TCPReceive()
{
	m_running = false;

	if (m_listenThread.joinable()) {
		m_listenThread.join();
	}

	if (m_listenSocket) {
		SDLNet_TCP_Close(m_listenSocket);
		m_listenSocket = nullptr;
	}

	if (m_receiveSocket) {
		SDLNet_TCP_Close(m_receiveSocket);
		m_receiveSocket = nullptr;
	}

	SDLNet_Quit();
}

std::vector<char>& TCPReceive::Receive()
{
	if (!m_receiveSocket) {
		printf("return here because no socket\n");
		m_recBuffer.clear();
		return m_recBuffer;
	}

	int size = 0;
	int result = 0;

	result = SDLNet_TCP_Recv(m_receiveSocket, &size, sizeof(int));
	printf("received %i\n", result);
	if (result <= 0) {
		SDLNet_TCP_Close(m_receiveSocket);
		m_receiveSocket = nullptr;
	}

	// reserve our space
	m_recBuffer.resize(size);

	while (size) {

		result = SDLNet_TCP_Recv(m_receiveSocket, m_recBuffer.data() + (m_recBuffer.size() - size), size);
		printf("received %i\n", result);
		if (result <= 0) {
			SDLNet_TCP_Close(m_receiveSocket);
			m_receiveSocket = nullptr;
			break;
		}

		size -= result;
	}

	return m_recBuffer;
}

void TCPReceive::ListenFunc()
{
	while (m_running) {

		std::this_thread::sleep_for(16ms);
		if (m_receiveSocket) continue;

		auto socket = SDLNet_TCP_Accept(m_listenSocket);

		if (socket) {
			m_receiveSocket = socket;
			printf("accepted a connectio\n");
		}
		
	}
}
