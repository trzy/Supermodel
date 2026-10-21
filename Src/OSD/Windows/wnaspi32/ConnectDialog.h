/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2003-2026 The Supermodel Team
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
 * ConnectDialog.h
 *
 * Connection to Supermodel for the replacement wnaspi32.dll: the "Supermodel
 * Pro-1000 Host" window, in which the user gives the host and the port of
 * Supermodel's SCSI TCP bridge.
 */

#ifndef INCLUDED_CONNECTDIALOG_H
#define INCLUDED_CONNECTDIALOG_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <cstdint>
#include <string>

/*
 * ShowConnectDialog(host, port, message):
 *
 * Shows the "Supermodel Pro-1000 Host" window and connects to Supermodel.
 * The window stays until a connection is made. If the user presses Exit,
 * the program ends (ExitProcess()). Do not call this from DllMain().
 *
 * Parameters:
 *    host      Host name or IP address. In: the first value in the window.
 *              Out: the value that was used for the connection.
 *    port      TCP port. In: the first value. Out: the value that was used.
 *    message   Text for the status line when the window opens (can be empty).
 *
 * Returns:
 *    The connected socket, or INVALID_SOCKET if the window cannot be shown.
 */
SOCKET ShowConnectDialog(std::string *host, uint16_t *port, const std::string &message);

/*
 * ConnectToHost(host, port, error):
 *
 * Finds the addresses of the host (getaddrinfo(): host names, IPv4 and IPv6
 * addresses) and connects to the first address that accepts the connection.
 * Each address has a time limit. Winsock must be started.
 *
 * Parameters:
 *    host    Host name or IP address. An IPv6 address can be in brackets.
 *    port    TCP port.
 *    error   Receives the reason if the connection fails.
 *
 * Returns:
 *    The connected socket, or INVALID_SOCKET if the connection fails.
 */
SOCKET ConnectToHost(const std::string &host, uint16_t port, std::string *error);

#endif  // INCLUDED_CONNECTDIALOG_H
