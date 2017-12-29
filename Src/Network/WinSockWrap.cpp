#include "WinSockWrap.h"
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

WinSockWrap::WinSockWrap()
{
	//==============
	WSADATA	wsaData;
	int		err;
	//==============

	m_count = 0;

	err = WSAStartup(MAKEWORD(2, 2), &wsaData);	// don't really need to care for checking supported version, we should be good from win95 onwards

	if (err == 0) {
		m_count++;
	}
}

WinSockWrap::~WinSockWrap()
{
	if (m_count) {
		WSACleanup();
	}
}