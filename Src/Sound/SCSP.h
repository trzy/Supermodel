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
 * SCSP.h
 * 
 * Header file defining for SCSP emulation.
 */

#ifndef INCLUDED_SCSP_H
#define INCLUDED_SCSP_H


void SCSP_w8(unsigned int addr,unsigned char val);
void SCSP_w16(unsigned int addr,unsigned short val);
void SCSP_w32(unsigned int addr,unsigned int val);
unsigned char SCSP_r8(unsigned int addr);
unsigned short SCSP_r16(unsigned int addr);
unsigned int SCSP_r32(unsigned int addr);

void SCSP_SetCB(int (*Run68k)(int cycles),void (*Int68k)(int irq), CIRQ *ppcIRQObjectPtr, unsigned soundIRQBit);
void SCSP_Update();
void SCSP_MidiIn(unsigned char);
void SCSP_MidiOutW(unsigned char);
unsigned char SCSP_MidiOutFill();
unsigned char SCSP_MidiInFill();
void SCSP_CpuRunScanline();
unsigned char SCSP_MidiOutR();
void SCSP_Init(int n);
void SCSP_SetRAM(int n,unsigned char *r);
void SCSP_RTECheck();
int SCSP_IRQCB(int);

void SCSP_Master_w8(unsigned int addr,unsigned char val);
void SCSP_Master_w16(unsigned int addr,unsigned short val);
void SCSP_Master_w32(unsigned int addr,unsigned int val);
void SCSP_Slave_w8(unsigned int addr,unsigned char val);
void SCSP_Slave_w16(unsigned int addr,unsigned short val);
void SCSP_Slave_w32(unsigned int addr,unsigned int val);
unsigned char SCSP_Master_r8(unsigned int addr);
unsigned short SCSP_Master_r16(unsigned int addr);
unsigned int SCSP_Master_r32(unsigned int addr);
unsigned char SCSP_Slave_r8(unsigned int addr);
unsigned short SCSP_Slave_r16(unsigned int addr);
unsigned int SCSP_Slave_r32(unsigned int addr);

// Supermodel interface functions
void SCSP_SetBuffers(INT16 *leftBufferPtr, INT16 *rightBufferPtr, int bufferLength);
void SCSP_Deinit(void);


#endif	// INCLUDED_SCSP_H