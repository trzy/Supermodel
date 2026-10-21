/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2003-2026 The Supermodel Team
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
 * SCSPDSP.cpp
 * 
 * SCSP DSP emulation.
 */
 
#include "Supermodel.h"
#include "SCSPDSP.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _MSC_VER
#pragma warning(disable:4311)
#endif

static UINT16 PACK(INT32 val)
{
	int sign = (val >> 23) & 0x1;
	UINT32 temp = (val ^ (val << 1)) & 0xFFFFFF;
	int exponent = 0;
	for (int k = 0; k < 12; k++)
	{
		if (temp & 0x800000)
			break;
		temp <<= 1;
		exponent += 1;
	}
	if (exponent < 12)
		val = (val << exponent) & 0x3FFFFF;
	else
		val <<= 11;
	val >>= 11;
	val &= 0x7FF;
	val |= sign << 15;
	val |= exponent << 11;

	return (UINT16)val;
}

static INT32 UNPACK(UINT16 val)
{
	int sign = (val >> 15) & 0x1;
	int exponent = (val >> 11) & 0xF;
	int mantissa = val & 0x7FF;
	INT32 uval = mantissa << 11;
	if (exponent > 11)
	{
		exponent = 11;
		uval |= sign << 22;
	}
	else
	{
		uval |= (sign ^ 1) << 22;
	}
	uval |= sign << 23;
	uval <<= 8;
	uval >>= 8;
	uval >>= exponent;

	return uval;
}

void SCSPDSP_Init(_SCSPDSP *DSP)
{
	memset(DSP, 0, sizeof(_SCSPDSP));
	DSP->RBL = (8 * 1024); // Initial RBL is 0
	DSP->Stopped = true;
}
static inline INT32 SignExtend(int bits, UINT32 v)
{
	return (INT32)(v << (32 - bits)) >> (32 - bits);
}

// The sound memory access started by the previous step happens now, through a single port: a pending read is done
// in preference to a pending write. A read becomes available to an IWT 2 or 3 steps after its MRD.
static inline void DoMemoryAccess(_SCSPDSP *DSP)
{
	if (DSP->READPENDING)
	{
		UINT16 w = DSP->SCSPRAM[DSP->RWADDR];
		DSP->MEMVAL = (DSP->READPENDING == 2) ? ((INT32)(INT16)w << 8) : UNPACK(w);
		DSP->READPENDING = 0;
	}
	else if (DSP->WRITEPENDING)
	{
		DSP->SCSPRAM[DSP->RWADDR] = DSP->WRITEVAL;
		DSP->WRITEPENDING = false;
	}
}

// One sample of the DSP program. Instruction timing follows Mednafen (hardware tested) and Neill Corlett's notes:
// - an MRD/MWT only starts a memory access, which is done during the next step;
// - IWT copies the last word read (from an MRD 2 or 3 steps earlier) into MEMS, after this step's input was read;
// - ACC, FRC, Y, ADRS and INPUTS keep their values from one sample to the next.
void SCSPDSP_Step(_SCSPDSP *DSP)
{
	if (DSP->Stopped)
		return;

	for (int step = 0; step < DSP->LastStep; ++step)
	{
		const UINT16 *IPtr = DSP->MPRO + step * 4;

		UINT32 TRA = (IPtr[0] >> 8) & 0x7F;
		UINT32 TWT = (IPtr[0] >> 7) & 0x01;
		UINT32 TWA = (IPtr[0] >> 0) & 0x7F;

		UINT32 XSEL = (IPtr[1] >> 15) & 0x01;
		UINT32 YSEL = (IPtr[1] >> 13) & 0x03;
		UINT32 IRA = (IPtr[1] >> 6) & 0x3F;
		UINT32 IWT = (IPtr[1] >> 5) & 0x01;
		UINT32 IWA = (IPtr[1] >> 0) & 0x1F;

		UINT32 TABLE = (IPtr[2] >> 15) & 0x01;
		UINT32 MWT = (IPtr[2] >> 14) & 0x01;
		UINT32 MRD = (IPtr[2] >> 13) & 0x01;
		UINT32 EWT = (IPtr[2] >> 12) & 0x01;
		UINT32 EWA = (IPtr[2] >> 8) & 0x0F;
		UINT32 ADRL = (IPtr[2] >> 7) & 0x01;
		UINT32 FRCL = (IPtr[2] >> 6) & 0x01;
		UINT32 SHIFT = (IPtr[2] >> 4) & 0x03;
		UINT32 YRL = (IPtr[2] >> 3) & 0x01;
		UINT32 NEGB = (IPtr[2] >> 2) & 0x01;
		UINT32 ZERO = (IPtr[2] >> 1) & 0x01;
		UINT32 BSEL = (IPtr[2] >> 0) & 0x01;

		UINT32 COEF = (IPtr[3] >> 9) & 0x3f;
		UINT32 NOFL = (IPtr[3] >> 8) & 0x01;	// 1: sound memory data is 16-bit integer, not floating point (bit 8, Mednafen)
		UINT32 MASA = (IPtr[3] >> 2) & 0x1f;	// MADRS entry for the memory address
		UINT32 ADREB = (IPtr[3] >> 1) & 0x01;
		UINT32 NXADR = (IPtr[3] >> 0) & 0x01;

		// Input: MEMS, MIXS (20 bit) or EXTS (16 bit). IRA 32h-3Fh selects nothing and INPUTS keeps its value.
		if (IRA <= 0x1f)
			DSP->INPUTS = SignExtend(24, DSP->MEMS[IRA]);
		else if (IRA <= 0x2F)
			DSP->INPUTS = SignExtend(20, DSP->MIXS[IRA - 0x20]) << 4;
		else if (IRA <= 0x31)
			DSP->INPUTS = (INT32)DSP->EXTS[IRA - 0x30] << 8;
		const INT32 INPUTS = DSP->INPUTS;

		// Y operand (13 bit), from the registers as they were before this step
		INT32 Y;
		if (YSEL == 0)
			Y = DSP->FRC_REG;
		else if (YSEL == 1)
			Y = DSP->COEF[COEF] >> 3;	// COEF[12:0] is in bits 15-3
		else if (YSEL == 2)
			Y = (DSP->Y_REG >> 11) & 0x1FFF;
		else
			Y = (DSP->Y_REG >> 4) & 0x0FFF;
		Y = SignExtend(13, (UINT32)Y);

		if (YRL)
			DSP->Y_REG = INPUTS & 0xFFFFFF;

		// Shifter: the accumulator from the previous step, x1 or x2, saturated (SHIFT 0, 1) or wrapped (2, 3) to 24 bits
		INT32 SHIFTED = DSP->ACC;
		if (SHIFT == 1 || SHIFT == 2)
			SHIFTED *= 2;
		if (SHIFT < 2)
		{
			if (SHIFTED > 0x007FFFFF)
				SHIFTED = 0x007FFFFF;
			if (SHIFTED < -0x00800000)
				SHIFTED = -0x00800000;
		}
		else
			SHIFTED = SignExtend(24, (UINT32)SHIFTED);

		if (FRCL)
			DSP->FRC_REG = (SHIFT == 3) ? (SHIFTED & 0x0FFF) : ((SHIFTED >> 11) & 0x1FFF);

		// Multiply and accumulate (26 bit)
		{
			INT32 TEMP = SignExtend(24, DSP->TEMP[(TRA + DSP->DEC) & 0x7F]);
			INT32 X = XSEL ? INPUTS : TEMP;
			INT32 B = BSEL ? DSP->ACC : TEMP;
			if (NEGB)
				B = 0 - B;
			if (ZERO)
				B = 0;
			INT64 product = ((INT64)X * (INT64)Y) >> 12;
			DSP->ACC = SignExtend(26, (UINT32)((INT32)product + B));
		}

		if (EWT)
			DSP->EFREG[EWA] = SHIFTED >> 8;

		if (TWT)
			DSP->TEMP[(TWA + DSP->DEC) & 0x7F] = SHIFTED;

		// IWT: the word from an earlier MRD goes to MEMS. This step already read its input, so with IRA == IWA it
		// saw the old MEMS value. VF3's master DSP program relies on this: forwarding the new value instead turns one
		// of its feedback loops into an integrator, and the DC from its synth waveforms builds up until the output
		// sits at the limit.
		if (IWT)
			DSP->MEMS[IWA] = DSP->MEMVAL;

		// Finish the memory access started by the previous step, then start this step's
		DoMemoryAccess(DSP);
		// The address is calculated on every step: a write left waiting behind a read goes to the next step's address
		{
			UINT32 ADDR = DSP->MADRS[MASA];
			if (NXADR)
				ADDR++;
			if (ADREB)
				ADDR += SignExtend(12, DSP->ADRS_REG);
			if (!TABLE)
			{
				ADDR += DSP->DEC;
				ADDR &= DSP->RBL - 1;
			}
			else
				ADDR &= 0xFFFF;
			DSP->RWADDR = (ADDR + (DSP->RBP << 12)) & 0x7FFFF;
		}
		if (MRD)
			DSP->READPENDING = NOFL ? 2 : 1;
		if (MWT)
		{
			DSP->WRITEPENDING = true;
			DSP->WRITEVAL = NOFL ? (UINT16)(SHIFTED >> 8) : PACK(SHIFTED);
		}
		if (ADRL)
			DSP->ADRS_REG = (SHIFT == 3) ? ((SHIFTED >> 12) & 0xFFF) : ((INPUTS >> 16) & 0xFFF);
	}

	// Steps LastStep-127 are empty instructions, which still run on the chip. They finish the pending memory accesses
	// (a read, then a write left waiting behind it, at MADRS[0] + DEC), select MEMS[0] as input and leave
	// ACC = TEMP[DEC] * FRC + TEMP[DEC].
	for (int step = DSP->LastStep; step < 128 && step < DSP->LastStep + 2; ++step)
	{
		DoMemoryAccess(DSP);
		DSP->RWADDR = (((DSP->MADRS[0] + DSP->DEC) & (DSP->RBL - 1)) + (DSP->RBP << 12)) & 0x7FFFF;
		DSP->INPUTS = SignExtend(24, DSP->MEMS[0]);
		INT32 TEMP = SignExtend(24, DSP->TEMP[DSP->DEC & 0x7F]);
		INT64 product = ((INT64)TEMP * SignExtend(13, (UINT32)DSP->FRC_REG)) >> 12;
		DSP->ACC = SignExtend(26, (UINT32)((INT32)product + TEMP));
	}
	--DSP->DEC;
	memset(DSP->MIXS, 0, sizeof(DSP->MIXS));
}

void SCSPDSP_SetSample(_SCSPDSP *DSP, INT32 sample, int SEL)
{
	DSP->MIXS[SEL] += sample;
}

void SCSPDSP_Start(_SCSPDSP *DSP)
{
	int i;
	DSP->Stopped = false;
	for (i = 127; i >= 0; --i)
	{
		UINT16 *IPtr = DSP->MPRO + i * 4;

		if (IPtr[0] != 0 || IPtr[1] != 0 || IPtr[2] != 0 || IPtr[3] != 0)
			break;
	}
	DSP->LastStep = i + 1;
}