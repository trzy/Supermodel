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
 * 68K.cpp
 * 
 * 68K CPU interface. This is presently just a wrapper for the Musashi 68K core
 * and therefore, only a single CPU is supported. In the future, we may want to
 * add in another 68K core (eg., Turbo68K, A68K, or a recompiler). 
 *
 * To-Do List
 * ----------
 * - Registers may not completely describe the 68K state. Musashi also has
 *   additional CPU state information in the context that its MAME interface
 *   accesses (interrupts pending, halted status, etc.)
 */

#include "Supermodel.h"
#include "Musashi/m68k.h"	// Musashi 68K core


/******************************************************************************
 Internal Context
 
 An active context must be mapped before calling M68K interface functions. Only
 the bus and IRQ handlers are copied here; the CPU context is passed directly
 to Musashi.
******************************************************************************/

// Bus
static CBus	*Bus = NULL;

// IRQ callback
static int	(*IRQAck)(int nIRQ) = NULL;


/******************************************************************************
 68K Interface
******************************************************************************/

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

void M68KSaveState(CBlockFile *StateFile, const char *name)
{
	StateFile->NewBlock(name, __FILE__);
	
	/*
	 * Rather than writing the context directly to the save state, each
	 * register is copied into an array first to ensure the same result across
	 * different compilers (in case the struct is ordered differently). This
	 * also prevents us from inadvertently modifying the callback pointers.
	 *
	 * Note: Some of these are undoubtedly 68010 or 68020 registers and not
	 * really necessary. But if the layout is changed now, the save state 
	 * version has to be changed, so don't do it!
	 */
	 
	UINT32	data[31];
	
	data[0] = m68k_get_reg(NULL, M68K_REG_D0);
	data[1] = m68k_get_reg(NULL, M68K_REG_D1);
	data[2] = m68k_get_reg(NULL, M68K_REG_D2);
	data[3] = m68k_get_reg(NULL, M68K_REG_D3);
	data[4] = m68k_get_reg(NULL, M68K_REG_D4);
	data[5] = m68k_get_reg(NULL, M68K_REG_D5);
	data[6] = m68k_get_reg(NULL, M68K_REG_D6);
	data[7] = m68k_get_reg(NULL, M68K_REG_D7);
	data[8] = m68k_get_reg(NULL, M68K_REG_A0);
	data[9] = m68k_get_reg(NULL, M68K_REG_A1);
	data[10] = m68k_get_reg(NULL, M68K_REG_A2);
	data[11] = m68k_get_reg(NULL, M68K_REG_A3);
	data[12] = m68k_get_reg(NULL, M68K_REG_A4);
	data[13] = m68k_get_reg(NULL, M68K_REG_A5);
	data[14] = m68k_get_reg(NULL, M68K_REG_A6);
	data[15] = m68k_get_reg(NULL, M68K_REG_A7);
	data[16] = m68k_get_reg(NULL, M68K_REG_PC);
	data[17] = m68k_get_reg(NULL, M68K_REG_SR);
	data[18] = m68k_get_reg(NULL, M68K_REG_SP);
	data[19] = m68k_get_reg(NULL, M68K_REG_USP);
	data[20] = m68k_get_reg(NULL, M68K_REG_ISP);
	data[21] = m68k_get_reg(NULL, M68K_REG_MSP);
	data[22] = m68k_get_reg(NULL, M68K_REG_SFC);
	data[23] = m68k_get_reg(NULL, M68K_REG_DFC);
	data[24] = m68k_get_reg(NULL, M68K_REG_VBR);
	data[25] = m68k_get_reg(NULL, M68K_REG_CACR);
	data[26] = m68k_get_reg(NULL, M68K_REG_CAAR);
	data[27] = m68k_get_reg(NULL, M68K_REG_PREF_ADDR);
	data[28] = m68k_get_reg(NULL, M68K_REG_PREF_DATA);
	data[29] = m68k_get_reg(NULL, M68K_REG_PPC);
	data[30] = m68k_get_reg(NULL, M68K_REG_IR);
	
	StateFile->Write(data, sizeof(data));
}

void M68KLoadState(CBlockFile *StateFile, const char *name)
{
	if (OKAY != StateFile->FindBlock(name))
	{
		ErrorLog("Unable to load 68K state. Save state file is corrupted.");
		return;
	}

	UINT32	data[31];
	
	StateFile->Read(data, sizeof(data));
	m68k_set_reg(M68K_REG_D0, data[0]);
	m68k_set_reg(M68K_REG_D1, data[1]);
	m68k_set_reg(M68K_REG_D2, data[2]);
	m68k_set_reg(M68K_REG_D3, data[3]);
	m68k_set_reg(M68K_REG_D4, data[4]);
	m68k_set_reg(M68K_REG_D5, data[5]);
	m68k_set_reg(M68K_REG_D6, data[6]);
	m68k_set_reg(M68K_REG_D7, data[7]);
	m68k_set_reg(M68K_REG_A0, data[8]);
	m68k_set_reg(M68K_REG_A1, data[9]);
	m68k_set_reg(M68K_REG_A2, data[10]);
	m68k_set_reg(M68K_REG_A3, data[11]);
	m68k_set_reg(M68K_REG_A4, data[12]);
	m68k_set_reg(M68K_REG_A5, data[13]);
	m68k_set_reg(M68K_REG_A6, data[14]);
	m68k_set_reg(M68K_REG_A7, data[15]);
	m68k_set_reg(M68K_REG_PC, data[16]);
	m68k_set_reg(M68K_REG_SR, data[17]);
	m68k_set_reg(M68K_REG_SP, data[18]);
	m68k_set_reg(M68K_REG_USP, data[19]);
	m68k_set_reg(M68K_REG_ISP, data[20]);
	m68k_set_reg(M68K_REG_MSP, data[21]);
	m68k_set_reg(M68K_REG_SFC, data[22]);
	m68k_set_reg(M68K_REG_DFC, data[23]);
	m68k_set_reg(M68K_REG_VBR, data[24]);
	m68k_set_reg(M68K_REG_CACR, data[25]);
	m68k_set_reg(M68K_REG_CAAR, data[26]);
	m68k_set_reg(M68K_REG_PREF_ADDR, data[27]);
	m68k_set_reg(M68K_REG_PREF_DATA, data[28]);
	m68k_set_reg(M68K_REG_PPC, data[29]);
	m68k_set_reg(M68K_REG_IR, data[30]);
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
	DebugLog("68K reset\n");
}

// Callback setup

void M68KSetIRQCallback(int (*F)(int nIRQ))
{
	IRQAck = F;
}

void M68KAttachBus(CBus *BusPtr)
{
	Bus = BusPtr;
	DebugLog("Attached bus to 68K\n");
}

// Context switching

void M68KGetContext(M68KCtx *Dest)
{
	Dest->IRQAck = IRQAck;
	Dest->Bus = Bus;
	m68k_get_context(Dest->musashiCtx);
}

void M68KSetContext(M68KCtx *Src)
{
	IRQAck = Src->IRQAck;
	Bus = Src->Bus;
	m68k_set_context(Src->musashiCtx);
}

// One-time initialization

bool M68KInit(void)
{
	m68k_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68000);
	m68k_set_int_ack_callback(M68KIRQCallback);
	Bus = NULL;
	
	DebugLog("Initialized 68K\n");
	return OKAY;
}


/******************************************************************************
 Musashi 68K Handlers
 
 Musashi/m68kconf.h has been configured to call these directly.
******************************************************************************/

extern "C" {
		
int M68KIRQCallback(int nIRQ)
{
	if (NULL == IRQAck)	// no handler, use default behavior
	{
		m68k_set_irq(0);	// clear line
		return M68K_IRQ_AUTOVECTOR;
	}
	else
		return IRQAck(nIRQ);
}

unsigned int FASTCALL M68KFetch8(unsigned int a)
{
	return Bus->Read8(a);
}

unsigned int FASTCALL M68KFetch16(unsigned int a)
{
	return Bus->Read16(a);
}

unsigned int FASTCALL M68KFetch32(unsigned int a)
{
	return Bus->Read32(a);
}

unsigned int FASTCALL M68KRead8(unsigned int a)
{
	return Bus->Read8(a);
}

unsigned int FASTCALL M68KRead16(unsigned int a)
{
	return Bus->Read16(a);
}

unsigned int FASTCALL M68KRead32(unsigned int a)
{
	return Bus->Read32(a);
}

void FASTCALL M68KWrite8(unsigned int a, unsigned int d)
{
	Bus->Write8(a, d);
}

void FASTCALL M68KWrite16(unsigned int a, unsigned int d)
{
	Bus->Write16(a, d);
}

void FASTCALL M68KWrite32(unsigned int a, unsigned int d)
{
	Bus->Write32(a, d);
}

}	// extern "C"
