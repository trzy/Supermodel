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
 * Thread.cpp (libretro)
 *
 * Implementation of the OSD thread interface (OSD/Thread.h) on top of the C++
 * standard library, so that the libretro core does not depend on SDL. Only
 * C++17 facilities are used; this builds with GCC, MinGW-w64 (posix thread
 * model), Clang and MSVC.
 */

#include "OSD/Thread.h"
#include "OSD/Logger.h"

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace
{
  struct ThreadImpl
  {
    std::thread thread;
    int status = 0;
  };

  struct SemaphoreImpl
  {
    std::mutex mutex;
    std::condition_variable cond;
    UINT32 count = 0;
  };

  const std::chrono::steady_clock::time_point s_startTime = std::chrono::steady_clock::now();
}

void CThread::Sleep(UINT32 ms)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

UINT32 CThread::GetTicks()
{
  auto elapsed = std::chrono::steady_clock::now() - s_startTime;
  return (UINT32) std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
}

CThread *CThread::CreateThread(const std::string &name, ThreadStart start, void *startParam)
{
  ThreadImpl *impl = new ThreadImpl();
  try
  {
    impl->thread = std::thread([impl, start, startParam]() { impl->status = start(startParam); });
  }
  catch (...)
  {
    delete impl;
    return nullptr;
  }
  return new CThread(name, impl);
}

CSemaphore *CThread::CreateSemaphore(UINT32 initVal)
{
  SemaphoreImpl *impl = new SemaphoreImpl();
  impl->count = initVal;
  return new CSemaphore(impl);
}

CCondVar *CThread::CreateCondVar()
{
  return new CCondVar(new std::condition_variable());
}

CMutex *CThread::CreateMutex()
{
  return new CMutex(new std::mutex());
}

const char *CThread::GetLastError()
{
  return "thread error";
}

CThread::CThread(const std::string &name, void *impl)
  : m_name(name),
    m_impl(impl)
{
}

CThread::~CThread()
{
  // User should have called Wait() before thread object is destroyed
  if (nullptr != m_impl)
  {
    ErrorLog("Runaway thread error. A thread was not properly halted: %s\n", GetName().c_str());
    ThreadImpl *impl = static_cast<ThreadImpl *>(m_impl);
    if (impl->thread.joinable())
      impl->thread.detach();
    delete impl;
  }
}

const std::string &CThread::GetName() const
{
  return m_name;
}

UINT32 CThread::GetId()
{
  if (m_impl == nullptr)
    return 0;
  ThreadImpl *impl = static_cast<ThreadImpl *>(m_impl);
  return (UINT32) std::hash<std::thread::id>()(impl->thread.get_id());
}

int CThread::Wait()
{
  if (m_impl == nullptr)
    return -1;
  ThreadImpl *impl = static_cast<ThreadImpl *>(m_impl);
  if (impl->thread.joinable())
    impl->thread.join();
  int status = impl->status;
  delete impl;
  m_impl = nullptr;
  return status;
}

CSemaphore::CSemaphore(void *impl) : m_impl(impl)
{
}

CSemaphore::~CSemaphore()
{
  delete static_cast<SemaphoreImpl *>(m_impl);
}

UINT32 CSemaphore::GetValue()
{
  SemaphoreImpl *impl = static_cast<SemaphoreImpl *>(m_impl);
  std::lock_guard<std::mutex> lock(impl->mutex);
  return impl->count;
}

bool CSemaphore::Wait()
{
  SemaphoreImpl *impl = static_cast<SemaphoreImpl *>(m_impl);
  std::unique_lock<std::mutex> lock(impl->mutex);
  impl->cond.wait(lock, [impl]() { return impl->count > 0; });
  --impl->count;
  return true;
}

bool CSemaphore::Post()
{
  SemaphoreImpl *impl = static_cast<SemaphoreImpl *>(m_impl);
  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    ++impl->count;
  }
  impl->cond.notify_one();
  return true;
}

CCondVar::CCondVar(void *impl) : m_impl(impl)
{
}

CCondVar::~CCondVar()
{
  delete static_cast<std::condition_variable *>(m_impl);
}

bool CCondVar::Wait(CMutex *mutex)
{
  // The caller already holds the mutex (CMutex::Lock()); adopt it for the
  // duration of the wait and give ownership back afterwards
  std::mutex *m = static_cast<std::mutex *>(mutex->m_impl);
  std::unique_lock<std::mutex> lock(*m, std::adopt_lock);
  static_cast<std::condition_variable *>(m_impl)->wait(lock);
  lock.release();
  return true;
}

bool CCondVar::Signal()
{
  static_cast<std::condition_variable *>(m_impl)->notify_one();
  return true;
}

bool CCondVar::SignalAll()
{
  static_cast<std::condition_variable *>(m_impl)->notify_all();
  return true;
}

CMutex::CMutex(void *impl) : m_impl(impl)
{
}

CMutex::~CMutex()
{
  delete static_cast<std::mutex *>(m_impl);
}

bool CMutex::Lock()
{
  static_cast<std::mutex *>(m_impl)->lock();
  return true;
}

bool CMutex::Unlock()
{
  static_cast<std::mutex *>(m_impl)->unlock();
  return true;
}
