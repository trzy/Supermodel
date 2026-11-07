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
 * Thread.h
 *
 * What the standard library does not provide for threads: naming them, leaving
 * signals to the main thread and, before C++20, semaphores.
 */

#ifndef INCLUDED_THREADS_H
#define INCLUDED_THREADS_H

#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace Thread
{
  /*
   * SetCurrentName
   *
   * Names the calling thread, for debuggers and system tools. Every thread
   * should call this first, unless created by Start(). Linux keeps only the
   * first 15 characters.
   */
  void SetCurrentName(const std::string &name);

  /*
   * Start
   *
   * Starts a named thread for emulation work, which leaves asynchronous signals
   * to the main thread. Throws std::system_error if the thread can't be created.
   */
  std::thread Start(const std::string &name, std::function<void()> body);

  /*
   * Semaphore
   *
   * Counts wake-ups so that one posted before the other thread waits isn't lost.
   */
  class Semaphore
  {
  public:
    void Wait()
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_available.wait(lock, [this] { return m_value > 0; });
      m_value--;
    }

    void Post()
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_value++;
      m_available.notify_one();
    }

  private:
    std::mutex m_mutex;
    std::condition_variable m_available;
    unsigned m_value = 0;
  };
}

#endif  // INCLUDED_THREADS_H
