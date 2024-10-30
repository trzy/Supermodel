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

