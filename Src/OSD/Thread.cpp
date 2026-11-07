/**
 ** Supermodel
 ** A Sega Model Series Arcade Emulator.
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
 * Thread.cpp
 *
 * Platform-specific parts of threads.
 */

#include "Thread.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <pthread.h>
#include <signal.h>
#endif

#include <utility>

namespace
{
  void BlockAsynchronousSignals()
  {
    // Emulation threads must not be interrupted to handle these: they are left
    // to the main thread
#if !defined(_WIN32)
    static const int signals[] = { SIGHUP, SIGINT, SIGQUIT, SIGPIPE, SIGALRM, SIGTERM, SIGCHLD, SIGWINCH, SIGVTALRM, SIGPROF };
    sigset_t mask;
    sigemptyset(&mask);
    for (int sig : signals)
      sigaddset(&mask, sig);
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);
#endif
  }
}

void Thread::SetCurrentName(const std::string &name)
{
#if defined(_WIN32)
  // Not available before Windows 10
  using SetThreadDescriptionFunc = HRESULT (WINAPI *)(HANDLE, PCWSTR);
  static const SetThreadDescriptionFunc setThreadDescription = reinterpret_cast<SetThreadDescriptionFunc>(
    reinterpret_cast<void *>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetThreadDescription")));
  if (setThreadDescription != nullptr)
  {
    std::wstring wideName(name.begin(), name.end());  // names are ASCII
    setThreadDescription(GetCurrentThread(), wideName.c_str());
  }
#elif defined(__APPLE__)
  pthread_setname_np(name.c_str());
#elif defined(__linux__)
  // Limited to 16 bytes, including the terminator
  pthread_setname_np(pthread_self(), name.substr(0, 15).c_str());
#else
  (void) name;
#endif
}

std::thread Thread::Start(const std::string &name, std::function<void()> body)
{
  return std::thread([name, body = std::move(body)]
  {
    BlockAsynchronousSignals();
    SetCurrentName(name);
    body();
  });
}
