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
 * Tests for Thread.h, and for the way CModel3 and CPro1000 synchronize their
 * threads. Not part of the emulator. To build and run on a UNIX or macOS system,
 * from the top directory:
 *
 *   c++ -std=c++17 -O2 -pthread -ISrc Src/OSD/Test_Thread.cpp Src/OSD/Thread.cpp -o Test_Thread && ./Test_Thread
 *
 * Adding -fsanitize=thread (or address,undefined) checks the patterns too. The
 * program exits with 0 if all tests pass. A test that does not finish in time is
 * reported as a hang.
 */

#include "OSD/Thread.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

#if !defined(_WIN32)
#include <pthread.h>
#include <signal.h>
#endif

static int s_failures = 0;
static std::atomic<const char *> s_currentTest(nullptr);
static std::atomic<int64_t> s_deadline(0);

static int64_t NowSeconds()
{
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

static void SleepMs(int ms)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
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
 Starting threads
******************************************************************************/

static void TestStartRunsBody()
{
  int value = 0;
  std::thread thread = Thread::Start("Body", [&] { value = 1234; });
  Check("thread is joinable", thread.joinable());
  thread.join();
  Check("the body ran", value == 1234);
}

static void TestManyThreads()
{
  std::vector<int> values(64, -1);
  std::vector<std::thread> threads;
  for (size_t i = 0; i < values.size(); i++)
    threads.push_back(Thread::Start("Many", [&values, i] { values[i] = int(i); }));
  for (std::thread &thread : threads)
    thread.join();
  for (size_t i = 0; i < values.size(); i++)
    Check("each thread ran its own body", values[i] == int(i));
}

#if !defined(_WIN32)
static void TestSignalsBlocked()
{
  int blocked = 0;
  std::thread thread = Thread::Start("Signals", [&]
  {
    sigset_t set;
    pthread_sigmask(SIG_BLOCK, nullptr, &set);
    for (int sig : { SIGHUP, SIGINT, SIGQUIT, SIGPIPE, SIGALRM, SIGTERM, SIGCHLD, SIGWINCH, SIGVTALRM, SIGPROF })
      blocked += sigismember(&set, sig) == 1 ? 1 : 0;
  });
  thread.join();
  Check("started threads leave asynchronous signals to the main thread", blocked == 10);
  sigset_t set;
  pthread_sigmask(SIG_BLOCK, nullptr, &set);
  Check("the creating thread's signals are unchanged", sigismember(&set, SIGINT) == 0);
}

static std::string ReadOwnName()
{
  char name[64] = {};
  pthread_getname_np(pthread_self(), name, sizeof(name));
  return name;
}

static void TestNaming()
{
  std::string fromStart;
  std::thread started = Thread::Start("SoundBoard", [&] { fromStart = ReadOwnName(); });
  started.join();
  Check("Start() names the thread", fromStart == "SoundBoard");

  std::string fromPlain;
  std::thread plain([&]
  {
    Thread::SetCurrentName("NetServer");
    fromPlain = ReadOwnName();
  });
  plain.join();
  Check("SetCurrentName() names a plain std::thread", fromPlain == "NetServer");

  std::string longName;
  std::thread trimmed([&]
  {
    Thread::SetCurrentName("SimNetBoardConnect");
    longName = ReadOwnName();
  });
  trimmed.join();
#if defined(__linux__)
  Check("a long name is cut to fit on Linux", longName == "SimNetBoardConn");
#else
  Check("a long name is kept", longName == "SimNetBoardConnect");
#endif
}
#endif

/******************************************************************************
 Semaphores
******************************************************************************/

static void TestSemaphoreCounts()
{
  // Posts made while nobody waits are kept. If not, each Wait() would hang.
  Thread::Semaphore semaphore;
  for (int i = 0; i < 4; i++)
    semaphore.Post();
  for (int i = 0; i < 4; i++)
    semaphore.Wait();
}

static void TestSemaphoreBlocks()
{
  Thread::Semaphore semaphore;
  std::atomic<bool> woke(false);
  std::thread waiter = Thread::Start("Waiter", [&]
  {
    semaphore.Wait();
    woke = true;
  });
  SleepMs(100);
  Check("Wait() blocks until a post", !woke);
  semaphore.Post();
  waiter.join();
  Check("Post() wakes the waiter", woke);
}

static void TestEachPostConsumedOnce()
{
  Thread::Semaphore items;
  std::atomic<int> taken(0);
  const int total = 500000;
  const int consumers = 4;
  std::vector<std::thread> threads;
  for (int i = 0; i < consumers; i++)
  {
    threads.push_back(Thread::Start("Consumer", [&]
    {
      for (;;)
      {
        items.Wait();
        if (taken.fetch_add(1) >= total)
          return;   // the extra posts at the end tell the consumers to stop
      }
    }));
  }
  for (int i = 0; i < total + consumers; i++)
    items.Post();
  for (std::thread &thread : threads)
    thread.join();
  Check("every post was consumed exactly once", taken == total + consumers);
}

static void TestDestroySemaphoreAfterWake()
{
  // A waiter can return while Post() is finishing. Destroying the semaphore
  // right after must be safe, which the sanitizers would catch if it isn't.
  for (int i = 0; i < 20000; i++)
  {
    std::unique_ptr<Thread::Semaphore> semaphore(new Thread::Semaphore());
    std::thread thread([&] { semaphore->Wait(); });
    semaphore->Post();
    thread.join();
  }
}

/******************************************************************************
 The frame loop of CModel3 and CPro1000
******************************************************************************/

// The main thread runs frames. Each board thread waits on its own semaphore for
// the next frame, works, flags that it is done and signals the main thread.
// Pausing waits for the boards to finish their work. Stopping also tells them to
// exit, wakes them and joins them. These use the same order of operations as the
// emulator, so a mistake made there can be tried here with the sanitizers.
class FrameLoop
{
public:
  static const int NUM_BOARDS = 4;

  explicit FrameLoop(bool jitter)
    : m_jitter(jitter)
  {
    for (int board = 0; board < NUM_BOARDS; board++)
      m_threads[board] = Thread::Start("Board", [this, board] { RunBoard(board); });
  }

  void RunFrame(uint64_t frame)
  {
    {
      std::lock_guard<std::mutex> lock(m_notifyLock);
      m_frameWork = frame;
    }
    for (Thread::Semaphore &sync : m_sync)
      sync.Post();
    std::unique_lock<std::mutex> lock(m_notifyLock);
    for (bool &done : m_done)
    {
      while (!done)
        m_notifySync.wait(lock);
    }
    for (bool &done : m_done)
      done = false;
  }

  void Pause()
  {
    std::unique_lock<std::mutex> lock(m_notifyLock);
    m_pause = true;
    while (AnyRunning())
      m_notifySync.wait(lock);
  }

  void Resume()
  {
    std::lock_guard<std::mutex> lock(m_notifyLock);
    m_pause = false;
  }

  bool IsAnyRunning()
  {
    std::lock_guard<std::mutex> lock(m_notifyLock);
    return AnyRunning();
  }

  void Stop()
  {
    Pause();
    {
      std::lock_guard<std::mutex> lock(m_notifyLock);
      m_stop = true;
    }
    for (int board = 0; board < NUM_BOARDS; board++)
    {
      m_sync[board].Post();
      m_threads[board].join();
    }
  }

  uint64_t Work(int board) const
  {
    return m_work[board];
  }

private:
  bool AnyRunning() const
  {
    for (bool running : m_running)
    {
      if (running)
        return true;
    }
    return false;
  }

  void RunBoard(int board)
  {
    unsigned seed = unsigned(board) * 2654435761u + 1;
    for (;;)
    {
      bool wait = true;
      bool exit = false;
      uint64_t frameWork = 0;
      while (wait && !exit)
      {
        m_sync[board].Wait();
        std::lock_guard<std::mutex> lock(m_notifyLock);
        if (m_stop)
          exit = true;
        else if (!m_pause)
        {
          wait = false;
          m_running[board] = true;
          frameWork = m_frameWork;
        }
      }
      if (exit)
        return;

      // Not protected: only the main thread reads this, after this board is done
      m_work[board] += frameWork;
      if (m_jitter)
      {
        seed = seed * 1103515245u + 12345u;
        if ((seed >> 16) % 64 == 0)
          std::this_thread::yield();
      }

      std::lock_guard<std::mutex> lock(m_notifyLock);
      m_running[board] = false;
      m_done[board] = true;
      m_notifySync.notify_all();
    }
  }

  const bool m_jitter;
  std::thread m_threads[NUM_BOARDS];
  Thread::Semaphore m_sync[NUM_BOARDS];
  std::mutex m_notifyLock;
  std::condition_variable m_notifySync;
  bool m_done[NUM_BOARDS] = {};
  bool m_running[NUM_BOARDS] = {};
  bool m_pause = false;
  bool m_stop = false;
  uint64_t m_frameWork = 0;
  uint64_t m_work[NUM_BOARDS] = {};
};

static void RunFrames(bool jitter, uint64_t frames)
{
  FrameLoop loop(jitter);
  uint64_t expected = 0;
  for (uint64_t frame = 1; frame <= frames; frame++)
  {
    loop.RunFrame(frame);
    expected += frame;
  }
  for (int board = 0; board < FrameLoop::NUM_BOARDS; board++)
    Check("every board did every frame exactly once", loop.Work(board) == expected);
  loop.Stop();
}

static void TestFrameLoopFast()
{
  RunFrames(false, 200000);
}

static void TestFrameLoopJitter()
{
  RunFrames(true, 100000);
}

static void TestPauseAndResume()
{
  // Pausing between frames, as a save state does
  FrameLoop loop(true);
  uint64_t expected = 0;
  bool anyRunningWhilePaused = false;
  for (uint64_t frame = 1; frame <= 20000; frame++)
  {
    loop.RunFrame(frame);
    expected += frame;
    if (frame % 7 == 0)
    {
      loop.Pause();
      anyRunningWhilePaused = anyRunningWhilePaused || loop.IsAnyRunning();
      loop.Resume();
    }
  }
  Check("nothing runs while paused", !anyRunningWhilePaused);
  for (int board = 0; board < FrameLoop::NUM_BOARDS; board++)
    Check("every board did every frame exactly once", loop.Work(board) == expected);
  loop.Stop();
}

static void TestStartAndStopRepeatedly()
{
  for (int cycle = 0; cycle < 2000; cycle++)
  {
    FrameLoop loop(false);
    for (uint64_t frame = 1; frame <= uint64_t(cycle % 5); frame++)
      loop.RunFrame(frame);
    loop.Stop();
  }
}

/******************************************************************************
 The unsynchronized sound board of CModel3
******************************************************************************/

// SDL's audio thread wakes the sound board thread, taking the sound board lock
// and then the main lock, while the main thread pauses and resumes it holding
// only the main lock. The sound board thread takes them in the same order.
class SoundBoardLoop
{
public:
  SoundBoardLoop()
  {
    m_thread = Thread::Start("SoundBoardNoSync", [this] { Run(); });
  }

  void Wake()
  {
    std::lock_guard<std::mutex> sndLock(m_sndLock);
    bool wake;
    {
      std::lock_guard<std::mutex> lock(m_notifyLock);
      wake = !m_running;
    }
    if (wake)
    {
      m_wakeNotify = true;
      m_sndSync.notify_one();
    }
  }

  void Pause()
  {
    std::unique_lock<std::mutex> lock(m_notifyLock);
    m_pause = true;
    while (m_running)
      m_notifySync.wait(lock);
  }

  void Resume()
  {
    std::lock_guard<std::mutex> lock(m_notifyLock);
    m_pause = false;
  }

  void Stop()
  {
    Pause();
    {
      std::lock_guard<std::mutex> lock(m_notifyLock);
      m_stop = true;
    }
    Wake();
    m_thread.join();
  }

  int Bursts() const
  {
    return m_bursts;
  }

private:
  void Run()
  {
    for (;;)
    {
      bool wait = true;
      bool exit = false;
      while (wait && !exit)
      {
        std::unique_lock<std::mutex> sndLock(m_sndLock);
        while (!m_wakeNotify)
          m_sndSync.wait(sndLock);
        m_wakeNotify = false;

        std::lock_guard<std::mutex> lock(m_notifyLock);
        if (m_stop)
          exit = true;
        else if (!m_pause)
        {
          wait = false;
          m_running = true;
        }
      }
      if (exit)
        return;

      m_bursts++;
      std::lock_guard<std::mutex> lock(m_notifyLock);
      m_running = false;
      m_notifySync.notify_all();
    }
  }

  std::thread m_thread;
  std::mutex m_sndLock;
  std::condition_variable m_sndSync;
  std::mutex m_notifyLock;
  std::condition_variable m_notifySync;
  bool m_wakeNotify = false;
  bool m_running = false;
  bool m_pause = false;
  bool m_stop = false;
  std::atomic<int> m_bursts{0};
};

static void TestSoundBoardWakes()
{
  SoundBoardLoop sound;
  std::atomic<bool> stopAudio(false);

  // Started with std::thread, not Thread::Start(), as the audio thread is foreign
  std::thread audio([&]
  {
    while (!stopAudio)
      sound.Wake();
  });

  for (int i = 0; i < 20000; i++)
  {
    sound.Pause();
    sound.Resume();
  }
  stopAudio = true;
  audio.join();
  sound.Stop();
  Check("the audio thread woke the sound board", sound.Bursts() > 0);
}

static void TestStopWithoutAudio()
{
  // Stopping must not depend on the audio thread running
  for (int i = 0; i < 2000; i++)
  {
    SoundBoardLoop sound;
    sound.Stop();
  }
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

  Test("Start runs the body", TestStartRunsBody);
  Test("Many threads", TestManyThreads);
#if !defined(_WIN32)
  Test("Asynchronous signals blocked", TestSignalsBlocked);
  Test("Thread naming", TestNaming);
#endif
  Test("Semaphore counts", TestSemaphoreCounts);
  Test("Semaphore blocks and wakes", TestSemaphoreBlocks);
  Test("Each post consumed once", TestEachPostConsumedOnce);
  Test("Semaphore destroyed after wake", TestDestroySemaphoreAfterWake, 180);
  Test("Frame loop", TestFrameLoopFast, 180);
  Test("Frame loop with jitter", TestFrameLoopJitter, 180);
  Test("Pause and resume", TestPauseAndResume, 180);
  Test("Start and stop repeatedly", TestStartAndStopRepeatedly, 180);
  Test("Sound board woken by a foreign thread", TestSoundBoardWakes, 180);
  Test("Sound board stops without audio", TestStopWithoutAudio, 180);

  s_deadline = 0;
  printf("%s: %d failed checks\n", s_failures == 0 ? "ALL PASSED" : "FAILED", s_failures);
  return s_failures == 0 ? 0 : 1;
}
