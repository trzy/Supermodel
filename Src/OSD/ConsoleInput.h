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
 * ConsoleInput.h
 *
 * Shared console (stdin) input. A single background thread reads lines from
 * stdin and delivers them to one of several consumers (e.g., the debugger and
 * an emulated serial port), so that only one reader ever touches stdin.
 *
 * Each consumer is given an IConsoleInput endpoint. Endpoints claim and
 * release the console; each new line goes to the endpoint with the most
 * recent active claim. Claims are counted, so an endpoint keeps its claim
 * until it has released as many times as it claimed. A line read while no
 * endpoint holds a claim is discarded.
 *
 * The reader thread is started by the first claim, not when CConsoleInput is
 * created. Until then nothing reads stdin and anything typed or piped in
 * stays buffered by the OS, so input that arrives during startup (e.g.,
 * before the emulator or debugger exists to claim it) is not lost. Once
 * started, the thread runs until CConsoleInput is destroyed, even when all
 * claims have been released.
 */

#ifndef INCLUDED_CONSOLEINPUT_H
#define INCLUDED_CONSOLEINPUT_H

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

/*
 * IConsoleInput:
 *
 * A consumer's view of console input.
 */
class IConsoleInput
{
public:
  /*
   * Claim(void):
   *
   * Requests that new lines be delivered to this endpoint. The most recent
   * claim takes precedence over earlier ones until it is released. The first
   * claim made on any endpoint starts the reader thread.
   */
  virtual void Claim(void) = 0;

  /*
   * Release(void):
   *
   * Releases one claim made by this endpoint.
   */
  virtual void Release(void) = 0;

  /*
   * ReadLine(line, wait):
   *
   * Takes the next line delivered to this endpoint, without the line
   * terminator.
   *
   * Parameters:
   *    line  Receives the line.
   *    wait  If true, blocks until a line arrives. Returns false without
   *          waiting if this endpoint is not the current receiver of lines,
   *          and returns false if console input has ended (EOF).
   *
   * Returns:
   *    True if a line was returned.
   */
  virtual bool ReadLine(std::string *line, bool wait) = 0;

  virtual ~IConsoleInput()
  {
  }
};

/*
 * CConsoleInputClaim:
 *
 * Claims an endpoint for the lifetime of the object.
 */
class CConsoleInputClaim
{
public:
  CConsoleInputClaim(IConsoleInput *input)
    : m_input(input)
  {
    if (m_input)
      m_input->Claim();
  }

  ~CConsoleInputClaim()
  {
    if (m_input)
      m_input->Release();
  }

  CConsoleInputClaim(const CConsoleInputClaim &) = delete;
  CConsoleInputClaim &operator=(const CConsoleInputClaim &) = delete;

private:
  IConsoleInput *m_input;
};

/*
 * CConsoleInput:
 *
 * Owns stdin and the reader thread, which is started by the first claim on
 * any of its endpoints. Must outlive all users of the endpoints it creates.
 */
class CConsoleInput
{
public:
  /*
   * CreateInput(void):
   *
   * Creates a new endpoint. The endpoint is owned by this object.
   */
  IConsoleInput *CreateInput(void);

  CConsoleInput(void);
  ~CConsoleInput(void);

  struct State;

private:
  void StartReader(void);           // starts the reader thread (once); called on each claim

  std::shared_ptr<State> m_state;   // shared with reader thread, which may outlive this object if detached
  std::thread m_thread;
  std::once_flag m_startOnce;
};

#endif  // INCLUDED_CONSOLEINPUT_H
