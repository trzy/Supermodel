#define MINIMP3_IMPLEMENTATION
#include "Pkgs/minimp3.h"
#include "MpegAudio.h"

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
};

static Decoder dec = { 0 };

void MpegDec::SetMemory(const uint8_t *data, int length, bool loop)
{
	mp3dec_init(&dec.mp3d);

	dec.buffer		= data;
	dec.size		= length;
	dec.pos			= 0;
	dec.numSamples	= 0;
	dec.pcmPos		= 0;
	dec.loop		= loop;
	dec.stopped		= false;
}

void MpegDec::UpdateMemory(const uint8_t* data, int length, bool loop)
{
	int diff;
	if (data > dec.buffer) {
		diff = (int)(data - dec.buffer);
	}
	else {
		diff = -(int)(dec.buffer - data);
	}

	dec.buffer	= data;
	dec.size	= length;
	dec.pos		= dec.pos - diff;		// update position relative to our new start location
	dec.loop	= loop;
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
