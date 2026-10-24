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
 * ConnectDialog.cpp
 *
 * The "Supermodel Pro-1000 Host" window of the replacement wnaspi32.dll.
 *
 * The window has a Host box, a Port box, a status line, and two buttons:
 *
 *  - Connect: available only when the host and the port are valid. A worker
 *    thread finds the addresses of the host and connects, so the window
 *    continues to operate. The window closes when the connection is made.
 *    If the connection fails, the status line gives the reason.
 *  - Exit: ends the program (ExitProcess()).
 *
 * The window has no close button (no system menu), and it ignores Esc and
 * Alt+F4. The DLL has no resource file, thus the dialog template is made in
 * memory.
 *
 * A valid host has 1 to 253 characters: letters, digits, '.', '-', '_', and
 * ':' for IPv6 addresses (optionally in brackets). A valid port is a decimal
 * number from 1 to 65535. Whether the host exists is found only when the user
 * presses Connect.
 */

#include "ConnectDialog.h"

#include <ws2tcpip.h>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <vector>

namespace
{
  // Control IDs
  constexpr int ID_HOST = 101;
  constexpr int ID_PORT = 102;
  constexpr int ID_STATUS = 103;
  constexpr int ID_EXIT = 104;
  constexpr int ID_CONNECT = IDOK;

  // Message from the worker thread: wParam = socket, lParam = ConnectResult*
  constexpr UINT WM_CONNECT_DONE = WM_APP + 1;

  constexpr DWORD CONNECT_TIMEOUT_MS = 5000;  // for each address

  struct DialogState
  {
    std::string *host;
    uint16_t *port;
    std::string message;
    SOCKET socket = INVALID_SOCKET;
    bool connecting = false;
  };

  struct ConnectRequest
  {
    HWND window;
    std::string host;
    uint16_t port;
  };

  struct ConnectResult
  {
    std::string error;
  };

  std::wstring Widen(const std::string &text)
  {
    std::wstring result;
    int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (length > 1)
    {
      result.resize(length - 1);
      MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &result[0], length);
    }
    return result;
  }

  std::string Narrow(const std::wstring &text)
  {
    std::string result;
    int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (length > 1)
    {
      result.resize(length - 1);
      WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &result[0], length, nullptr, nullptr);
    }
    return result;
  }

  std::wstring GetControlText(HWND window, int id)
  {
    HWND control = GetDlgItem(window, id);
    int length = GetWindowTextLengthW(control);
    std::wstring text(length + 1, L'\0');
    GetWindowTextW(control, &text[0], length + 1);
    text.resize(length);
    return text;
  }

  bool ValidHost(const std::wstring &host)
  {
    if (host.empty() || host.size() > 253)
      return false;
    for (wchar_t c : host)
    {
      bool ok = (c < 0x80 && iswalnum(c)) || c == L'.' || c == L'-' || c == L'_' || c == L':' || c == L'[' || c == L']';
      if (!ok)
        return false;
    }
    return true;
  }

  bool ParsePort(const std::wstring &text, uint16_t *port)
  {
    if (text.empty() || text.size() > 5)
      return false;
    unsigned value = 0;
    for (wchar_t c : text)
    {
      if (c < L'0' || c > L'9')
        return false;
      value = value * 10 + unsigned(c - L'0');
    }
    if (value < 1 || value > 65535)
      return false;
    *port = uint16_t(value);
    return true;
  }

  // Makes Connect available only when the host and the port are valid
  void UpdateControls(HWND window, const DialogState *state)
  {
    uint16_t port;
    bool valid = ValidHost(GetControlText(window, ID_HOST)) && ParsePort(GetControlText(window, ID_PORT), &port);
    EnableWindow(GetDlgItem(window, ID_CONNECT), valid && !state->connecting);
    EnableWindow(GetDlgItem(window, ID_HOST), !state->connecting);
    EnableWindow(GetDlgItem(window, ID_PORT), !state->connecting);
  }

  void SetStatus(HWND window, const std::string &text)
  {
    SetDlgItemTextW(window, ID_STATUS, Widen(text).c_str());
  }

  DWORD WINAPI ConnectThread(LPVOID parameter)
  {
    ConnectRequest *request = static_cast<ConnectRequest *>(parameter);
    ConnectResult *result = new ConnectResult();
    SOCKET socket = ConnectToHost(request->host, request->port, &result->error);
    PostMessageW(request->window, WM_CONNECT_DONE, WPARAM(socket), LPARAM(result));
    delete request;
    return 0;
  }

  void StartConnect(HWND window, DialogState *state)
  {
    uint16_t port;
    std::wstring host = GetControlText(window, ID_HOST);
    if (state->connecting || !ValidHost(host) || !ParsePort(GetControlText(window, ID_PORT), &port))
      return;

    ConnectRequest *request = new ConnectRequest();
    request->window = window;
    request->host = Narrow(host);
    request->port = port;
    *state->host = request->host;
    *state->port = port;

    state->connecting = true;
    UpdateControls(window, state);
    SetStatus(window, "Connecting to " + request->host + " (port " + std::to_string(port) + ")...");

    HANDLE thread = CreateThread(nullptr, 0, ConnectThread, request, 0, nullptr);
    if (thread)
      CloseHandle(thread);
    else
    {
      delete request;
      state->connecting = false;
      UpdateControls(window, state);
      SetStatus(window, "Unable to start the connection.");
    }
  }

  INT_PTR CALLBACK DialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
  {
    DialogState *state = reinterpret_cast<DialogState *>(GetWindowLongPtrW(window, DWLP_USER));

    switch (message)
    {
    case WM_INITDIALOG:
      state = reinterpret_cast<DialogState *>(lParam);
      SetWindowLongPtrW(window, DWLP_USER, LONG_PTR(state));
      SendDlgItemMessageW(window, ID_HOST, EM_LIMITTEXT, 253, 0);
      SendDlgItemMessageW(window, ID_PORT, EM_LIMITTEXT, 5, 0);
      SetDlgItemTextW(window, ID_HOST, Widen(*state->host).c_str());
      SetDlgItemTextW(window, ID_PORT, std::to_wstring(*state->port).c_str());
      SetStatus(window, state->message);
      UpdateControls(window, state);
      SetForegroundWindow(window);
      return TRUE;

    case WM_COMMAND:
      switch (LOWORD(wParam))
      {
      case ID_HOST:
      case ID_PORT:
        if (HIWORD(wParam) == EN_CHANGE)
          UpdateControls(window, state);
        return TRUE;
      case ID_CONNECT:  // also Enter
        StartConnect(window, state);
        return TRUE;
      case ID_EXIT:
        ExitProcess(1);
        return TRUE;
      case IDCANCEL:    // Esc: ignore
        return TRUE;
      }
      break;

    case WM_CONNECT_DONE:
    {
      SOCKET socket = SOCKET(wParam);
      ConnectResult *result = reinterpret_cast<ConnectResult *>(lParam);
      state->connecting = false;
      if (socket != INVALID_SOCKET)
      {
        state->socket = socket;
        EndDialog(window, IDOK);
      }
      else
      {
        SetStatus(window, result->error);
        UpdateControls(window, state);
      }
      delete result;
      return TRUE;
    }

    case WM_CLOSE:      // Alt+F4: ignore
      return TRUE;
    }
    return FALSE;
  }

  /*
   * In-memory dialog template. Coordinates are in dialog units.
   */
  class DialogTemplate
  {
  public:
    DialogTemplate(const wchar_t *title, short width, short height, unsigned numItems)
    {
      DLGTEMPLATE dialog = {};
      dialog.style = DS_SETFONT | DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION;  // no WS_SYSMENU: no close button
      dialog.cdit = WORD(numItems);
      dialog.cx = width;
      dialog.cy = height;
      Add(&dialog, sizeof(dialog));
      AddWord(0);                     // no menu
      AddWord(0);                     // standard dialog class
      AddString(title);
      AddWord(8);                     // font size
      AddString(L"MS Shell Dlg");
    }

    // Window classes: 0x80 button, 0x81 edit, 0x82 static
    void AddItem(WORD windowClass, int id, DWORD style, short x, short y, short width, short height, const wchar_t *text)
    {
      Align();
      DLGITEMTEMPLATE item = {};
      item.style = style | WS_CHILD | WS_VISIBLE;
      item.x = x;
      item.y = y;
      item.cx = width;
      item.cy = height;
      item.id = WORD(id);
      Add(&item, sizeof(item));
      AddWord(0xFFFF);
      AddWord(windowClass);
      AddString(text);
      AddWord(0);                     // no creation data
    }

    const DLGTEMPLATE *Get(void) const
    {
      return reinterpret_cast<const DLGTEMPLATE *>(m_data.data());
    }

  private:
    void Add(const void *data, size_t size)
    {
      const uint8_t *p = static_cast<const uint8_t *>(data);
      m_data.insert(m_data.end(), p, p + size);
    }

    void AddWord(WORD value)
    {
      Add(&value, sizeof(value));
    }

    void AddString(const wchar_t *text)
    {
      Add(text, (wcslen(text) + 1) * sizeof(wchar_t));
    }

    void Align(void)
    {
      while (m_data.size() % 4)
        m_data.push_back(0);
    }

    std::vector<uint8_t> m_data;
  };
}

SOCKET ConnectToHost(const std::string &hostText, uint16_t port, std::string *error)
{
  std::string host = hostText;
  if (host.size() >= 2 && host.front() == '[' && host.back() == ']')
    host = host.substr(1, host.size() - 2);

  addrinfo hints = {};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  addrinfo *addresses = nullptr;
  std::string portText = std::to_string(port);
  int result = getaddrinfo(host.c_str(), portText.c_str(), &hints, &addresses);
  if (result != 0 || !addresses)
  {
    *error = "Cannot find host " + hostText + ".";
    return INVALID_SOCKET;
  }

  *error = "No connection to " + hostText + " (port " + portText + "). Is Supermodel running?";
  SOCKET connected = INVALID_SOCKET;
  for (addrinfo *address = addresses; address && connected == INVALID_SOCKET; address = address->ai_next)
  {
    SOCKET s = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
    if (s == INVALID_SOCKET)
      continue;

    // Connect without blocking, and wait for a limited time
    u_long nonBlocking = 1;
    ioctlsocket(s, FIONBIO, &nonBlocking);
    int status = connect(s, address->ai_addr, int(address->ai_addrlen));
    bool ok = status == 0;
    if (!ok && WSAGetLastError() == WSAEWOULDBLOCK)
    {
      fd_set writeSet, errorSet;
      FD_ZERO(&writeSet);
      FD_ZERO(&errorSet);
      FD_SET(s, &writeSet);
      FD_SET(s, &errorSet);
      timeval timeout = { long(CONNECT_TIMEOUT_MS / 1000), long((CONNECT_TIMEOUT_MS % 1000) * 1000) };
      int ready = select(0, nullptr, &writeSet, &errorSet, &timeout);
      if (ready > 0 && FD_ISSET(s, &writeSet))
        ok = true;
      else if (ready == 0)
        *error = "No answer from " + hostText + " (port " + portText + ").";
    }
    if (ok)
    {
      u_long blocking = 0;
      ioctlsocket(s, FIONBIO, &blocking);
      connected = s;
    }
    else
      closesocket(s);
  }
  freeaddrinfo(addresses);
  return connected;
}

SOCKET ShowConnectDialog(std::string *host, uint16_t *port, const std::string &message)
{
  DialogTemplate dialog(L"Supermodel Pro-1000 Host", 200, 88, 7);
  dialog.AddItem(0x82, -1, SS_LEFT, 7, 9, 30, 8, L"Host:");
  dialog.AddItem(0x81, ID_HOST, ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, 40, 7, 153, 12, L"");
  dialog.AddItem(0x82, -1, SS_LEFT, 7, 27, 30, 8, L"Port:");
  dialog.AddItem(0x81, ID_PORT, ES_NUMBER | WS_BORDER | WS_TABSTOP, 40, 25, 40, 12, L"");
  dialog.AddItem(0x82, ID_STATUS, SS_LEFT, 7, 43, 186, 18, L"");
  dialog.AddItem(0x80, ID_CONNECT, BS_DEFPUSHBUTTON | WS_TABSTOP, 88, 67, 50, 14, L"Connect");
  dialog.AddItem(0x80, ID_EXIT, BS_PUSHBUTTON | WS_TABSTOP, 143, 67, 50, 14, L"Exit");

  DialogState state;
  state.host = host;
  state.port = port;
  state.message = message;
  INT_PTR result = DialogBoxIndirectParamW(GetModuleHandleW(nullptr), dialog.Get(), nullptr, DialogProc, LPARAM(&state));
  if (result != IDOK)
    return INVALID_SOCKET;
  return state.socket;
}
