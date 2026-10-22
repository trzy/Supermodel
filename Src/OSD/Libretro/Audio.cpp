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
 * Audio.cpp (libretro)
 *
 * Implementation of the OSD audio interface (OSD/Audio.h) for the libretro
 * core. The sound board produces one frame of 4-channel audio (front and rear
 * pairs) per emulated frame; it is downmixed to stereo here, following the
 * stereo mixing rules of the SDL implementation, and queued until the
 * frontend loop hands it to the libretro audio callback.
 */

#include "OSD/Audio.h"
#include "LibretroAudio.h"

#include "Supermodel.h"

#include <algorithm>
#include <mutex>
#include <vector>

static Game::AudioTypes s_audioType = Game::STEREO_LR;
static bool s_enabled = true;
static bool s_mono = false;

// Mixer factors computed from the front/rear and left/right balance settings
static float s_factorFrontLeft = 1.0f;
static float s_factorFrontRight = 1.0f;
static float s_factorRearLeft = 1.0f;
static float s_factorRearRight = 1.0f;

static std::mutex s_queueMutex;
static std::vector<int16_t> s_queue;  // interleaved stereo samples

static int16_t ClampINT16(float x)
{
  int32_t xi = (int32_t) x;
  if (xi > INT16_MAX)
    xi = INT16_MAX;
  if (xi < INT16_MIN)
    xi = INT16_MIN;
  return (int16_t) xi;
}

static int16_t MixINT16(float x, float y)
{
  return ClampINT16((x + y) * 0.5f);
}

void SetAudioCallback(AudioCallbackFPtr callback, void *data)
{
  // Not used: the sound board runs in lockstep with the emulated frame
  (void) callback;
  (void) data;
}

bool AudioCallbackSupported()
{
  // The frontend takes the audio of each frame in retro_run(): the sound board
  // must make one frame of audio per emulated frame
  return false;
}

void SetAudioEnabled(bool enabled)
{
  s_enabled = enabled;
}

void SetAudioType(Game::AudioTypes type)
{
  s_audioType = type;
}

void LibretroAudio::SetBalance(float leftRight, float frontRear)
{
  float lr = std::max(-100.f, std::min(100.f, leftRight)) * 0.01f;
  float fr = std::max(-100.f, std::min(100.f, frontRear)) * 0.01f;
  s_factorFrontLeft  = (lr < 0.f ? 1.f + lr : 1.f) * (fr < 0.f ? 1.f + fr : 1.f);
  s_factorFrontRight = (lr > 0.f ? 1.f - lr : 1.f) * (fr < 0.f ? 1.f + fr : 1.f);
  s_factorRearLeft   = (lr < 0.f ? 1.f + lr : 1.f) * (fr > 0.f ? 1.f - fr : 1.f);
  s_factorRearRight  = (lr > 0.f ? 1.f - lr : 1.f) * (fr > 0.f ? 1.f - fr : 1.f);
}

size_t LibretroAudio::Drain(std::vector<int16_t> *samples)
{
  std::lock_guard<std::mutex> lock(s_queueMutex);
  samples->swap(s_queue);
  s_queue.clear();
  return samples->size() / 2;
}

void LibretroAudio::Clear()
{
  std::lock_guard<std::mutex> lock(s_queueMutex);
  s_queue.clear();
}

Result OpenAudio(const Util::Config::Node &config)
{
  // libretro output is always stereo; mono games are duplicated on both sides
  s_mono = s_audioType == Game::MONO;
  LibretroAudio::SetBalance(config["BalanceLeftRight"].ValueAsDefault<float>(0.0f), config["BalanceFrontRear"].ValueAsDefault<float>(0.0f));
  LibretroAudio::Clear();
  s_enabled = true;
  return Result::OKAY;
}

bool OutputAudio(unsigned numSamples, const float *leftFrontBuffer, const float *rightFrontBuffer, const float *leftRearBuffer, const float *rightRearBuffer, bool flipStereo)
{
  // Same channel ordering rules as the SDL implementation
  switch (s_audioType)
  {
  case Game::STEREO_RL:
  case Game::QUAD_1_FRL_2_RRL:
  case Game::QUAD_1_RRL_2_FRL:
    flipStereo = !flipStereo;
    break;
  default:
    break;
  }

  std::lock_guard<std::mutex> lock(s_queueMutex);
  size_t base = s_queue.size();
  s_queue.resize(base + numSamples * 2);
  int16_t *p = s_queue.data() + base;

  if (!s_enabled)
  {
    std::fill(p, p + numSamples * 2, int16_t(0));
    return false;
  }

  for (unsigned i = 0; i < numSamples; i++)
  {
    float fl = leftFrontBuffer[i] * s_factorFrontLeft;
    float fr = rightFrontBuffer[i] * s_factorFrontRight;
    float rl = leftRearBuffer[i] * s_factorRearLeft;
    float rr = rightRearBuffer[i] * s_factorRearRight;

    int16_t left, right;
    if (s_mono)
    {
      float front = (fl + fr) * 0.5f;
      float rear = (rl + rr) * 0.5f;
      left = right = MixINT16(front, rear);
    }
    else
    {
      left = MixINT16(fl, rl);
      right = MixINT16(fr, rr);
    }

    if (flipStereo)
    {
      *p++ = right;
      *p++ = left;
    }
    else
    {
      *p++ = left;
      *p++ = right;
    }
  }

  // The frontend paces emulation, so the buffer is never considered full
  return false;
}

void CloseAudio()
{
  LibretroAudio::Clear();
}
