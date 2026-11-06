/**
 ** Supermodel
 ** A Sega Model Series Arcade Emulator.
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

#ifndef _MPEG_AUDIO_H_
#define _MPEG_AUDIO_H_

#include "Game.h"
#include <cstdint>


namespace MpegDec
{
  void  LoadCustomTracks(const std::string &music_filepath, const Game &game);
  void	SetMemory(const uint8_t *data, int offset, int length, bool loop);
	void	UpdateMemory(const uint8_t *data, int offset, int length, bool loop);
	int		GetPosition();
	void	SetPosition(int pos);
	void	DecodeAudio(int16_t* left, int16_t* right, int numStereoSamples);
	void	Stop();
	bool	IsLoaded();
}

#endif

