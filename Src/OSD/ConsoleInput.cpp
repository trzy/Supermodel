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
 * ConsoleInput.cpp
 *
 * Implementation of CConsoleInput. The reader thread is started lazily by the
 * first Claim() on any endpoint (see ConsoleInput.h for why). It waits for
 * input with a timeout so that it can notice stop requests, then reads a line
 * with fgets(). If it is blocked in fgets() when stopped (e.g., the user is in
 * the middle of typing a line), it is detached; its state is reference counted
 * so that this is safe.
 *
 * Ordinarily, this would have separate OS-dependent functions split across
 * Windows/, SDL/, etc., but it was easier to handle it all here.
 */

#include "ConsoleInput.h"

#include "OSD/Logger.h"
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <poll.h>
#include <unistd.h>
#endif


/******************************************************************************
 Shared State and Endpoints
******************************************************************************/

namespace
{
  class Endpoint;
}

struct CConsoleInput::State
{
  std::mutex mutex;
  std::condition_variable lineAvailable;
  std::vector<std::unique_ptr<Endpoint>> endpoints;
  uint64_t nextClaimOrder = 1;
  bool stop = false;
  bool eof = false;
  bool exited = false;

  Endpoint *Receiver();               // endpoint with most recent active claim (mutex must be held)
  void Deliver(const std::string &line);
  void SetEOF();
};

namespace
{
  class Endpoint : public IConsoleInput
  {
  public:
    void Claim(void) override
    {
      {
        std::lock_guard<std::mutex> lock(m_state->mutex);
        ++m_claimCount;
        m_claimOrder = m_state->nextClaimOrder++;
        m_state->lineAvailable.notify_all();
      }

      // Start the reader only now that a claimant exists to receive its first
      // line. Done outside the lock because starting may take it (SetEOF).
      m_startReader();
    }

    void Release(void) override
    {
      std::lock_guard<std::mutex> lock(m_state->mutex);
      if (m_claimCount > 0)
        --m_claimCount;
      m_state->lineAvailable.notify_all();
    }

    bool ReadLine(std::string *line, bool wait) override
    {
      std::unique_lock<std::mutex> lock(m_state->mutex);
      if (wait)
      {
        if (m_lines.empty() && m_state->Receiver() != this)
        {
          ErrorLog("Console input requested by an endpoint that has not claimed the console.");
          return false;
        }
        m_state->lineAvailable.wait(lock, [this]
        {
          return !m_lines.empty() || m_state->eof || m_state->Receiver() != this;
        });
      }
      if (m_lines.empty())
        return false;
      *line = m_lines.front();
      m_lines.pop_front();
      return true;
    }

    Endpoint(CConsoleInput::State *state, std::function<void()> startReader)
      : m_state(state),
        m_startReader(startReader)
    {
    }

    CConsoleInput::State *m_state;
    std::function<void()> m_startReader;
    std::deque<std::string> m_lines;
    unsigned m_claimCount = 0;
    uint64_t m_claimOrder = 0;
  };
}

Endpoint *CConsoleInput::State::Receiver()
{
  Endpoint *receiver = nullptr;
  for (auto &endpoint: endpoints)
  {
    if (endpoint->m_claimCount > 0 && (!receiver || endpoint->m_claimOrder > receiver->m_claimOrder))
      receiver = endpoint.get();
  }
  return receiver;
}

void CConsoleInput::State::Deliver(const std::string &line)
{
  std::lock_guard<std::mutex> lock(mutex);
  Endpoint *receiver = Receiver();
  if (receiver)
  {
    receiver->m_lines.push_back(line);
    lineAvailable.notify_all();
  }
  else
    DebugLog("Console input discarded (no claimant): %s\n", line.c_str());
}

void CConsoleInput::State::SetEOF()
{
  std::lock_guard<std::mutex> lock(mutex);
  eof = true;
  lineAvailable.notify_all();
}


/******************************************************************************
 Reader Thread
******************************************************************************/

// Waits up to timeoutMs for stdin to have something to read
static bool WaitForInput(unsigned timeoutMs)
{
#ifdef _WIN32
  HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
  DWORD mode;
  switch (GetFileType(handle))
  {
  case FILE_TYPE_CHAR:
    if (GetConsoleMode(handle, &mode))
    {
      // Console: the handle is also signaled by mouse, focus, and key up
      // events, which are discarded. Only a key press means a line is being
      // typed.
      if (WaitForSingleObject(handle, timeoutMs) != WAIT_OBJECT_0)
        return false;
      INPUT_RECORD record;
      DWORD numEvents;
      while (PeekConsoleInputA(handle, &record, 1, &numEvents) && numEvents > 0)
      {
        if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown)
          return true;
        ReadConsoleInputA(handle, &record, 1, &numEvents);
      }
      return false;
    }
    return true;  // character device that is not a console (e.g., NUL): let fgets() report EOF
  case FILE_TYPE_PIPE:
  {
    DWORD available = 0;
    if (!PeekNamedPipe(handle, nullptr, 0, nullptr, &available, nullptr))
      return true;  // broken pipe: let fgets() report EOF
    if (available > 0)
      return true;
    Sleep(timeoutMs);
    return false;
  }
  default:
    return true;    // file
  }
#else
  struct pollfd fd;
  fd.fd = STDIN_FILENO;
  fd.events = POLLIN;
  fd.revents = 0;
  return poll(&fd, 1, int(timeoutMs)) > 0;
#endif
}

static bool StdinAvailable()
{
#ifdef _WIN32
  HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
  return handle != nullptr && handle != INVALID_HANDLE_VALUE && GetFileType(handle) != FILE_TYPE_UNKNOWN;
#else
  return true;
#endif
}

static bool StopRequested(CConsoleInput::State *state)
{
  std::lock_guard<std::mutex> lock(state->mutex);
  return state->stop;
}

static void ReaderThread(std::shared_ptr<CConsoleInput::State> state)
{
  std::string line;
  char buffer[256];
  while (!StopRequested(state.get()))
  {
    if (!WaitForInput(100))
      continue;
    if (fgets(buffer, sizeof(buffer), stdin) == nullptr)
    {
      if (!line.empty())
        state->Deliver(line);
      state->SetEOF();
      break;
    }
    line += buffer;
    if (!line.empty() && line.back() == '\n')
    {
      while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
        line.pop_back();
      state->Deliver(line);
      line.clear();
    }
  }

  std::lock_guard<std::mutex> lock(state->mutex);
  state->exited = true;
}


/******************************************************************************
 CConsoleInput
******************************************************************************/

IConsoleInput *CConsoleInput::CreateInput(void)
{
  std::lock_guard<std::mutex> lock(m_state->mutex);
  m_state->endpoints.emplace_back(std::make_unique<Endpoint>(m_state.get(), [this] { StartReader(); }));
  return m_state->endpoints.back().get();
}

void CConsoleInput::StartReader(void)
{
  // Endpoints may be claimed from different threads, hence call_once
  std::call_once(m_startOnce, [this]
  {
    if (!StdinAvailable())
    {
      InfoLog("Console input unavailable.");
      m_state->SetEOF();
      return;
    }
    m_thread = std::thread(ReaderThread, m_state);
  });
}

CConsoleInput::CConsoleInput(void)
  : m_state(std::make_shared<State>())
{
}

CConsoleInput::~CConsoleInput(void)
{
  if (!m_thread.joinable())
    return;

  {
    std::lock_guard<std::mutex> lock(m_state->mutex);
    m_state->stop = true;
  }

  // Give the thread time to notice. If it is blocked reading a partially
  // typed line, detach it rather than wait for the user.
  for (int i = 0; i < 50; i++)
  {
    {
      std::lock_guard<std::mutex> lock(m_state->mutex);
      if (m_state->exited)
        break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  bool exited;
  {
    std::lock_guard<std::mutex> lock(m_state->mutex);
    exited = m_state->exited;
  }
  if (exited)
    m_thread.join();
  else
    m_thread.detach();
}
