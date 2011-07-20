#include "Supermodel.h"

#ifdef SUPERMODEL_OSX
#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>
#else
#include <SDL.h>
#include <SDL_thread.h>
#endif

CThread *CThread::CreateThread(ThreadStart start, void *startParam)
{
	SDL_Thread *impl = SDL_CreateThread(start, startParam);
	if (impl == NULL)
		return NULL;
	return new CThread(impl);
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

CThread::CThread(void *impl) : m_impl(impl)
{
	//
}

CThread::~CThread()
{
	Kill();
}

UINT32 CThread::GetId()
{
	return SDL_GetThreadID((SDL_Thread*)m_impl);
}

void CThread::Kill()
{
	if (m_impl != NULL)
		SDL_KillThread((SDL_Thread*)m_impl);
	m_impl = NULL;
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
