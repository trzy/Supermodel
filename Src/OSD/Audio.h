#ifndef INCLUDED_AUDIO_H
#define INCLUDED_AUDIO_H

/*
 * Audio.h
 * 
 * Function-based interface for audio output.
 */

/*
 * OpenAudio()
 *
 * Initializes the audio system.
 */
extern bool OpenAudio();

/*
 * OutputAudio(unsigned numSamples, *INT16 leftBuffer, *INT16 rightBuffer)
 *
 * Sends a chunk of two-channel audio with the given number of samples to the audio system.
 */
extern void OutputAudio(unsigned numSamples, INT16 *leftBuffer, INT16 *rightBuffer);

/*
 * CloseAudio()
 *
 * Shuts down the audio system.
 */
extern void CloseAudio();

#endif	// INCLUDED_AUDIO_H
