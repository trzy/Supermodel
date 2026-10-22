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
 * LibretroAudio.h
 *
 * Libretro-specific extensions of the OSD audio interface.
 */

#ifndef INCLUDED_LIBRETROAUDIO_H
#define INCLUDED_LIBRETROAUDIO_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace LibretroAudio
{
  // Balance in percent (-100..100), as the BalanceLeftRight/BalanceFrontRear settings
  void SetBalance(float leftRight, float frontRear);

  // Moves the queued interleaved stereo samples into *samples, returns the number of stereo frames
  size_t Drain(std::vector<int16_t> *samples);

  // Discards queued samples
  void Clear();
}

#endif  // INCLUDED_LIBRETROAUDIO_H
