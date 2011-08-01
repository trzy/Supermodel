#include "Supermodel.h"

#ifdef SUPERMODEL_OSX
#include <SDL/SDL.h>
#include <SDL/SDL_audio.h>
#else
#include <SDL.h>
#include <SDL_audio.h>
#endif

#include <math.h>

// Model3 audio output is 44.1KHz 2-channel sound and frame rate is 60fps
#define SAMPLE_RATE 44100
#define NUM_CHANNELS 2
#define SUPERMODEL_FPS 60

#define BYTES_PER_SAMPLE (NUM_CHANNELS * sizeof(INT16))
#define SAMPLES_PER_FRAME (SAMPLE_RATE / SUPERMODEL_FPS)
#define BYTES_PER_FRAME (SAMPLES_PER_FRAME * BYTES_PER_SAMPLE) 

#define MAX_LATENCY 100

static bool enabled = true;         // True if sound output is enabled
static unsigned latency = 20;       // Audio latency to use (ie size of audio buffer) as percentage of max buffer size

static unsigned playSamples = 512;  // Size (in samples) of callback play buffer

static UINT32 audioBufferSize = 0;  // Size (in bytes) of audio buffer
static INT8	*audioBuffer = NULL;    // Audio buffer

static UINT32 writePos = 0;         // Current position at which writing into buffer
static UINT32 playPos = 0;          // Current position at which playing data in buffer via callback

static bool writeWrapped = false;   // True if write position has wrapped around at end of buffer but play position has not done so yet

static unsigned underRuns = 0;      // Number of buffer under-runs that have occured
static unsigned overRuns = 0;       // Number of buffer over-runs that have occured

static void PlayCallback(void *data, Uint8 *stream, int len)
{
	//printf("PlayCallback(%d)\n", len);

	// Get current write position and adjust it if write has wrapped
	UINT32 adjWritePos = writePos;
	if (writeWrapped)
		adjWritePos += audioBufferSize;

	// Check if play position overlaps write position (ie buffer under-run)
	if (adjWritePos < playPos + len)
	{
		// If so, just copy silence to audio output stream and exit
		memset(stream, 0, len);

		//printf("Audio buffer under-run in PlayCallback\n");
		underRuns++;
		return;
	}
	
	INT8* src1;
	INT8* src2;
	UINT32 len1;
	UINT32 len2;

	// Check if play region extends past end of buffer
	if (playPos + len > audioBufferSize)
	{
		// If so, split play region into two
		src1 = audioBuffer + playPos;
		src2 = audioBuffer;
		len1 = audioBufferSize - playPos;
		len2 = len - len1;
	}
	else
	{
		// Otherwise, just copy whole region
		src1 = audioBuffer + playPos;
		src2 = 0;
		len1 = len;
		len2 = 0;
	}

	// Check if audio is enabled
	if (enabled)
	{
		// If so, copy play region into audio output stream and blank region out afterwards
		memcpy(stream, src1, len1);
		memset(src1, 0, len1);
		
		if (len2)
		{
			// If region was split into two, copy second half into audio output stream as well and blank region out afterwards
			memcpy(stream + len1, src2, len2);
			memset(src2, 0, len2);
		}
	}
	else
		// Otherwise, just copy silence to audio output stream
		memset(stream, 0, len);

	// Move play position forward for next time
	playPos += len;

	// Check if play position has moved past end of buffer
	if (playPos >= audioBufferSize)
	{
		// If so, wrap it around to beginning again and reset write wrap flag
		playPos -= audioBufferSize;
		writeWrapped = false;
	}
}

static void MixChannels(unsigned numSamples, INT16 *leftBuffer, INT16 *rightBuffer, void *dest)
{
	INT16 *p = (INT16*)dest;
	for (unsigned i = 0; i < numSamples; i++)
	{
#if (NUM_CHANNELS == 1)
		*p++ = leftBuffer[i] + rightBuffer[i];
#else
		*p++ = rightBuffer[i];
		*p++ = leftBuffer[i];
#endif
	}
}

BOOL OpenAudio()
{
	// Initialize SDL audio sub-system
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
	{
		ErrorLog("Unable to initialize SDL audio sub-system: %s\n", SDL_GetError());
		return FAIL;
	}

	// Set up audio specification
	SDL_AudioSpec fmt;
	memset(&fmt, 0, sizeof(SDL_AudioSpec));
	fmt.freq = SAMPLE_RATE;
	fmt.channels = NUM_CHANNELS;
	fmt.format = AUDIO_S16SYS;
	fmt.samples = playSamples;
	fmt.callback = PlayCallback;
	
	// Try opening SDL audio output with that specification
	SDL_AudioSpec obtained;
	if (SDL_OpenAudio(&fmt, &obtained) < 0)
	{
		ErrorLog("Unable to open 44.1KHz 2-channel audio with SDL: %s\n", SDL_GetError());
		return FAIL;
	}

	// Check what buffer sample size was actually obtained, and use that
	playSamples = obtained.samples;

	// Create audio buffer
	audioBufferSize = SAMPLE_RATE * BYTES_PER_SAMPLE * latency / MAX_LATENCY;
	int roundBuffer = 2 * playSamples;
	audioBufferSize = max<int>(roundBuffer, (audioBufferSize / roundBuffer) * roundBuffer);
	audioBuffer = new INT8[audioBufferSize];
	memset(audioBuffer, 0, sizeof(INT8) * audioBufferSize);
	
	// Set initial play position to be beginning of buffer and initial write position to be half-way into buffer
	playPos = 0;
	writePos = (BYTES_PER_FRAME + audioBufferSize) / 2;
	writeWrapped = false;

	// Reset counters
	underRuns = 0;
	overRuns = 0;

	// Start audio playing
	SDL_PauseAudio(0);
	return OKAY;
}

void OutputAudio(unsigned numSamples, INT16 *leftBuffer, INT16 *rightBuffer)
{
	//printf("OutputAudio(%u)\n", numSamples);

	UINT32 bytesRemaining;
	UINT32 bytesToCopy;
	INT16 *src;

	// Number of samples should never be more than max number of samples per frame
	if (numSamples > SAMPLES_PER_FRAME)
		numSamples = SAMPLES_PER_FRAME;

	// Mix together left and right channels into single chunk of data
	INT16 mixBuffer[NUM_CHANNELS * SAMPLES_PER_FRAME];
	MixChannels(numSamples, leftBuffer, rightBuffer, mixBuffer);
	
	// Lock SDL audio callback so that it doesn't interfere with following code
	SDL_LockAudio();
	
	// Calculate number of bytes for current sound chunk
	UINT32 numBytes = numSamples * BYTES_PER_SAMPLE;
	
	// Get end of current play region (writing must occur past this point)
	UINT32 playEndPos = playPos + BYTES_PER_FRAME;
	
	// Undo any wrap-around of the write position that may have occured to create following ordering: playPos < playEndPos < writePos
	if (writePos < playEndPos && writeWrapped)
		writePos += audioBufferSize;

	// If play region has caught up with write position and now overlaps it (ie buffer under-run), then bump write position forward in chunks
	// until it is past end of play region
	while (writePos < playEndPos)
	{
		//printf("Audio buffer under-run in OutputAudio\n");
		underRuns++;
		writePos += numBytes;
	}

	// If write position has caught up with play region and now overlaps it (ie buffer over-run), then discard current chunk of data
	if (writePos + numBytes > playPos + audioBufferSize)
	{
		//printf("Audio buffer over-run in OutputAudio\n");
		overRuns++;
		goto Finish;
	}

	// Check if write position has moved past end of buffer
	if (writePos >= audioBufferSize)
	{
		// If so, wrap it around to beginning again and set write wrap flag
		writePos -= audioBufferSize;
		writeWrapped = true;
	}

	src = mixBuffer;
	INT8 *dst1;
	INT8 *dst2;
	UINT32 len1;
	UINT32 len2;

	// Check if write region extends past end of buffer
	if (writePos + numBytes > audioBufferSize)
	{
		// If so, split write region into two
		dst1 = audioBuffer + writePos;
		dst2 = audioBuffer;
		len1 = audioBufferSize - writePos;
		len2 = numBytes - len1;
	}
	else
	{
		// Otherwise, just copy whole region
		dst1 = audioBuffer + writePos;
		dst2 = 0;
		len1 = numBytes;
		len2 = 0;
	}

	// Copy chunk to write position in buffer
	bytesRemaining = numBytes;
	bytesToCopy = (bytesRemaining > len1 ? len1 : bytesRemaining);
	memcpy(dst1, src, bytesToCopy);

	// Adjust for number of bytes copied
	bytesRemaining -= bytesToCopy;
	src = (INT16*)((UINT8*)src + bytesToCopy);

	if (bytesRemaining)
	{
		// If write region was split into two, copy second half of chunk into buffer as well
		bytesToCopy = (bytesRemaining > len2 ? len2 : bytesRemaining);
		memcpy(dst2, src, bytesToCopy);
	}

	// Move write position forward for next time
	writePos += numBytes;

	// Check if write position has moved past end of buffer
	if (writePos >= audioBufferSize)
	{
		// If so wrap it around to beginning again and set write wrap flag
		writePos -= audioBufferSize;
		writeWrapped = true;
	}

Finish:
	// Unlock SDL audio callback
	SDL_UnlockAudio();
}

void CloseAudio()
{
	// Close SDL audio output
	SDL_CloseAudio();

	// Delete audio buffer
	if (audioBuffer != NULL)
	{
		delete[] audioBuffer;
		audioBuffer = NULL;
	}
}