/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011 Bart Trzynadlowski, Nik Henson 
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
 * DriveBoard.h
 *
 * Header for the CDriveBoard (force feedback emulation) class.
 */

#ifndef INCLUDED_DRIVEBOARD_H
#define INCLUDED_DRIVEBOARD_H

/*
 * CDriveBoardConfig:
 *
 * Settings used by CDriveBoard.
 */
class CDriveBoardConfig
{
public:
	bool	 forceFeedback;		// Enable drive board emulation/simulation (read only during Reset(), cannot be changed in-game)
	bool     simulateDrvBoard;  // Simulate drive board rather than emulating it
	unsigned steeringStrength;  // Setting for steering strength on DIP switches of drive board

	// Defaults
	CDriveBoardConfig(void)
	{
		forceFeedback = false;
		simulateDrvBoard = false;
		steeringStrength = 5;
	}
};

/*
 * CDriveBoard
 */
class CDriveBoard : public CBus
{
public:
	bool IsAttached(void);

	bool IsSimulated(void);

	void GetDIPSwitches(UINT8 &dip1, UINT8 &dip2);

	void SetDIPSwitches(UINT8 dip1, UINT8 dip2);

	unsigned GetSteeringStrength();

	void SetSteeringStrength(unsigned steeringStrength);

	void Get7SegDisplays(UINT8 &seg1Digit, UINT8 &seg1Digit2, UINT8 &seg2Digit1, UINT8 &seg2Digit2);

	CZ80 *GetZ80(void);

	void SaveState(CBlockFile *SaveState);

	void LoadState(CBlockFile *SaveState);

	bool Init(const UINT8 *romPtr);

	void AttachInputs(CInputs *InputsPtr, unsigned gameInputFlags);

	void Reset(void);

	UINT8 Read(void);

	void Write(UINT8 data);

	void RunFrame(void);

	CDriveBoard();
	
	~CDriveBoard(void);

	// CBus methods
	UINT8 Read8(UINT32 addr);
	
	void Write8(UINT32 addr, UINT8 data);

	UINT8 IORead8(UINT32 portNum);
	
	void IOWrite8(UINT32 portNum, UINT8 data);
	
private:
	bool m_attached;		// True if drive board is attached
	bool m_tmpDisabled;	    // True if temporarily disabled by loading an incompatible save state
	bool m_simulated;       // True if drive board should be simulated rather than emulated

	UINT8 m_dip1;		    // Value of DIP switch 1
	UINT8 m_dip2;	        // Value of DIP switch 2

	const UINT8* m_rom;	    // 32k ROM 
	UINT8* m_ram;           // 8k RAM

	CZ80 m_z80;             // Z80 CPU @ 4MHz

	CInputs *m_inputs;      
	unsigned m_inputFlags;
	
	// Emulation state
	bool m_initialized;     // True if drive board has finished initialization
	bool m_allowInterrupts; // True if drive board has enabled NMI interrupts

	UINT8 m_seg1Digit1;		// Current value of left digit on 7-segment display 1
	UINT8 m_seg1Digit2;		// Current value of right digit on 7-segment display 1
	UINT8 m_seg2Digit1;		// Current value of left digit on 7-segment display 2
	UINT8 m_seg2Digit2;     // Current value of right digit on 7-segment display 2

	UINT8 m_dataSent;       // Last command sent by main board
	UINT8 m_dataReceived;   // Data to send back to main board

	UINT16 m_adcPortRead;   // ADC port currently reading from
	UINT8 m_adcPortBit;     // Bit number currently reading on ADC port

	UINT8 m_port42Out;      // Last value sent to Z80 I/O port 42 (encoder motor data)
	UINT8 m_port46Out;      // Last value sent to Z80 I/O port 46 (encoder motor control)

	UINT8 m_prev42Out;      // Previous value sent to Z80 I/O port 42
	UINT8 m_prev46Out;      // Previous value sent to Z80 I/O port 46

	UINT8 m_uncenterVal1;   // First part of pending uncenter command
	UINT8 m_uncenterVal2;   // Second part of pending uncenter command

	// Simulation state
	UINT8 m_initState;
	UINT8 m_statusFlags;
	UINT8 m_boardMode;
	UINT8 m_readMode;
	UINT8 m_wheelCenter;
	UINT8 m_cockpitCenter;
	UINT8 m_echoVal;

	// Feedback state
	INT8 m_lastConstForce;  // Last constant force command sent
	UINT8 m_lastSelfCenter; // Last self center command sent
	UINT8 m_lastFriction;   // Last friction command sent
	UINT8 m_lastVibrate;    // Last vibrate command sent

	UINT8 SimulateRead(void);

	void SimulateWrite(UINT8 data);

	void SimulateFrame(void);

	void EmulateFrame(void);

	void ProcessEncoderCmd(void);

	void SendStopAll(void);

	void SendConstantForce(INT8 val);

	void SendSelfCenter(UINT8 val);

	void SendFriction(UINT8 val);

	void SendVibrate(UINT8 val);
};

#endif	// INCLUDED_DRIVEBOARD_H
