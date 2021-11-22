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
 * SCSPLFO.cpp
 * 
 * SCSP low frequency oscillator emulation. Included directly into SCSP.cpp. Do
 * not compile this!
 */

#include <cmath>
#include <cstdlib>

#define LFO_SHIFT 	8

struct _LFO
{
	unsigned short phase;
	UINT32 phase_step;
	int *table;
	int *scale;
};

#define LFIX(v)	((unsigned int) ((float) (1<<LFO_SHIFT)*(v)))

//Convert DB to multiply amplitude
#define DB(v) 	LFIX(pow(10.0,v/20.0))

//Convert cents to step increment
#define CENTS(v) LFIX(pow(2.0,v/1200.0))

static int PLFO_TRI[256], PLFO_SQR[256], PLFO_SAW[256], PLFO_NOI[256];
static int ALFO_TRI[256], ALFO_SQR[256], ALFO_SAW[256], ALFO_NOI[256];
static float LFOFreq[32] = { 0.17,0.19,0.23,0.27,0.34,0.39,0.45,0.55,0.68,0.78,0.92,1.10,1.39,1.60,1.87,2.27,
			  2.87,3.31,3.92,4.79,6.15,7.18,8.60,10.8,14.4,17.2,21.5,28.7,43.1,57.4,86.1,172.3 };
static float ASCALE[8] = { 0.0,0.4,0.8,1.5,3.0,6.0,12.0,24.0 };
static float PSCALE[8] = { 0.0,7.0,13.5,27.0,55.0,112.0,230.0,494 };
static int PSCALES[8][256];
static int ASCALES[8][256];

void LFO_Init(void)
{
	int i, s;
	for (i = 0; i < 256; ++i)
	{
		int a, p;
		//		float TL;
				//Saw
		a = 255 - i;
		if (i < 128)
			p = i;
		else
			p = i - 256;
		ALFO_SAW[i] = a;
		PLFO_SAW[i] = p;

		//Square
		if (i < 128)
		{
			a = 255;
			p = 127;
		}
		else
		{
			a = 0;
			p = -128;
		}
		ALFO_SQR[i] = a;
		PLFO_SQR[i] = p;

		//Tri
		if (i < 128)
			a = 255 - (i * 2);
		else
			a = (i * 2) - 256;
		if (i < 64)
			p = i * 2;
		else if (i < 128)
			p = 255 - i * 2;
		else if (i < 192)
			p = 256 - i * 2;
		else
			p = i * 2 - 511;
		ALFO_TRI[i] = a;
		PLFO_TRI[i] = p;

		//noise
		//a=lfo_noise[i];
		a = rand() & 0xff;
		p = 128 - a;
		ALFO_NOI[i] = a;
		PLFO_NOI[i] = p;
	}

	for (s = 0; s < 8; ++s)
	{
		float limit = PSCALE[s];
		for (i = -128; i < 128; ++i)
		{
			PSCALES[s][i + 128] = CENTS(((limit*(float)i) / 128.0));
		}
		limit = -ASCALE[s];
		for (i = 0; i < 256; ++i)
		{
			ASCALES[s][i] = DB(((limit*(float)i) / 256.0));
		}
	}
}

signed int inline PLFO_Step(struct _LFO *LFO)
{
	int p;
	LFO->phase += LFO->phase_step;
#if LFO_SHIFT!=8    
	LFO->phase &= (1 << (LFO_SHIFT + 8)) - 1;
#endif    
	p = LFO->table[LFO->phase >> LFO_SHIFT];
	p = LFO->scale[p + 128];
	return p << (SHIFT - LFO_SHIFT);
}

signed int inline ALFO_Step(struct _LFO *LFO)
{
	int p;
	LFO->phase += LFO->phase_step;
#if LFO_SHIFT!=8    
	LFO->phase &= (1 << (LFO_SHIFT + 8)) - 1;
#endif    
	p = LFO->table[LFO->phase >> LFO_SHIFT];
	p = LFO->scale[p];
	return p << (SHIFT - LFO_SHIFT);
}

void LFO_ComputeStep(struct _LFO *LFO, UINT32 LFOF, UINT32 LFOWS, UINT32 LFOS, int ALFO)
{
	float step = (float)LFOFreq[LFOF] * 256.0 / (float)44100.0;
	LFO->phase_step = (unsigned int)((float)(1 << LFO_SHIFT)*step);
	if (ALFO)
	{
		switch (LFOWS)
		{
		case 0: LFO->table = ALFO_SAW; break;
		case 1: LFO->table = ALFO_SQR; break;
		case 2: LFO->table = ALFO_TRI; break;
		case 3: LFO->table = ALFO_NOI; break;
		}
		LFO->scale = ASCALES[LFOS];
	}
	else
	{
		switch (LFOWS)
		{
		case 0: LFO->table = PLFO_SAW; break;
		case 1: LFO->table = PLFO_SQR; break;
		case 2: LFO->table = PLFO_TRI; break;
		case 3: LFO->table = PLFO_NOI; break;
		}
		LFO->scale = PSCALES[LFOS];
	}
}