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
 * WinOutputs.h
 *
 * Implementation of COutputs that sends MAMEHooker compatible messages via Windows messages.
 */

#ifndef INCLUDED_WINOUTPUTS_H
#define INCLUDED_WINOUTPUTS_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "OSD/Outputs.h"

#include <vector>

using namespace std;

// Struct that represents a client (eg MAMEHooker) currently registered with the emulator
struct RegisteredClient
{
	LPARAM id;	// Client-specified id
	HWND hwnd;	// Client HWND
};

struct CopyDataIdString
{
	UINT32 id;		// Id that was requested
	char string[1];	// String containing data
};

class CWinOutputs : public COutputs
{
public:
	/*
	 * CWinOutputs():
	 * ~CWinOutputs():
	 *
	 * Constructor and destructor.
	 */
	CWinOutputs();
	
	virtual ~CWinOutputs();

	/*
	 * Initialize():
	 *
 	 * Initializes this class.
	 */
	bool Initialize();

	/*
	 * Attached():
	 *
	 * Lets the class know that it has been attached to the emulator.
	 */
	void Attached();

protected:
	/*
	 * SendOutput():
	 *
	 * Sends the appropriate output message to all registered clients.
	 */
	void SendOutput(EOutputs output, UINT8 prevValue, UINT8 value);

private:
	static bool s_createdClass;

	/*
	 * CreateWindowClass():
	 *
	 * Registers the window class and sets up OutputWindowProcCallback to process all messages sent to the emulator window. 
	 */
	static bool CreateWindowClass();
	static bool DeleteWindowClass();

	/*
	 * OutputWindowProcCallback(hwnd, msg, wParam, lParam):
	 *
	 * Receives all messages sent to the emulator window and passes them on to the CWinOutputs object (whose pointer is passed
	 * via GWLP_USERDATA).
	 */
	static LRESULT CALLBACK OutputWindowProcCallback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	
	HWND m_hwnd;

	UINT m_onStart;
	UINT m_onStop;
	UINT m_updateState;
	UINT m_regClient;
	UINT m_unregClient;
	UINT m_getIdString;

	vector<RegisteredClient> m_clients;

	/*
	 * AllocateMessageId(regId, str):
	 *
	 * Defines a new window message type with the given name and returns the allocated message id.
	 */
	bool AllocateMessageId(UINT &regId, LPCSTR str);

	/*
	 * OutputWindowProc(hwnd, msg, wParam, lParam):
	 *
	 * Processes the messages sent to the emulator window.
	 */
	LRESULT OutputWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	/*
	 * RegisterClient(hwnd, id):
	 *
	 * Registers a client (eg MAMEHooker) with the emulator.
	 */
	LRESULT RegisterClient(HWND hwnd, LPARAM id);

	/*
	 * SendAllToClient(client):
	 *
	 * Sends the current state of all the outputs to the given registered client.
	 * Called whenever a client is registered with the emulator.
	 */
	void SendAllToClient(RegisteredClient &client);

	/*
	 * UnregisterClient(hwnd, id):
	 *
	 * Unregisters a client from the emulator.
	 */
	LRESULT UnregisterClient(HWND hwnd, LPARAM id);

	/*
	 * SendIdString(hwnd, id):
	 *
	 * Sends the name of the requested output back to a client, or the name of the current running game if an id of zero is requested.
	 */
	LRESULT SendIdString(HWND hwnd, LPARAM id);

	/*
	 * MapIdToName(id):
	 *
	 * Maps the given id to an output's name.
	 */
	const char *MapIdToName(LPARAM id);

	/*
	 * MapNameToId(name):
	 *
	 * Maps the given name to an output's id.
	 */
	LPARAM MapNameToId(const char *name);
};

#endif	// INCLUDED_WINOUTPUTS_H
