/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011 Bart Trzynadlowski 
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
 * SoundBoard.h
 * 
 * Header file defining the CSoundBoard class: Model 3 audio subsystem.
 */

#ifndef INCLUDED_SOUNDBOARD_H
#define INCLUDED_SOUNDBOARD_H


/*
 * CSoundBoard:
 *
 * Model 3 sound board (68K CPU + 2 x SCSP).
 */
class CSoundBoard
{
public:
	/*
	 * WriteMIDIPort(data):
	 *
	 * Writes to the sound board MIDI port.
	 *
	 * Parameters:
	 *		data	Byte to write to MIDI port.
	 */
	void WriteMIDIPort(UINT8 data);
	
	/*
	 * RunFrame(void):
	 *
	 * Runs the sound board for one frame, updating sound in the process.
	 */
	void RunFrame(void);
	
	/*
	 * Reset(void):
	 *
	 * Resets the sound board.
	 */
	void Reset(void);
	
	/*
	 * Init(soundROMPtr, sampleROMPtr):
	 *
	 * One-time initialization. Must be called prior to all other members.
	 *
	 * Parameters:
	 *		soundROMPtr		Pointer to sound ROM (68K program).
	 *		sampleROMPtr	Pointer to sample ROM.
	 *		ppcIRQObjectPtr	Pointer to PowerPC-side IRQ object.
	 *		soundIRQBit		IRQ bit mask to use for sound board PowerPC IRQs.
	 *
	 * Returns:
	 *		OKAY if successful, FAIL if unable to allocate memory. Prints own
	 *		error messages.
	 */
	BOOL Init(const UINT8 *soundROMPtr, const UINT8 *sampleROMPtr, CIRQ *ppcIRQObjectPtr, unsigned soundIRQBit);
	 
	/*
	 * CSoundBoard(void):
	 * ~CSoundBoard(void):
	 *
	 * Constructor and destructor.
	 */
	CSoundBoard(void);
	~CSoundBoard(void);
	
private:
	// PowerPC IRQ controller
	CIRQ		*ppcIRQ;
	unsigned	ppcSoundIRQBit;
	
	// Sound board memory
	const UINT8	*soundROM;		// 68K program ROM (passed in from parent object)
	const UINT8	*sampleROM;		// 68K sample ROM (passed in from parent object)
	UINT8		*memoryPool;	// single allocated region for all sound board RAM
	UINT8		*ram1, *ram2;	// SCSP1 and SCSP2 RAM
};


#endif	// INCLUDED_SOUNDBOARD_H
