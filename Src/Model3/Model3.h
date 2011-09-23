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
 * Model3.h
 * 
 * Header file defining the CModel3 and CModel3Inputs classes.
 */

#ifndef INCLUDED_MODEL3_H
#define INCLUDED_MODEL3_H


/*
 * CModel3Inputs:
 *
 * Model 3 inputs. All buttons may only be 0 (released) or 1 (pressed). Any
 * other non-zero values will cause conflicts. Note that there is no 
 * constructor, so values must be initialized manually by the OSD code. In
 * practice, buttons do not need to be initialized because they are polled
 * but analog data should be initialized.
 *
 * To-Do List
 * ----------
 * - Add a constructor to initialize analog controls.
 * - Replace analog controls with floats.
 */
class CModel3Inputs
{
public:
	// Common between all games
	UINT8	coin[2], start[2], test[2], service[2];
	
	// Joysticks (players 1 and 2)
	UINT8	up[2], down[2], left[2], right[2];
	
	// Fighting game controls (players 1 and 2)
	UINT8	punch[2], kick[2], guard[2], escape[2];
	
	// Soccer game controls (only player 1 for now)
	UINT8	shortPass, longPass, shoot;
	
	// Vehicle controls
	UINT8	steering;
	UINT8	accelerator;
	UINT8	brake;
	
	// VR view buttons: VR1 Red, VR2 Blue, VR3 Yellow, VR4 Green
	UINT8	vr[4];

	// 4-speed gear shift
	UINT8	gearShift4[4];
	
	// Rally controls
	UINT8	viewChange;
	UINT8	handBrake;
	
	// Twin joysticks
	UINT8	twinJoyTurnLeft, twinJoyTurnRight;
	UINT8	twinJoyStrafeLeft, twinJoyStrafeRight;
	UINT8	twinJoyForward, twinJoyReverse;
	UINT8	twinJoyJump, twinJoyCrouch;
	UINT8	twinJoyLeftShot, twinJoyRightShot;
	UINT8	twinJoyLeftTurbo, twinJoyRightTurbo;

	// Analog joystick
	UINT8	analogJoyX, analogJoyY;
	UINT8	analogJoyTrigger;	// trigger
	UINT8	analogJoyEvent;		// event button
	
	// Gun controls
	UINT16	gunX, gunY;
	UINT8	trigger;
	UINT8	offscreen;

};

/*
 * CModel3:
 *
 * A complete Model 3 system.
 *
 * Inherits CBus in order to pass the address space handlers to devices that
 * may need them (CPU, DMA, etc.)
 *
 * NOTE: Currently NOT re-entrant due to a non-OOP PowerPC core. Do NOT create
 * create more than one CModel3 object!
 */
class CModel3: public CBus, public CPCIDevice
{
public:
	/*
	 * ReadPCIConfigSpace(device, reg, bits, offset):
	 *
	 * Handles unknown PCI devices. See CPCIDevice definition for more details.
	 *
	 * Parameters:
	 *		device	Device number.
	 *		reg		Register number.
	 *		bits	Bit width of access (8, 16, or 32 only).;
	 *		offset	Byte offset within register, aligned to the specified bit
	 *				width, and offset from the 32-bit aligned base of the
	 * 				register number.
	 *
	 * Returns:
	 *		Register data.
	 */
	UINT32 ReadPCIConfigSpace(unsigned device, unsigned reg, unsigned bits, unsigned width);
	
	/*
	 * WritePCIConfigSpace(device, reg, bits, offset, data):
	 *
	 * Handles unknown PCI devices. See CPCIDevice definition for more details.
	 *
	 * Parameters:
	 *		device	Device number.
	 *		reg		Register number.
	 *		bits	Bit width of access (8, 16, or 32 only).
	 *		offset	Byte offset within register, aligned to the specified bit
	 *				width, and offset from the 32-bit aligned base of the
	 * 				register number.
	 *		data	Data.
	 */
	void WritePCIConfigSpace(unsigned device, unsigned reg, unsigned bits, unsigned width, UINT32 data);

	/*
	 * Read8(addr):
	 * Read16(addr):
	 * Read32(addr):
	 * Read64(addr):
	 *
	 * Read a byte, 16-bit half word, 32-bit word, or 64-bit double word from 
	 * the PowerPC address space. This implements the PowerPC address bus. Note
	 * that it is big endian, so when accessing from a little endian device,
	 * the byte order must be manually reversed.
	 *
	 * Parameters:
	 *		addr	Address to read.
	 *
	 * Returns:
	 *		Data at the address.
	 */
	UINT8 Read8(UINT32 addr);
	UINT16 Read16(UINT32 addr);
	UINT32 Read32(UINT32 addr);
	UINT64 Read64(UINT32 addr);
	
	/*
	 * Write8(addr, data):
	 * Write16(addr, data):
	 * Write32(addr, data):
	 * Write64(addr, data):
	 *
	 * Write a byte, half word, word, or double word to the PowerPC address
	 * space. Note that everything is stored in big endian form, so when
	 * accessing with a little endian device, the byte order must be manually
	 * reversed.
	 *
	 * Parameters:
	 *		addr	Address to write.
	 *		data	Data to write.
	 */
	void Write8(UINT32 addr, UINT8 data);
	void Write16(UINT32 addr, UINT16 data);
	void Write32(UINT32 addr, UINT32 data);
	void Write64(UINT32 addr, UINT64 data);
	
	/*
	 * SaveState(SaveState):
	 *
	 * Saves an image of the current state.
	 *
	 * Parameters:
	 *		SaveState	Block file to save state information to.
	 */
	void SaveState(CBlockFile *SaveState);

	/*
	 * LoadState(SaveState):
	 *
	 * Loads and resumes execution from a state image.
	 *
	 * Parameters:
	 *		SaveState	Block file to load state information from.
	 */
	void LoadState(CBlockFile *SaveState);
	
	/*
	 * SaveNVRAM(NVRAM):
	 *
	 * Saves an image of the current NVRAM state.
	 *
	 * Parameters:
	 *		NVRAM	Block file to save NVRAM to.
	 */
	void SaveNVRAM(CBlockFile *NVRAM);

	/*
	 * LoadNVRAM(NVRAM):
	 *
	 * Loads an NVRAM image.
	 *
	 * Parameters:
	 *		NVRAM	Block file to load NVRAM state from.
	 */
	void LoadNVRAM(CBlockFile *NVRAM);
	
	/*
	 * ClearNVRAM(void):
	 *
	 * Clears all NVRAM (backup RAM and EEPROM).
	 */
	void ClearNVRAM(void);
	
	/*
	 * RunFrame(void):
	 *
	 * Runs one frame (assuming 60 Hz video refresh rate).
	 */
	void RunFrame(void);
	
	/*
	 * Reset(void):
	 *
	 * Resets the system. Does not modify non-volatile memory.
	 */
	void Reset(void);
	
	/* 
	 * GetGameInfo(void):
	 *
	 * Returns:
	 *		A pointer to the presently loaded game's information structure (or
	 *		NULL if no ROM set has yet been loaded).
	 */
	const struct GameInfo * GetGameInfo(void);
	
	/*
	 * LoadROMSet(GameList, zipFile):
	 *
	 * Loads a complete ROM set from the specified ZIP archive.
	 *
	 * Parameters:
	 *		GameList	List of all supported games and their ROMs.
 	 *		zipFile		ZIP file to load from.
 	 *
 	 * Returns:
 	 *		OKAY if successful, FAIL otherwise. Prints errors.
 	 */
 	BOOL LoadROMSet(const struct GameInfo *GameList, const char *zipFile);
 	
	/*
	 * AttachRenderers(Render2DPtr, Render3DPtr):
	 *
	 * Attaches the renderers to the appropriate device objects.
	 *
	 * Parameters:
	 *		Render2DPtr		Pointer to a tile renderer object.
	 *		Render3DPtr		Same as above but for a 3D renderer.
	 */
	void AttachRenderers(CRender2D *Render2DPtr, CRender3D *Render3DPtr);
	
	/*
	 * AttachInputs(InputsPtr):
	 *
	 * Attaches OSD-managed inputs.
	 *
	 * Parameters:
	 *		InputsPtr	Pointer to the object containing input states.
	 */
	void AttachInputs(CModel3Inputs *InputsPtr);
	
	/*
	 * Init(ppcFrequencyParam):
	 *
	 * One-time initialization of the context. Must be called prior to all
	 * other members. Allocates memory and initializes device states.
	 *
	 * Parameters:
	 *		ppcFrequencyParam	PowerPC frequency in Hz. If less than 1 MHz,
	 *							will be clamped to 1 MHz.
	 *
	 * Returns:
	 *		OKAY is successful, otherwise FAILED if a non-recoverable error
	 *		occurred. Prints own error messages.
	 */
	BOOL Init(unsigned ppcFrequencyParam);
	 
	/*
	 * CModel3(void):
	 * ~CModel3(void):
	 *
	 * Constructor and destructor for Model 3 class. Constructor performs a 
	 * bare-bones initialization of object; does not perform any memory 
	 * allocation or any actions that can fail. The destructor will deallocate
	 * memory and free resources used by the object (and its child objects).
	 */
	CModel3(void);
	~CModel3(void);
	
	/*
	 * Private Property.
	 * Tresspassers will be shot! ;)
	 */
private:
	
	// Private member functions
	UINT8	ReadInputs(unsigned reg);
	void	WriteInputs(unsigned reg, UINT8 data);
	UINT32	ReadSecurity(unsigned reg);
	void	WriteSecurity(unsigned reg, UINT32 data);
	void 	SetCROMBank(unsigned idx);
	UINT8	ReadSystemRegister(unsigned reg);
	void 	WriteSystemRegister(unsigned reg, UINT8 data);
	void	Patch(void);

	// Game and hardware information
	const struct GameInfo	*Game;
	
	// Game inputs
	CModel3Inputs	*Inputs;
		 
	// Input registers (game controls)
	UINT8		inputBank;
	UINT8		serialFIFO1, serialFIFO2;
	UINT8		gunReg;
	int			adcChannel;
	
	// Emulated core Model 3 memory regions
	UINT8		*memoryPool;	// single allocated region for all ROM and system RAM
	UINT8		*ram;			// 8 MB PowerPC RAM
	UINT8		*crom;			// 8+128 MB CROM (fixed CROM first, then 64MB of banked CROMs -- Daytona2 might need extra?)
	UINT8		*vrom;			// 64 MB VROM (video ROM, visible only to Real3D)
	UINT8		*backupRAM;		// 128 KB Backup RAM (battery backed)
	UINT8		*securityRAM;	// 128 KB Security Board RAM
	
	// Banked CROM
	UINT8		*cromBank;		// currently mapped in CROM bank
	unsigned	cromBankReg;	// the CROM bank register 
	
	// Security device
	unsigned	securityPtr;	// pointer to current offset in security data
	
	// PowerPC
	PPC_FETCH_REGION	PPCFetchRegions[3];
	unsigned			ppcFrequency;	// clock frequency (Hz)
	
	// Other devices
	CIRQ		IRQ;		// Model 3 IRQ controller
	CMPC10x		PCIBridge;	// MPC10x PCI/bridge/memory controller
	CPCIBus		PCIBus;		// Model 3's PCI bus
	C53C810		SCSI;		// NCR 53C810 SCSI controller
	CRTC72421	RTC;		// Epson RTC-72421 real-time clock
	C93C46		EEPROM;		// 93C46 EEPROM
	CTileGen	TileGen;	// Sega 2D tile generator
	CReal3D		GPU;		// Real3D graphics hardware
};


#endif	// INCLUDED_MODEL3_H
