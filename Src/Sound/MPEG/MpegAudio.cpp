/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2003-2024 The Supermodel Team
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

#define MINIMP3_IMPLEMENTATION
#include "Pkgs/minimp3.h"
#include "MpegAudio.h"
#include "Util/ConfigBuilders.h"
#include "OSD/Logger.h"

#include <cstdio>
#include <map>
#include <memory>
#include <filesystem>
#include <tuple>


/***************************************************************************************************
 Custom MPEG Tracks
***************************************************************************************************/

struct CustomTrack
{
  std::shared_ptr<uint8_t[]> mpeg_data;
  size_t mpeg_data_size;
  uint32_t mpeg_rom_start_offset;
  size_t file_start_offset;

  CustomTrack()
    : mpeg_data(nullptr),
      mpeg_data_size(0),
      mpeg_rom_start_offset(0),
      file_start_offset(0)
  {
  }

  CustomTrack(const std::shared_ptr<uint8_t[]> &mpeg_data, size_t mpeg_data_size, uint32_t mpeg_rom_start_offset, size_t file_start_offset)
    : mpeg_data(mpeg_data),
      mpeg_data_size(mpeg_data_size),
      mpeg_rom_start_offset(mpeg_rom_start_offset),
      file_start_offset(file_start_offset)
  {
  }
};

struct FileContents
{
  std::shared_ptr<uint8_t[]> bytes;
  size_t size = 0;
};

static std::map<uint32_t, CustomTrack> s_custom_tracks_by_mpeg_rom_address;

static FileContents LoadFile(const std::string &filepath)
{
  FILE *fp = fopen(filepath.c_str(), "rb");
  if (!fp)
  {
    ErrorLog("Unable to load music track from disk: %s.", filepath.c_str());
    return { nullptr, 0 };
  }
  fseek(fp, 0, SEEK_END);
  long file_size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  std::shared_ptr<uint8_t[]> mpeg_data(new uint8_t[file_size], std::default_delete<uint8_t[]>());
  fread(mpeg_data.get(), sizeof(uint8_t), file_size, fp);
  fclose(fp);
  return { mpeg_data, size_t(file_size) };
}

void MpegDec::LoadCustomTracks(const std::string &music_filepath, const Game &game)
{
  s_custom_tracks_by_mpeg_rom_address.clear();

  if (game.mpeg_board.empty())
  {
    // No MPEG board
    return;
  }

  if (!std::filesystem::exists(music_filepath))
  {
    // Custom music configuration file is optional
    return;
  }

  Util::Config::Node xml("xml");
  if (Util::Config::FromXMLFile(&xml, music_filepath))
  {
    ErrorLog("Custom music configuration file could not be loaded. Original game tracks will be used.");
    return;
  }

  /*
   * Sample XML:
   *
   * <games>
   *  <game name="scud">
   *    <track mpeg_rom_start_offset="" file_start_offset="0" filepath="song1.mp3" />
   *    <track mpeg_rom_start_offset="" file_start_offset="0x1000" filepath="song2.mp3" />
   *  </game>
   * </games>
   */

  std::map<std::string, FileContents> file_contents_by_filepath;

  for (auto it = xml.begin(); it != xml.end(); ++it)
  {
    auto &root_node = *it;
    if (root_node.Key() != "games")
    {
      continue;
    }

    for (auto &game_node: root_node)
    {
      if (game_node.Key() != "game")
      {
        continue;
      }

      if (game_node["name"].Empty())
      {
        continue;
      }
      std::string game_name = game_node["name"].ValueAs<std::string>();
      if (game_name != game.name)
      {
        continue;
      }

      for (auto &track_node: game_node)
      {
        if (track_node.Key() != "track")
        {
          continue;
        }

        size_t file_start_offset = 0;
        if (track_node["mpeg_rom_start_offset"].Empty())
        {
          ErrorLog("%s: Track in '%s' is missing 'mpeg_rom_start_offset' attribute and will be ignored.", music_filepath.c_str(), game.name.c_str());
          continue;
        }
        if (track_node["filepath"].Empty())
        {
          ErrorLog("%s: Track in '%s' is missing 'filepath' attribute and will be ignored.", music_filepath.c_str(), game.name.c_str());
          continue;
        }
        if (track_node["file_start_offset"].Exists())
        {
          file_start_offset = track_node["file_start_offset"].ValueAs<size_t>();
        }
        const std::string filepath = track_node["filepath"].ValueAs<std::string>();
        const uint32_t mpeg_rom_start_offset = track_node["mpeg_rom_start_offset"].ValueAs<uint32_t>();

        if (s_custom_tracks_by_mpeg_rom_address.count(mpeg_rom_start_offset) != 0)
        {
          ErrorLog("%s: Multiple tracks defined for '%s' MPEG ROM offset 0x%08x. Only the first will be used.", music_filepath.c_str(), game.name.c_str(), mpeg_rom_start_offset);
          continue;
        }

        if (file_contents_by_filepath.count(filepath) == 0)
        {
          FileContents contents = LoadFile(filepath);
          if (contents.bytes == nullptr)
          {
            continue;
          }
          file_contents_by_filepath[filepath] = contents;
          InfoLog("Loaded custom track: %s.", filepath.c_str());
          printf("Loaded custom track: %s.\n", filepath.c_str());
        }

        FileContents contents = file_contents_by_filepath[filepath];
        s_custom_tracks_by_mpeg_rom_address[mpeg_rom_start_offset] = CustomTrack(contents.bytes, contents.size, mpeg_rom_start_offset, file_start_offset);
      }
    }
  }
}


/***************************************************************************************************
 MPEG Music Playback
***************************************************************************************************/

struct Decoder
{
	mp3dec_t			mp3d;
	mp3dec_frame_info_t	info;
	const uint8_t*		buffer;
	int					size, pos;
	bool				loop;
	bool				stopped;
	int					numSamples;
	int					pcmPos;
	short				pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];

	std::shared_ptr<uint8_t[]>  custom_mpeg_data;
};

static Decoder dec{};

void MpegDec::SetMemory(const uint8_t *data, int offset, int length, bool loop)
{
  mp3dec_init(&dec.mp3d);

  auto it = s_custom_tracks_by_mpeg_rom_address.find(offset);
  if (it == s_custom_tracks_by_mpeg_rom_address.end()) {
    // MPEG ROM
    dec.buffer            = data + offset;
    dec.size              = length;
    dec.custom_mpeg_data  = nullptr;
  }
  else {
    // Custom track available
    const CustomTrack &track = it->second;

    size_t offset_in_file = track.file_start_offset;
    if (offset_in_file >= track.mpeg_data_size)
    {
      // Out of bounds, go to start of file
      offset_in_file = 0;
    }

    dec.buffer            = track.mpeg_data.get() + offset_in_file;
    dec.size              = track.mpeg_data_size - offset_in_file;
    dec.custom_mpeg_data  = track.mpeg_data;
  }

  dec.pos         = 0;
  dec.numSamples  = 0;
  dec.pcmPos      = 0;
  dec.loop        = loop;
  dec.stopped     = false;

	// Uncomment this line to print out track offsets in the MPEG ROM
	//printf("MPEG: Set memory: %08x\n", offset);
}

void MpegDec::UpdateMemory(const uint8_t* data, int offset, int length, bool loop)
{
  auto it = s_custom_tracks_by_mpeg_rom_address.find(offset);
  if (it == s_custom_tracks_by_mpeg_rom_address.end()) {
    // MPEG ROM
  	int diff;
  	if ((data + offset) > dec.buffer) {
  		diff = (int)(data + offset - dec.buffer);
  	}
  	else {
  		diff = -(int)(dec.buffer - data - offset);
  	}
    dec.buffer  = data + offset;
    dec.size    = length;
    dec.pos     = dec.pos - diff; // update position relative to our new start location
  }
  else {
    // Custom track available. This is tricky. This command updates the start/end pointers (usually
    // used by games to create a loop point). We need to ensure that the custom track definition is
    // consistent: the custom track associated with this ROM offset must be the same file as is
    // currently playing, otherwise we do nothing.
    CustomTrack &track = it->second;
    if (track.mpeg_data == dec.custom_mpeg_data)
    {
      size_t offset_in_file = track.file_start_offset;
      if (offset_in_file >= track.mpeg_data_size)
      {
        // Out of bounds, just use start of file
        offset_in_file = 0;
      }

      int diff;
    	if ((track.mpeg_data.get() + offset_in_file) > dec.buffer) {
    		diff = (int)(track.mpeg_data.get() + offset_in_file - dec.buffer);
    	}
    	else {
    		diff = -(int)(dec.buffer - track.mpeg_data.get() - offset_in_file);
    	}
      dec.buffer  = track.mpeg_data.get() + offset_in_file;
      dec.size    = track.mpeg_data_size - offset_in_file;  // ignoring length specified by caller because MPEG ROM end offsets won't in general match with track, so we always have to use EOF
      dec.pos     = dec.pos - diff; // update position relative to our new start location
    }
  }

	dec.loop	= loop;

	// Uncomment this line to print out track offsets in the MPEG ROM
	//printf("MPEG: Update memory: %08x\n", offset);
}

int MpegDec::GetPosition()
{
	return (int)dec.pos;
}

void MpegDec::SetPosition(int pos)
{
	dec.pos = pos;
}

static void FlushBuffer(int16_t*& left, int16_t*& right, int& numStereoSamples)
{
	int numChans = dec.info.channels;

	int &i = dec.pcmPos;

	for (; i < (dec.numSamples * numChans) && numStereoSamples; i += numChans) {
		*left++ = dec.pcm[i];
		*right++ = dec.pcm[i + numChans - 1];
		numStereoSamples--;
	}
}

// might need to copy some silence to end the buffer if we aren't looping
static void EndWithSilence(int16_t*& left, int16_t*& right, int& numStereoSamples)
{
	while (numStereoSamples)
	{
		*left++ = 0;
		*right++ = 0;
		numStereoSamples--;
	}
}

static bool EndOfBuffer()
{
	return dec.pos >= dec.size - HDR_SIZE;
}

void MpegDec::Stop()
{
	dec.stopped = true;
}

bool MpegDec::IsLoaded()
{
	return dec.buffer != nullptr;
}

void MpegDec::DecodeAudio(int16_t* left, int16_t* right, int numStereoSamples)
{
	// if we are stopped return silence
	if (dec.stopped || !dec.buffer) {
		EndWithSilence(left, right, numStereoSamples);
	}

	// copy any left over samples first
	FlushBuffer(left, right, numStereoSamples);

	while (numStereoSamples) {

		dec.numSamples = mp3dec_decode_frame(
			&dec.mp3d,
			dec.buffer + dec.pos,
			dec.size - dec.pos,
			dec.pcm,
			&dec.info);

		dec.pos += dec.info.frame_bytes;
		dec.pcmPos = 0;	// reset pos

		FlushBuffer(left, right, numStereoSamples);

		// check end of buffer handling
		if (EndOfBuffer()) {
			if (dec.loop) {
				dec.pos = 0;
			}
			else {
				EndWithSilence(left, right, numStereoSamples);
			}
		}

	}

}
