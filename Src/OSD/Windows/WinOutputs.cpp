/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011 Bart Trzynadlowski, Nik Henson
 **
 ** This file is part of Supermodel.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free 
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **
 ** Supermodel is distributed in the hope that it will be useful, but WITHOUT
 ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 ** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 ** more details.
 **
 ** You should have received a copy of the GNU General Public License along
 ** with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
 **/
 
/*
 * WinOutputs.cpp
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "OSD/Windows/WinOutputs.h"
#include "Supermodel.h"

#define OUTPUT_WINDOW_CLASS	TEXT("MAMEOutput")
#define OUTPUT_WINDOW_NAME	TEXT("MAMEOutput")

// Messages sent from emulator to external clients
#define START        TEXT("MAMEOutputStart")
#define STOP		 TEXT("MAMEOutputStop")
#define UPDATE_STATE TEXT("MAMEOutputUpdateState")

// Messages sent from external clients to emulator
#define REGISTER_CLIENT	  TEXT("MAMEOutputRegister")
#define UNREGISTER_CLIENT TEXT("MAMEOutputUnregister")
#define GET_ID_STRING	  TEXT("MAMEOutputGetIDString")

#define COPYDATA_MESSAGE_ID_STRING 1

bool CWinOutputs::s_createdClass = false;

CWinOutputs::CWinOutputs() : m_hwnd(NULL)
{
	//
}

CWinOutputs::~CWinOutputs()
{
	// Broadcast a shutdown message
	if (m_hwnd)
		PostMessage(HWND_BROADCAST, m_onStop, (WPARAM)m_hwnd, 0);
	DeleteWindowClass();
}

bool CWinOutputs::Initialize()
{
	// Create window class
	if (!CreateWindowClass())
	{
		ErrorLog("Unable to register window class for Windows outputs");
		return false;
	}
	
	// Create window
	m_hwnd = CreateWindowEx(0,
							OUTPUT_WINDOW_CLASS,
							OUTPUT_WINDOW_NAME,
							WS_OVERLAPPEDWINDOW,
							0, 0,
							1, 1,
							NULL,
							NULL,
							GetModuleHandle(NULL),
							NULL);
	if (!m_hwnd)
	{
		ErrorLog("Unable to create window handle for Windows outputs");
		return false;
	}

	// Allocate message ids
	if (!AllocateMessageId(m_onStart,     START))             return false;
	if (!AllocateMessageId(m_onStop,      STOP))              return false;
	if (!AllocateMessageId(m_updateState, UPDATE_STATE))      return false;
	if (!AllocateMessageId(m_regClient,   REGISTER_CLIENT))   return false;
	if (!AllocateMessageId(m_unregClient, UNREGISTER_CLIENT)) return false;
	if (!AllocateMessageId(m_getIdString, GET_ID_STRING))     return false;

	// Set pointer to this object
	SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);
	return true;
}

void CWinOutputs::Attached()
{
	// Broadcast a startup message
	PostMessage(HWND_BROADCAST, m_onStart, (WPARAM)m_hwnd, 0);
}

void CWinOutputs::SendOutput(EOutputs output, UINT8 prevValue, UINT8 value)
{
	//printf("LAMP OUTPUT %s = %u -> %u\n", GetOutputName(output), prevValue, value);
	
	// Loop through all registered clients and send them new output value
	LPARAM param = (LPARAM)output + 1;
	for (vector<RegisteredClient>::iterator it = m_clients.begin(), end = m_clients.end(); it != end; it++)
		PostMessage(it->hwnd, m_updateState, param, value);
}

LRESULT CALLBACK CWinOutputs::OutputWindowProcCallback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	LONG_PTR ptr = GetWindowLongPtr(hwnd, GWLP_USERDATA);
	CWinOutputs *outputs = (CWinOutputs*)ptr;
	if (!outputs)
		return 1;
	return outputs->OutputWindowProc(hwnd, msg, wParam, lParam);
}

bool CWinOutputs::CreateWindowClass()
{
	if (s_createdClass)
		return true;

	// Setup description of window class
	WNDCLASS wc = { 0 };
	wc.lpszClassName = OUTPUT_WINDOW_CLASS;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpfnWndProc = OutputWindowProcCallback;

	// Register class
	if (RegisterClass(&wc))
	{
		s_createdClass = true;
		return true;
	}
	
	return false;
}
bool CWinOutputs::DeleteWindowClass()
{
	if (!s_createdClass)
		return true;

	// Register class
	if (UnregisterClass(OUTPUT_WINDOW_CLASS, GetModuleHandle(NULL))) {
		s_createdClass = false;
		return true;
	}

	return false;
}
bool CWinOutputs::AllocateMessageId(UINT &regId, LPCSTR str)
{
	regId = RegisterWindowMessage(str);
	if (regId != 0)
		return true;
	ErrorLog("Unable to register window message '%s' for Windows outputs", str);
	return false;
}

LRESULT CWinOutputs::OutputWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Process message sent to emulator window
	if      (msg == m_regClient)   return RegisterClient((HWND)wParam, lParam);
	else if (msg == m_unregClient) return UnregisterClient((HWND)wParam, lParam);
	else if (msg == m_getIdString) return SendIdString((HWND)wParam, lParam);
	else                           return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CWinOutputs::RegisterClient(HWND hwnd, LPARAM id)
{
	// Check that given client is not already registered
	for (vector<RegisteredClient>::iterator it = m_clients.begin(), end = m_clients.end(); it != end; it++)
	{
		if (it->id == id)
		{
			it->hwnd = hwnd;

			// If so, just send it current state of all outputs
			SendAllToClient(*it);
			return 1;
		}
	}

	// If not, store details about client and send it current state of all outputs
	RegisteredClient client;
	client.id = id;
	client.hwnd = hwnd;
	m_clients.push_back(client);

	SendAllToClient(client);
	return 0;
}

void CWinOutputs::SendAllToClient(RegisteredClient &client)
{
	// Loop through all known outputs and send their current state to given client
	for (unsigned i = 0; i < NUM_OUTPUTS; i++)
	{
		EOutputs output = (EOutputs)i;
		LPARAM param = (LPARAM)output + 1;
		PostMessage(client.hwnd, m_updateState, param, GetValue(output));
	}
}

LRESULT CWinOutputs::UnregisterClient(HWND hwnd, LPARAM id)
{
	// Find any matching clients and remove them
	bool found = false;
	vector<RegisteredClient>::iterator it = m_clients.begin();
	while (it != m_clients.end())
	{
		if (it->id == id)
		{
			it = m_clients.erase(it);
			found = true;
		}
		else
			it++;
	}

	// Return error if no matches found
	return (found ? 0 : 1);
}

LRESULT CWinOutputs::SendIdString(HWND hwnd, LPARAM id)
{
	// Id 0 is the name of the game
	std::string name;
	if (id == 0)
		name = GetGame().name;
	else
		name = MapIdToName(id) ? MapIdToName(id) : "";
	
	// Allocate memory for message
	int dataLen = sizeof(CopyDataIdString) + name.length();
	CopyDataIdString *data = (CopyDataIdString*)new(nothrow) UINT8[dataLen];
	if (!data)
		return 1;
	data->id = id;
	strcpy(data->string, name.c_str());

	// Reply by using SendMessage with WM_COPYDATA
	COPYDATASTRUCT copyData;
	copyData.dwData = COPYDATA_MESSAGE_ID_STRING;
	copyData.cbData = dataLen;
	copyData.lpData = data;
	SendMessage(hwnd, WM_COPYDATA, (WPARAM)m_hwnd, (LPARAM)&copyData);

	delete[] data;
	return 0;
}

const char *CWinOutputs::MapIdToName(LPARAM id)
{
	EOutputs output = (EOutputs)(id - 1);
	return GetOutputName(output);
}

LPARAM CWinOutputs::MapNameToId(const char *name)
{
	EOutputs output = GetOutputByName(name);
	if (output == OutputUnknown)
		return 0;
	return (LPARAM)output + 1;
}