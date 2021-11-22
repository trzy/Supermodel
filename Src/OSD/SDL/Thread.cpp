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
 * Thread.cpp
 *
 * SDL-based implementation of threading primitives.
 */

#include "Thread.h"

#include "Supermodel.h"
#include "SDLIncludes.h"

void CThread::Sleep(UINT32 ms)
{
	SDL_Delay(ms);
}

UINT32 CThread::GetTicks()
{
	return SDL_GetTicks();
}

CThread *CThread::CreateThread(const std::string &name, ThreadStart start, void *startParam)
{
	SDL_Thread *impl = SDL_CreateThread(start, name.c_str(), startParam);
	if (impl == NULL)
		return NULL;
	return new CThread(name, impl);
}

CSemaphore *CThread::CreateSemaphore(UINT32 initVal)
{
	SDL_sem *impl = SDL_CreateSemaphore(initVal);
	if (impl == NULL)
		return NULL;
	return new CSemaphore(impl);
}

CCondVar *CThread::CreateCondVar()
{
	SDL_cond *impl = SDL_CreateCond();
	if (impl == NULL)
		return NULL;
	return new CCondVar(impl);
}

CMutex *CThread::CreateMutex()
{
	SDL_mutex *impl = SDL_CreateMutex();
	if (impl == NULL)
		return NULL;
	return new CMutex(impl);
}

const char *CThread::GetLastError()
{
	return SDL_GetError();
}

CThread::CThread(const std::string &name, void *impl)
  : m_name(name),
    m_impl(impl)

{
	//
}

CThread::~CThread()
{
  // User should have called Wait() before thread object is destroyed
  if (nullptr != m_impl)
  {
    ErrorLog("Runaway thread error. A thread was not properly halted: %s\n", GetName().c_str());
  }
}

const std::string &CThread::GetName() const
{
  return m_name;
}

UINT32 CThread::GetId()
{
	return SDL_GetThreadID((SDL_Thread*)m_impl);
}

int CThread::Wait()
{
	int status;
	if (m_impl == NULL)
		return -1;
	SDL_WaitThread((SDL_Thread*)m_impl, &status);
	m_impl = NULL;
	return status;
}

CSemaphore::CSemaphore(void *impl) : m_impl(impl)
{
	//
}

CSemaphore::~CSemaphore()
{
	SDL_DestroySemaphore((SDL_sem*)m_impl);
}

UINT32 CSemaphore::GetValue()
{
	return SDL_SemValue((SDL_sem*)m_impl);
}

bool CSemaphore::Wait()
{
	return SDL_SemWait((SDL_sem*)m_impl) == 0;
}

bool CSemaphore::Post()
{
	return SDL_SemPost((SDL_sem*)m_impl) == 0;
}

CCondVar::CCondVar(void *impl) : m_impl(impl)
{
	//
}

CCondVar::~CCondVar()
{
	SDL_DestroyCond((SDL_cond*)m_impl);
}

bool CCondVar::Wait(CMutex *mutex)
{
	return SDL_CondWait((SDL_cond*)m_impl, (SDL_mutex*)mutex->m_impl) == 0;
}

bool CCondVar::Signal()
{
	return SDL_CondSignal((SDL_cond*)m_impl) == 0;
}

bool CCondVar::SignalAll()
{
	return SDL_CondBroadcast((SDL_cond*)m_impl) == 0;
}

CMutex::CMutex(void *impl) : m_impl(impl)
{
	//
}

CMutex::~CMutex()
{
	SDL_DestroyMutex((SDL_mutex*)m_impl);
}

bool CMutex::Lock()
{
	return SDL_mutexP((SDL_mutex*)m_impl) == 0;
}

bool CMutex::Unlock()
{
	return SDL_mutexV((SDL_mutex*)m_impl) == 0;
}
