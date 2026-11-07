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
 * Test_Thread.cpp
 *
 * Tests for the threading primitives in Thread.h, including the way CModel3
 * and CPro1000 use them. Not part of the emulator. To build and run on a UNIX
 * or macOS system, from the top directory:
 *
 *   c++ -std=c++17 -O2 -pthread -ISrc -ISrc/OSD/SDL Src/OSD/Test_Thread.cpp Src/OSD/Thread.cpp -o Test_Thread && ./Test_Thread
 *
 * Adding -fsanitize=thread (or address,undefined) checks the primitives too.
 * The program exits with 0 if all tests pass. A test that does not finish in
 * time is reported as a hang.
 */

#include "OSD/Thread.h"
#include "Supermodel.h"

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

#if !defined(_WIN32)
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#endif

// Thread.cpp reports runaway threads through this
static std::atomic<int> s_errorLogCalls(0);
Result ErrorLog(const char *, ...)
{
  s_errorLogCalls++;
  return Result::FAIL;
}

static int s_failures = 0;
static std::atomic<const char *> s_currentTest(nullptr);
static std::atomic<int64_t> s_deadline(0);

static int64_t NowSeconds()
{
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

static void Check(const char *what, bool ok)
{
  if (!ok)
  {
    printf("  FAILED: %s\n", what);
    s_failures++;
  }
}

static void Test(const char *name, void (*test)(), int timeoutSeconds = 60)
{
  printf("%s\n", name);
  fflush(stdout);
  int before = s_failures;
  s_currentTest = name;
  s_deadline = NowSeconds() + timeoutSeconds;
  test();
  s_deadline = 0;
  printf("  %s\n", s_failures == before ? "passed" : "FAILED");
  fflush(stdout);
}

/******************************************************************************
 Threads
******************************************************************************/

static int ReturnParam(void *param)
{
  return *(int *) param;
}

static void TestThreadStatus()
{
  int value = 1234;
  CThread *thread = CThread::CreateThread("Status", ReturnParam, &value);
  Check("thread created", thread != nullptr);
  Check("name is kept", thread->GetName() == "Status");
  Check("Wait() returns the thread's result", thread->Wait() == 1234);
  Check("second Wait() returns -1", thread->Wait() == -1);
  delete thread;
}

static void TestManyThreads()
{
  std::vector<CThread *> threads;
  std::vector<int> values(64);
  for (size_t i = 0; i < values.size(); i++)
  {
    values[i] = int(i);
    threads.push_back(CThread::CreateThread("Many", ReturnParam, &values[i]));
    Check("thread created", threads.back() != nullptr);
  }
  for (size_t i = 0; i < threads.size(); i++)
  {
    Check("each thread gets its own parameter", threads[i]->Wait() == int(i));
    delete threads[i];
  }
}

static int WaitForFlag(void *param)
{
  auto *flag = (std::atomic<bool> *) param;
  while (!flag->load())
    CThread::Sleep(1);
  return 0;
}

static void TestRunawayThread()
{
  // Deleting a thread that was not waited on reports an error and must not
  // end the program. The thread keeps running and must be able to finish.
  static std::atomic<bool> flag(false);
  int errorsBefore = s_errorLogCalls;
  CThread *thread = CThread::CreateThread("Runaway", WaitForFlag, &flag);
  Check("thread created", thread != nullptr);
  delete thread;
  Check("an error is logged", s_errorLogCalls == errorsBefore + 1);
  flag = true;
  CThread::Sleep(50);
}

static void TestSleepAndTicks()
{
  UINT32 start = CThread::GetTicks();
  CThread::Sleep(100);
  UINT32 elapsed = CThread::GetTicks() - start;
  Check("Sleep(100) takes at least about 100 ms", elapsed >= 99);
  Check("Sleep(100) doesn't take far longer", elapsed < 2000);
  UINT32 last = CThread::GetTicks();
  bool monotonic = true;
  for (int i = 0; i < 100000; i++)
  {
    UINT32 now = CThread::GetTicks();
    monotonic = monotonic && now >= last;
    last = now;
  }
  Check("GetTicks() never goes backward", monotonic);
}

#if !defined(_WIN32)
static std::atomic<int> s_blockedSignals(0);

static int CountBlockedSignals(void *)
{
  sigset_t set;
  pthread_sigmask(SIG_BLOCK, nullptr, &set);
  int blocked = 0;
  for (int sig : { SIGHUP, SIGINT, SIGQUIT, SIGPIPE, SIGALRM, SIGTERM, SIGCHLD, SIGWINCH, SIGVTALRM, SIGPROF })
    blocked += sigismember(&set, sig) == 1 ? 1 : 0;
  s_blockedSignals = blocked;
  return 0;
}

static void TestSignalsBlocked()
{
  CThread *thread = CThread::CreateThread("Signals", CountBlockedSignals, nullptr);
  thread->Wait();
  delete thread;
  Check("new threads leave asynchronous signals to the main thread", s_blockedSignals == 10);
  sigset_t set;
  pthread_sigmask(SIG_BLOCK, nullptr, &set);
  Check("the creating thread's signals are unchanged", sigismember(&set, SIGINT) == 0);
}

static char s_threadName[64];

static int ReadOwnName(void *)
{
  pthread_getname_np(pthread_self(), s_threadName, sizeof(s_threadName));
  return 0;
}

static void TestThreadNaming()
{
  CThread *thread = CThread::CreateThread("SoundBoard", ReadOwnName, nullptr);
  thread->Wait();
  delete thread;
  Check("the system sees the thread's name", std::string(s_threadName) == "SoundBoard");
}
#endif

/******************************************************************************
 Mutexes
******************************************************************************/

struct Counter
{
  CMutex *mutex;
  int value;
  int perThread;
};

static int Increment(void *param)
{
  Counter *counter = (Counter *) param;
  for (int i = 0; i < counter->perThread; i++)
  {
    if (!counter->mutex->Lock())
      return 1;
    counter->value++;
    if (!counter->mutex->Unlock())
      return 1;
  }
  return 0;
}

static void TestMutualExclusion()
{
  Counter counter { CThread::CreateMutex(), 0, 200000 };
  std::vector<CThread *> threads;
  for (int i = 0; i < 8; i++)
    threads.push_back(CThread::CreateThread("Lock", Increment, &counter));
  for (CThread *thread : threads)
  {
    Check("no lock error", thread->Wait() == 0);
    delete thread;
  }
  Check("no increment was lost", counter.value == 8 * counter.perThread);
  delete counter.mutex;
}

static int TryToEnter(void *param)
{
  Counter *counter = (Counter *) param;
  counter->mutex->Lock();
  counter->value = 1;
  counter->mutex->Unlock();
  return 0;
}

static void TestRecursiveLocking()
{
  // Previous implementations allowed this and CMutex users may rely on it
  Counter counter { CThread::CreateMutex(), 0, 0 };
  Check("first lock", counter.mutex->Lock());
  Check("second lock by the owner", counter.mutex->Lock());
  CThread *other = CThread::CreateThread("Other", TryToEnter, &counter);
  CThread::Sleep(50);
  Check("another thread can't enter while held twice", counter.value == 0);
  Check("first unlock", counter.mutex->Unlock());
  CThread::Sleep(50);
  Check("another thread can't enter while still held once", counter.value == 0);
  Check("second unlock", counter.mutex->Unlock());
  other->Wait();
  Check("another thread enters after the last unlock", counter.value == 1);
  delete other;
  delete counter.mutex;
}

/******************************************************************************
 Semaphores
******************************************************************************/

static void TestSemaphoreCounting()
{
  CSemaphore *sem = CThread::CreateSemaphore(3);
  Check("initial value", sem->GetValue() == 3);
  Check("wait", sem->Wait());
  Check("value after wait", sem->GetValue() == 2);
  Check("post", sem->Post());
  Check("post", sem->Post());
  Check("value after posts", sem->GetValue() == 4);
  for (int i = 0; i < 4; i++)
    Check("wait without blocking", sem->Wait());
  Check("value at the end", sem->GetValue() == 0);
  delete sem;
}

struct SemaphoreWork
{
  CSemaphore *items;
  std::atomic<int> taken;
  int total;
};

static int Consume(void *param)
{
  SemaphoreWork *work = (SemaphoreWork *) param;
  for (;;)
  {
    if (!work->items->Wait())
      return 1;
    if (work->taken.fetch_add(1) >= work->total)
      return 0;   // the extra posts at the end tell the consumers to stop
  }
}

static void TestSemaphoreBlocksAndWakes()
{
  SemaphoreWork work;
  work.items = CThread::CreateSemaphore(0);
  work.taken = 0;
  work.total = 500000;
  std::vector<CThread *> threads;
  const int consumers = 4;
  for (int i = 0; i < consumers; i++)
    threads.push_back(CThread::CreateThread("Consumer", Consume, &work));
  for (int i = 0; i < work.total + consumers; i++)
    Check("post", work.items->Post());
  for (CThread *thread : threads)
  {
    Check("no wait error", thread->Wait() == 0);
    delete thread;
  }
  Check("every post was consumed exactly once", work.taken == work.total + consumers);
  delete work.items;
}

static int WaitThenExit(void *param)
{
  return ((CSemaphore *) param)->Wait() ? 0 : 1;
}

static void TestDestroySemaphoreAfterWake()
{
  // As CModel3::StopThreads() does: post, wait for the thread, destroy
  for (int i = 0; i < 20000; i++)
  {
    CSemaphore *sem = CThread::CreateSemaphore(0);
    CThread *thread = CThread::CreateThread("Stop", WaitThenExit, sem);
    sem->Post();
    Check("thread woke up", thread->Wait() == 0);
    delete thread;
    delete sem;
  }
}

/******************************************************************************
 Condition variables
******************************************************************************/

struct Waiters
{
  CMutex *mutex;
  CCondVar *cond;
  int generation;
  int woken;
};

static int WaitForGeneration(void *param)
{
  Waiters *w = (Waiters *) param;
  w->mutex->Lock();
  while (w->generation == 0)
    if (!w->cond->Wait(w->mutex))
      return 1;
  w->woken++;
  w->mutex->Unlock();
  return 0;
}

static void TestSignalWakesOneAndBroadcastWakesAll()
{
  Waiters w { CThread::CreateMutex(), CThread::CreateCondVar(), 0, 0 };
  std::vector<CThread *> threads;
  for (int i = 0; i < 8; i++)
    threads.push_back(CThread::CreateThread("Waiter", WaitForGeneration, &w));
  CThread::Sleep(100);    // let them all wait

  // Waiting must release the mutex, or this would block forever
  Check("mutex is free while waiting", w.mutex->Lock());
  Check("nobody woke up yet", w.woken == 0);
  w.mutex->Unlock();

  w.mutex->Lock();
  w.generation = 1;
  w.mutex->Unlock();
  Check("signal", w.cond->Signal());
  CThread::Sleep(100);
  w.mutex->Lock();
  Check("signal wakes at least one waiter", w.woken >= 1);
  w.mutex->Unlock();

  Check("signal all", w.cond->SignalAll());
  for (CThread *thread : threads)
  {
    Check("no wait error", thread->Wait() == 0);
    delete thread;
  }
  Check("signal all wakes every waiter", w.woken == 8);
  delete w.cond;
  delete w.mutex;
}

static void TestSignalWithoutWaiters()
{
  CCondVar *cond = CThread::CreateCondVar();
  Check("signal with nobody waiting", cond->Signal());
  Check("signal all with nobody waiting", cond->SignalAll());
  delete cond;
}

/******************************************************************************
 The frame loop of CModel3 and CPro1000
******************************************************************************/

// The main thread runs frames, each board thread waits on its own semaphore
// for the next frame, works, flags that it is done and signals the main thread.
// Pausing waits for the boards to stop at the top of their loops. Stopping
// tells them to exit, wakes them and waits for them.
struct FrameLoop
{
  static const int NUM_BOARDS = 4;
  CThread *threads[NUM_BOARDS];
  CSemaphore *sync[NUM_BOARDS];
  CMutex *notifyLock;
  CCondVar *notifySync;
  bool done[NUM_BOARDS];
  bool running[NUM_BOARDS];
  bool pause;
  bool stop;
  uint64_t work[NUM_BOARDS];
  uint64_t frameWork;
  int board[NUM_BOARDS];
  bool jitter;
};

static FrameLoop s_loop;

static int BoardThread(void *param)
{
  int board = *(int *) param;
  FrameLoop &loop = s_loop;
  unsigned seed = unsigned(board) * 2654435761u + 1;
  for (;;)
  {
    if (!loop.sync[board]->Wait())
      return 1;

    if (!loop.notifyLock->Lock())
      return 1;
    while (loop.pause && !loop.stop)
    {
      loop.running[board] = false;
      loop.notifySync->SignalAll();
      if (!loop.notifySync->Wait(loop.notifyLock))
        return 1;
    }
    if (loop.stop)
    {
      loop.running[board] = false;
      loop.notifySync->SignalAll();
      loop.notifyLock->Unlock();
      return 0;
    }
    loop.running[board] = true;
    uint64_t frameWork = loop.frameWork;
    if (!loop.notifyLock->Unlock())
      return 1;

    // Not protected: only the main thread reads this, after this board is done
    loop.work[board] += frameWork;
    if (loop.jitter)
    {
      seed = seed * 1103515245u + 12345u;
      if ((seed >> 16) % 64 == 0)
        std::this_thread::yield();
    }

    if (!loop.notifyLock->Lock())
      return 1;
    loop.done[board] = true;
    loop.running[board] = false;
    if (!loop.notifySync->SignalAll())
      return 1;
    if (!loop.notifyLock->Unlock())
      return 1;
  }
}

static bool StartLoop(bool jitter)
{
  s_loop.notifyLock = CThread::CreateMutex();
  s_loop.notifySync = CThread::CreateCondVar();
  s_loop.pause = false;
  s_loop.stop = false;
  s_loop.frameWork = 0;
  s_loop.jitter = jitter;
  for (int i = 0; i < FrameLoop::NUM_BOARDS; i++)
  {
    s_loop.board[i] = i;
    s_loop.done[i] = false;
    s_loop.running[i] = false;
    s_loop.work[i] = 0;
    s_loop.sync[i] = CThread::CreateSemaphore(0);
    s_loop.threads[i] = CThread::CreateThread("Board", BoardThread, &s_loop.board[i]);
    if (s_loop.threads[i] == nullptr)
      return false;
  }
  return true;
}

static bool RunLoopFrame(uint64_t frame)
{
  s_loop.notifyLock->Lock();
  s_loop.frameWork = frame;
  s_loop.notifyLock->Unlock();
  for (int i = 0; i < FrameLoop::NUM_BOARDS; i++)
    if (!s_loop.sync[i]->Post())
      return false;
  if (!s_loop.notifyLock->Lock())
    return false;
  for (int i = 0; i < FrameLoop::NUM_BOARDS; i++)
  {
    while (!s_loop.done[i])
      if (!s_loop.notifySync->Wait(s_loop.notifyLock))
        return false;
  }
  for (int i = 0; i < FrameLoop::NUM_BOARDS; i++)
    s_loop.done[i] = false;
  return s_loop.notifyLock->Unlock();
}

static void PauseLoop()
{
  s_loop.notifyLock->Lock();
  s_loop.pause = true;
  for (int i = 0; i < FrameLoop::NUM_BOARDS; i++)
    while (s_loop.running[i])
      s_loop.notifySync->Wait(s_loop.notifyLock);
  s_loop.notifyLock->Unlock();
}

static void ResumeLoop()
{
  s_loop.notifyLock->Lock();
  s_loop.pause = false;
  s_loop.notifySync->SignalAll();
  s_loop.notifyLock->Unlock();
}

static void StopLoop()
{
  s_loop.notifyLock->Lock();
  s_loop.stop = true;
  s_loop.notifySync->SignalAll();
  s_loop.notifyLock->Unlock();
  for (int i = 0; i < FrameLoop::NUM_BOARDS; i++)
  {
    s_loop.sync[i]->Post();
    Check("board thread exits cleanly", s_loop.threads[i]->Wait() == 0);
    delete s_loop.threads[i];
    delete s_loop.sync[i];
  }
  delete s_loop.notifySync;
  delete s_loop.notifyLock;
}

static void TestFrameLoop(bool jitter, uint64_t frames)
{
  Check("threads started", StartLoop(jitter));
  uint64_t expected = 0;
  bool ok = true;
  for (uint64_t frame = 1; frame <= frames; frame++)
  {
    ok = ok && RunLoopFrame(frame);
    expected += frame;
  }
  Check("no thread error during frames", ok);
  for (int i = 0; i < FrameLoop::NUM_BOARDS; i++)
    Check("every board did every frame exactly once", s_loop.work[i] == expected);
  StopLoop();
}

static void TestFrameLoopFast()
{
  TestFrameLoop(false, 200000);
}

static void TestFrameLoopJitter()
{
  TestFrameLoop(true, 100000);
}

static void TestPauseAndResume()
{
  // Pausing between frames, as a save state does
  Check("threads started", StartLoop(true));
  uint64_t expected = 0;
  bool ok = true;
  for (uint64_t frame = 1; frame <= 20000; frame++)
  {
    ok = ok && RunLoopFrame(frame);
    expected += frame;
    if (frame % 7 == 0)
    {
      PauseLoop();
      bool anyRunning = false;
      s_loop.notifyLock->Lock();
      for (int i = 0; i < FrameLoop::NUM_BOARDS; i++)
        anyRunning = anyRunning || s_loop.running[i];
      s_loop.notifyLock->Unlock();
      ok = ok && !anyRunning;
      ResumeLoop();
    }
  }
  Check("no thread error, and nothing runs while paused", ok);
  for (int i = 0; i < FrameLoop::NUM_BOARDS; i++)
    Check("every board did every frame exactly once", s_loop.work[i] == expected);
  StopLoop();
}

static void TestStartAndStopRepeatedly()
{
  // Starting and stopping the threads, as switching games or resetting does
  for (int cycle = 0; cycle < 2000; cycle++)
  {
    Check("threads started", StartLoop(false));
    for (uint64_t frame = 1; frame <= uint64_t(cycle % 5); frame++)
      Check("frame ran", RunLoopFrame(frame));
    StopLoop();
  }
}

// SDL's audio thread calls CModel3::AudioCallback, which wakes the sound board
// thread from a thread that CThread did not create
static std::atomic<int> s_foreignDone(0);

static void TestForeignThreadSignals()
{
  Waiters w { CThread::CreateMutex(), CThread::CreateCondVar(), 0, 0 };
  CThread *waiter = CThread::CreateThread("Waiter", WaitForGeneration, &w);
  std::thread foreign([&]
  {
    CThread::Sleep(50);
    w.mutex->Lock();
    w.generation = 1;
    w.cond->Signal();
    w.mutex->Unlock();
    s_foreignDone = 1;
  });
  Check("waiter woke up", waiter->Wait() == 0);
  foreign.join();
  Check("foreign thread finished", s_foreignDone == 1);
  Check("waiter ran", w.woken == 1);
  delete waiter;
  delete w.cond;
  delete w.mutex;
}

/******************************************************************************
 Main
******************************************************************************/

int main()
{
  // A hang can't be reported by the test that hangs
  std::thread([]
  {
    for (;;)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      int64_t deadline = s_deadline;
      if (deadline != 0 && NowSeconds() > deadline)
      {
        printf("HANG in test: %s\n", s_currentTest.load());
        fflush(stdout);
        _Exit(2);
      }
    }
  }).detach();

  Test("Thread returns its status", TestThreadStatus);
  Test("Many threads", TestManyThreads);
  Test("Runaway thread", TestRunawayThread);
  Test("Sleep and ticks", TestSleepAndTicks);
#if !defined(_WIN32)
  Test("Asynchronous signals blocked", TestSignalsBlocked);
  Test("Thread naming", TestThreadNaming);
#endif
  Test("Mutual exclusion", TestMutualExclusion);
  Test("Recursive locking", TestRecursiveLocking);
  Test("Semaphore counting", TestSemaphoreCounting);
  Test("Semaphore blocks and wakes", TestSemaphoreBlocksAndWakes);
  Test("Semaphore destroyed after wake", TestDestroySemaphoreAfterWake, 180);
  Test("Signal and broadcast", TestSignalWakesOneAndBroadcastWakesAll);
  Test("Signal without waiters", TestSignalWithoutWaiters);
  Test("Frame loop", TestFrameLoopFast, 180);
  Test("Frame loop with jitter", TestFrameLoopJitter, 180);
  Test("Pause and resume", TestPauseAndResume, 180);
  Test("Start and stop repeatedly", TestStartAndStopRepeatedly, 180);
  Test("Signals from a foreign thread", TestForeignThreadSignals);

  s_deadline = 0;
  printf("%s: %d failed checks\n", s_failures == 0 ? "ALL PASSED" : "FAILED", s_failures);
  return s_failures == 0 ? 0 : 1;
}
