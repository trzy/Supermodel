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
 * M68K.h
 * 
 * Header file for 68K CPU interface. Caution: there is only a single 68K core
 * available to Supermodel right now. Therefore, multiple 68K CPUs are not 
 * presently supported. See M68K.c for more details.
 *
 * TO-DO List:
 * -----------
 * - Optimize things, perhaps by using FASTCALL
 */

#ifndef INCLUDED_M68K_H
#define INCLUDED_M68K_H

#include "Types.h"
#include "Musashi/m68k.h"

#ifdef __cplusplus
extern "C" {
#endif

// This doesn't work for now (needs to be added to the prototypes in m68k.h for m68k_read_memory*)
//#ifndef FASTCALL
	#undef FASTCALL
	#define FASTCALL
//#endif


/******************************************************************************
 Definitions 
******************************************************************************/

// IRQ callback special return values
#define M68K_IRQ_AUTOVECTOR	M68K_INT_ACK_AUTOVECTOR	// signals an autovectored interrupt
#define M68K_IRQ_SPURIOUS	M68K_INT_ACK_SPURIOUS	// signals a spurious interrupt


/******************************************************************************
 68K Interface
******************************************************************************/

/*
 * M68KGetDRegister(int n):
 *
 * Parameters:
 *		n	Register number (0-7).
 *
 * Returns:
 *		Register An.
 */
extern UINT32 M68KGetARegister(int n);

/*
 * M68KGetDRegister(int n):
 *
 * Parameters:
 *		n	Register number (0-7).
 *
 * Returns:
 *		Register Dn.
 */
extern UINT32 M68KGetDRegister(int reg);

/*
 * M68KGetPC():
 *
 * Returns:
 *		Current program counter.
 */
extern UINT32 M68KGetPC(void);

/*
 * M68KSetIRQ(irqLevel):
 *
 * Sets the interrupt level (IPL2-IPL0 pins).
 *
 * Parameters:
 *		irqLevel	The interrupt level (1-7) or 0 to clear the interrupt.
 */
extern void M68KSetIRQ(int irqLevel);

/*
 * M68KRun(numCycles):
 *
 * Runs the 68K.
 *
 * Parameters:
 *		numCycles	Number of cycles to run.
 *
 * Returns:
 *		Number of cycles executed.
 */
extern int M68KRun(int numCycles);

/*
 * M68KReset():
 *
 * Resets the 68K.
 */
extern void M68KReset(void);

/*
 * M68KSetIRQCallback(F):
 *
 * Installs an interrupt acknowledge callback. The default behavior is to 
 * always assume autovectored interrupts.
 *
 * Parameters:
 *		F	Callback function.
 */
extern void M68KSetIRQCallback(int (*F)(int));

/*
 * M68KSetFetch8Callback(F):
 * M68KSetFetch16Callback(F):
 * M68KSetFetch32Callback(F):
 * M68KSetRead8Callback(F):
 * M68KSetRead16Callback(F):
 * M68KSetRead32Callback(F):
 * M68KSetWrite8Callback(F):
 * M68KSetWrite16Callback(F):
 * M68KSetWrite32Callback(F):
 *
 * Installs address space handler callbacks. There is no default behavior;
 * these must all be set up before any 68K-related emulation functions are
 * invoked.
 *
 * Parameters:
 *		F	Callback function.
 */
extern void M68KSetFetch8Callback(UINT8 (*F)(UINT32));
extern void M68KSetFetch16Callback(UINT16 (*F)(UINT32));
extern void M68KSetFetch32Callback(UINT32 (*F)(UINT32));
extern void M68KSetRead8Callback(UINT8 (*F)(UINT32));
extern void M68KSetRead16Callback(UINT16 (*F)(UINT32));
extern void M68KSetRead32Callback(UINT32 (*F)(UINT32));
extern void M68KSetWrite8Callback(void (*F)(UINT32,UINT8));
extern void M68KSetWrite16Callback(void (*F)(UINT32,UINT16));
extern void M68KSetWrite32Callback(void (*F)(UINT32,UINT32));

/*
 * M68KInit():
 *
 * Initializes the 68K emulator. Must be called once per program session prior
 * to any other 68K interface calls.
 *
 * Returns:
 *		Always returns OKAY.
 */
extern BOOL M68KInit(void);


/******************************************************************************
 68K Handlers
 
 Intended for use directly by the 68K core.
******************************************************************************/

/*
 * M68KIRQCallback(nIRQ):
 *
 * Interrupt acknowledge callback. Called when an interrupt is being serviced.
 * Actually implemented by calling a user-supplied callback.
 *
 * Parameters:
 *		nIRQ	IRQ level (1-7).
 *
 * Returns:
 *		The interrupt vector to use, M68K_IRQ_AUTOVECTOR, or
 *		M68K_IRQ_SPURIOUS. If no callback has been installed, the default
 *		callback is used and always returns M68K_IRQ_AUTOVECTOR.
 */
extern int M68KIRQCallback(int nIRQ);

/*
 * M68KFetch8(a):
 * M68KFetch16(a):
 * M68KFetch32(a):
 *
 * Read data from the program address space.
 *
 * Parameters:
 *		a		Address to read from.
 *
 * Returns:
 *		The 8, 16, or 32-bit value read.
 */
unsigned int FASTCALL M68KFetch8(unsigned int a);
unsigned int FASTCALL M68KFetch16(unsigned int a);
unsigned int FASTCALL M68KFetch32(unsigned int a);

/*
 * M68KRead8(a):
 * M68KRead16(a):
 * M68KRead32(a):
 *
 * Read data from the data address space.
 *
 * Parameters:
 *		a		Address to read from.
 *
 * Returns:
 *		The 8, 16, or 32-bit value read.
 */
unsigned int FASTCALL M68KRead8(unsigned int a);
unsigned int FASTCALL M68KRead16(unsigned int a);
unsigned int FASTCALL M68KRead32(unsigned int a);

/*
 * M68KWrite8(a, d):
 * M68KWrite16(a, d):
 * M68KWrite32(a, d):
 *
 * Writes to the data address space.
 *
 * Parameters:
 *		a		Address to write to.
 *		d		Data to write.
 */
void FASTCALL M68KWrite8(unsigned int a, unsigned int d);
void FASTCALL M68KWrite16(unsigned int a, unsigned int d);
void FASTCALL M68KWrite32(unsigned int a, unsigned int d);


#ifdef __cplusplus
 }
#endif


#endif	// INCLUDED_M68K_H