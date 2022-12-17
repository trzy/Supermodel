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
 * SCSPDSP.cpp
 * 
 * SCSP DSP emulation.
 */
 
#include "Supermodel.h"
#include "SCSPDSP.h"
//#include <assert.h>
#define assert(x)	;	// disable assert() for releases
//#include <memory.h>
//#include <stdio.h>
//#include <malloc.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#pragma warning(disable:4311)

#define DYNBUF	0x10000

//this doesn't work at all
//#define USEFLOATPACK

//the PACK func in asm plus mov [esi],ax
unsigned char PackFunc[]={0x8B,0xD8,0xA9,0x00,0x00,0x80,0x00,0x75,0x02,0xF7,0xD3,0xF7,0xD3,0x0F,0xBD,0xCB,
						  0xF7,0xD9,0xC1,0xE0,0x08,0x83,0xC1,0x16,0xD3,0xE0,0xC1,0xF8,0x13,0xC1,0xE1,0x0B,
						  0x25,0xFF,0x87,0x00,0x00,0x0B,0xC1,0x66,0x89,0x06};
unsigned char UnpackFunc[]={0x8B,0xD8,0x8B,0xC8,0x81,0xE3,0x00,0x80,0x00,0x00,0x25,0xFF,0x07,
							0x00,0x00,0xC1,0xE9,0x0B,0xC1,0xE0,0x0B,0xC1,0xE3,0x08,0x83,0xE1,0x0F,0x0B,0xC3,
							0xD1,0xEB,0x81,0xF3,0x00,0x00,0x40,0x00,0x0B,0xC3,0x83,0xC1,0x08,0xC1,0xE0,0x08,
							0xD3,0xF8};
//#if 0
//unsigned short inline PACK(signed int val)
//{
///*	signed int v1=val;
//	int n=0;
//	while(((v1>>22)&1) == ((v1>>23)&1))
//	{
//		v1<<=1;
//		++n;
//	}
//	v1<<=8;
//	v1>>=11+8;
//	v1=(v1&(~0x7800))|(n<<11);
//	return v1;
//*/
//#ifdef USEFLOATPACK
//	unsigned short f;
//	__asm
//	{
//		mov eax,val
//		mov ebx,eax
//		test eax,0x00800000
//		jne negval
//		not ebx
//negval:	not ebx
//		bsr ecx,ebx
//		neg ecx
//		shl eax,8
//		add ecx,22
//		shl eax,cl
//		sar eax,8+11
//		shl ecx,11
//		and eax,~0x7800
//		or eax,ecx
//		mov f,ax
//	}
//	return f;
//#else
//
//	//cut to 16 bits
//	unsigned int f=((unsigned int ) val)>>8;
//	return f;
//#endif
//}
//
//signed int inline UNPACK(unsigned short val)
//{
///*	if(val)
//		int a=1;
//	unsigned int mant=val&0x7ff;
//	unsigned int exp=(val>>11)&0xf;
//	unsigned int sign=(val>>15)&1;
//	signed int r=0;
//	r|=mant<<11;
//	r|=sign<<23;
//	r|=(sign^1)<<22;
//
//	//signed int r=val<<8;
//	//if(r&0x00800000)
//	//	r|=0xFF000000;
//	r<<=8;
//	r>>=8+exp;
//	return r;
//*/
//#ifdef USEFLOATPACK
//	signed int r;
//	__asm
//	{
//		xor eax,eax
//		mov ax,val
//		mov ebx,eax
//		mov ecx,eax
//		and ebx,0x8000
//		and eax,0x07ff
//		shr ecx,11
//		shl eax,11
//		shl ebx,8
//		and ecx,0xF
//		or eax,ebx
//		shr ebx,1
//		xor ebx,0x00400000
//		or eax,ebx
//		add ecx,8
//		shl eax,8
//		sar eax,cl
//		mov r,eax
//	}
//#else
//	//unpack 16->24
//	signed int r=val<<8;
//	r<<=8;
//	r>>=8;
//#endif
//	return r;
//}
//#else

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
//#endif

void SCSPDSP_Init(_SCSPDSP *DSP)
{
	memset(DSP, 0, sizeof(_SCSPDSP));
	DSP->RBL = (8 * 1024); // Initial RBL is 0
	DSP->Stopped = 1;
}
//#ifndef DYNDSP
void SCSPDSP_Step(_SCSPDSP *DSP)
{
	INT32 ACC = 0;    //26 bit
	INT32 SHIFTED = 0;    //24 bit
	INT32 X = 0;  //24 bit
	INT32 Y = 0;  //13 bit
	INT32 B = 0;  //26 bit
	INT32 INPUTS = 0; //24 bit
	INT32 MEMVAL = 0;
	INT32 FRC_REG = 0;    //13 bit
	INT32 Y_REG = 0;      //24 bit
	UINT32 ADDR = 0;
	UINT32 ADRS_REG = 0;  //13 bit
	int step;

	if (DSP->Stopped)
		return;

	memset(DSP->EFREG, 0, 2 * 16);
	for (step = 0; step </*128*/DSP->LastStep; ++step)
	{
		UINT16 *IPtr = DSP->MPRO + step * 4;

		//		if(IPtr[0]==0 && IPtr[1]==0 && IPtr[2]==0 && IPtr[3]==0)
		//			break;

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

		UINT32 NOFL = (IPtr[3] >> 15) & 0x01;	//????
		UINT32 COEF = (IPtr[3] >> 9) & 0x3f;

		UINT32 MASA = (IPtr[3] >> 2) & 0x1f;	//???
		UINT32 ADREB = (IPtr[3] >> 1) & 0x01;
		UINT32 NXADR = (IPtr[3] >> 0) & 0x01;

		INT64 v;

		//operations are done at 24 bit precision
#if 0
		if (MASA)
			int a = 1;
		if (NOFL)
			int a = 1;
#endif
		//INPUTS RW
// colmns97 hits this
//		assert(IRA<0x32);
		if (IRA <= 0x1f)
			INPUTS = DSP->MEMS[IRA];
		else if (IRA <= 0x2F)
			INPUTS = DSP->MIXS[IRA - 0x20] << 4;  //MIXS is 20 bit
		else if (IRA <= 0x31)
			INPUTS = DSP->EXTS[IRA - 0x30] << 8;  //EXTS is 16 bit
		else
			return;

		INPUTS <<= 8;
		INPUTS >>= 8;
		//if(INPUTS&0x00800000)
		//	INPUTS|=0xFF000000;

		if (IWT)
		{
			DSP->MEMS[IWA] = MEMVAL;  //MEMVAL was selected in previous MRD
			if (IRA == IWA)
				INPUTS = MEMVAL;
		}

		//Operand sel
		//B
		if (!ZERO)
		{
			if (BSEL)
				B = ACC;
			else
			{
				B = DSP->TEMP[(TRA + DSP->DEC) & 0x7F];
				B <<= 8;
				B >>= 8;
				//if(B&0x00800000)
				//	B|=0xFF000000;  //Sign extend
			}
			if (NEGB)
				B = 0 - B;
		}
		else
			B = 0;

		//X
		if (XSEL)
			X = INPUTS;
		else
		{
			X = DSP->TEMP[(TRA + DSP->DEC) & 0x7F];
			X <<= 8;
			X >>= 8;
			//if(X&0x00800000)
			//	X|=0xFF000000;
		}

		//Y
		if (YSEL == 0)
			Y = FRC_REG;
		else if (YSEL == 1)
			Y = DSP->COEF[COEF] >> 3;   //COEF is 16 bits
		else if (YSEL == 2)
			Y = (Y_REG >> 11) & 0x1FFF;
		else if (YSEL == 3)
			Y = (Y_REG >> 4) & 0x0FFF;

		if (YRL)
			Y_REG = INPUTS;

		//Shifter
		if (SHIFT == 0)
		{
			SHIFTED = ACC;
			if (SHIFTED > 0x007FFFFF)
				SHIFTED = 0x007FFFFF;
			if (SHIFTED < (-0x00800000))
				SHIFTED = -0x00800000;
		}
		else if (SHIFT == 1)
		{
			SHIFTED = ACC * 2;
			if (SHIFTED > 0x007FFFFF)
				SHIFTED = 0x007FFFFF;
			if (SHIFTED < (-0x00800000))
				SHIFTED = -0x00800000;
		}
		else if (SHIFT == 2)
		{
			SHIFTED = ACC * 2;
			SHIFTED <<= 8;
			SHIFTED >>= 8;
			//SHIFTED&=0x00FFFFFF;
			//if(SHIFTED&0x00800000)
			//	SHIFTED|=0xFF000000;
		}
		else if (SHIFT == 3)
		{
			SHIFTED = ACC;
			SHIFTED <<= 8;
			SHIFTED >>= 8;
			//SHIFTED&=0x00FFFFFF;
			//if(SHIFTED&0x00800000)
			//	SHIFTED|=0xFF000000;
		}

		//ACCUM
		Y <<= 19;
		Y >>= 19;
		//if(Y&0x1000)
		//	Y|=0xFFFFF000;

		v = (((INT64)X*(INT64)Y) >> 12);
		ACC = (int)v + B;

		if (TWT)
			DSP->TEMP[(TWA + DSP->DEC) & 0x7F] = SHIFTED;

		if (FRCL)
		{
			if (SHIFT == 3)
				FRC_REG = SHIFTED & 0x0FFF;
			else
				FRC_REG = (SHIFTED >> 11) & 0x1FFF;
		}

		if (MRD || MWT)
			//if(0)
		{
			ADDR = DSP->MADRS[MASA];
			if (!TABLE)
				ADDR += DSP->DEC;
			if (ADREB)
				ADDR += ADRS_REG & 0x0FFF;
			if (NXADR)
				ADDR++;
			if (!TABLE)
				ADDR &= DSP->RBL - 1;
			else
				ADDR &= 0xFFFF;
			//ADDR<<=1;
			//ADDR+=DSP->RBP<<13;
			//MEMVAL=DSP->SCSPRAM[ADDR>>1];
			ADDR += DSP->RBP << 12;
			if (ADDR > 0x7ffff) ADDR = 0; //!! MAME has ADDR <<= 1 in here, but this seems to be wrong?
			if (MRD && (step & 1)) //memory only allowed on odd? DoA inserts NOPs on even
			{
				if (NOFL)
					MEMVAL = DSP->SCSPRAM[ADDR] << 8;
				else
					MEMVAL = UNPACK(DSP->SCSPRAM[ADDR]);
			}
			if (MWT && (step & 1))
			{
				if (NOFL)
					DSP->SCSPRAM[ADDR] = SHIFTED >> 8;
				else
					DSP->SCSPRAM[ADDR] = PACK(SHIFTED);
			}
		}

		if (ADRL)
		{
			if (SHIFT == 3)
				ADRS_REG = (SHIFTED >> 12) & 0xFFF;
			else
				ADRS_REG = (INPUTS >> 16);
		}

		if (EWT)
			DSP->EFREG[EWA] += SHIFTED >> 8;

	}
	--DSP->DEC;
	memset(DSP->MIXS, 0, 4 * 16);
}

void SCSPDSP_SetSample(_SCSPDSP *DSP, INT32 sample, int SEL, int MXL)
{
	//DSP->MIXS[SEL]+=sample<<(MXL+1)/*7*/;
	DSP->MIXS[SEL] += sample;
	//	if(MXL)
	//		int a=1;
}

void SCSPDSP_Start(_SCSPDSP *DSP)
{
	int i;
	DSP->Stopped = 0;
	for (i = 127; i >= 0; --i)
	{
		UINT16 *IPtr = DSP->MPRO + i * 4;

		if (IPtr[0] != 0 || IPtr[1] != 0 || IPtr[2] != 0 || IPtr[3] != 0)
			break;
	}
	DSP->LastStep = i + 1;

/*
	int test=0;
	if(test)
	{
		//test
		FILE *f1;
		f1=fopen("MPRO","wb");
		fwrite(DSP->MPRO,128*4*2,1,f1);
		fwrite(DSP->COEF,64*2,1,f1);
		fwrite(DSP->MADRS,32*2,1,f1);
		fclose(f1);
	}
*/

	for(int t=0;t<0x10000;++t)
	{
		signed int unp=UNPACK(t);
		unsigned short t2=PACK(unp);
		if(t2!=t)
			int a=1;
	}

#ifdef DYNDSP
	SCSPDSP_Recompile(DSP);
#endif
}