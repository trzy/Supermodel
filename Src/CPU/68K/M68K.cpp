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
 * M68K.cpp
 * 
 * 68K CPU interface. This is presently just a wrapper for the Musashi 68K core
 * and therefore, only a single CPU is supported. In the future, we may want to
 * add in another 68K core (eg., Turbo68K, A68K, or a recompiler). 
 */

#include "Supermodel.h"
#include "Musashi/m68k.h"	// Musashi 68K core

/******************************************************************************
 68K Interface
******************************************************************************/

// Interface function pointers
static int		(*IRQAck)(int nIRQ) = NULL;
static UINT8	(*Fetch8)(UINT32 addr) = NULL;
static UINT16	(*Fetch16)(UINT32 addr) = NULL;
static UINT32	(*Fetch32)(UINT32 addr) = NULL;
static UINT8	(*Read8)(UINT32 addr) = NULL;
static UINT16	(*Read16)(UINT32 addr) = NULL;
static UINT32	(*Read32)(UINT32 addr) = NULL;
static void		(*Write8)(UINT32 addr, UINT8 data) = NULL;
static void		(*Write16)(UINT32 addr, UINT16 data) = NULL;
static void		(*Write32)(UINT32 addr, UINT32 data) = NULL;

extern "C" {
	
// CPU state

UINT32 M68KGetARegister(int n)
{
	m68k_register_t	r;
	
	switch (n)
	{
	case 0:		r = M68K_REG_A0; break;
	case 1:		r = M68K_REG_A1; break;
	case 2:		r = M68K_REG_A2; break;
	case 3:		r = M68K_REG_A3; break;
	case 4:		r = M68K_REG_A4; break;
	case 5:		r = M68K_REG_A5; break;
	case 6:		r = M68K_REG_A6; break;
	case 7:		r = M68K_REG_A7; break;
	default:	r = M68K_REG_A7; break;
	}
	
	return m68k_get_reg(NULL, r);
}

UINT32 M68KGetDRegister(int n)
{
	m68k_register_t	r;
	
	switch (n)
	{
	case 0:		r = M68K_REG_D0; break;
	case 1:		r = M68K_REG_D1; break;
	case 2:		r = M68K_REG_D2; break;
	case 3:		r = M68K_REG_D3; break;
	case 4:		r = M68K_REG_D4; break;
	case 5:		r = M68K_REG_D5; break;
	case 6:		r = M68K_REG_D6; break;
	case 7:		r = M68K_REG_D7; break;
	default:	r = M68K_REG_D7; break;
	}
	
	return m68k_get_reg(NULL, r);
}

UINT32 M68KGetPC(void)
{
	return m68k_get_reg(NULL, M68K_REG_PC);
}

// Emulation functions

void M68KSetIRQ(int irqLevel)
{
	m68k_set_irq(irqLevel);
}

int M68KRun(int numCycles)
{
	return m68k_execute(numCycles);
}

void M68KReset(void)
{
	m68k_pulse_reset();
}

// Callback setup

void M68KSetIRQCallback(int (*F)(int nIRQ))
{
	IRQAck = F;
}

void M68KSetFetch8Callback(UINT8 (*F)(UINT32))
{
	Fetch8 = F;
}

void M68KSetFetch16Callback(UINT16 (*F)(UINT32))
{
	Fetch16 = F;
}

void M68KSetFetch32Callback(UINT32 (*F)(UINT32))
{
	Fetch32 = F;
}

void M68KSetRead8Callback(UINT8 (*F)(UINT32))
{
	Read8 = F;
}

void M68KSetRead16Callback(UINT16 (*F)(UINT32))
{
	Read16 = F;
}

void M68KSetRead32Callback(UINT32 (*F)(UINT32))
{
	Read32 = F;
}

void M68KSetWrite8Callback(void (*F)(UINT32,UINT8))
{
	Write8 = F;
}

void M68KSetWrite16Callback(void (*F)(UINT32,UINT16))
{
	Write16 = F;
}

void M68KSetWrite32Callback(void (*F)(UINT32,UINT32))
{
	Write32 = F;
}

// One-time initialization

BOOL M68KInit(void)
{
	m68k_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68000);
	m68k_set_int_ack_callback(M68KIRQCallback);
	return OKAY;
}

}	// extern "C"


/******************************************************************************
 Musashi 68K Handlers
 
 Musashi/m68kconf.h has been configured to call these directly.
******************************************************************************/

extern "C" {
		
int M68KIRQCallback(int nIRQ)
{
	if (NULL == IRQAck)
		return M68K_IRQ_AUTOVECTOR;
	else
		return IRQAck(nIRQ);
}

unsigned int FASTCALL M68KFetch8(unsigned int a)
{
	return Fetch8(a);
}

unsigned int FASTCALL M68KFetch16(unsigned int a)
{
	return Fetch16(a);
}

unsigned int FASTCALL M68KFetch32(unsigned int a)
{
	return Fetch32(a);
}

unsigned int FASTCALL M68KRead8(unsigned int a)
{
	return Read8(a);
}

unsigned int FASTCALL M68KRead16(unsigned int a)
{
	return Read16(a);
}

unsigned int FASTCALL M68KRead32(unsigned int a)
{
	return Read32(a);
}

void FASTCALL M68KWrite8(unsigned int a, unsigned int d)
{
	Write8(a, d);
}

void FASTCALL M68KWrite16(unsigned int a, unsigned int d)
{
	Write16(a, d);
}

void FASTCALL M68KWrite32(unsigned int a, unsigned int d)
{
	Write32(a, d);
}

}	// extern "C"
