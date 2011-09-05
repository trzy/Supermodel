//#ifndef SCSPDSP_H
#define SCSPDSP_H

//#define DYNDSP
#define DYNOPT	1		//set to 1 to enable optimization of recompiler


//the DSP Context
struct _SCSPDSP
{
//Config
	UINT16 *SCSPRAM;
	unsigned int RBP;	//Ring buf pointer
	unsigned int RBL;	//Delay ram (Ring buffer) size in words

//context	
	
	INT16 COEF[64];		//16 bit signed
	UINT16 MADRS[32];	//offsets (in words), 16 bit
	UINT16 MPRO[128*4];	//128 steps 64 bit
	INT32 TEMP[128];	//TEMP regs,24 bit signed
	INT32 MEMS[32];	//MEMS regs,24 bit signed
	unsigned int DEC;

//input
	INT32 MIXS[16];	//MIXS, 24 bit signed
	INT16 EXTS[2];	//External inputs (CDDA)	16 bit signed

//output
	INT16 EFREG[16];	//EFREG, 16 bit signed
	
	bool Stopped;
	int LastStep;
#ifdef DYNDSP
	INT32 ACC;	//26 bit
	INT32 SHIFTED;	//24 bit
	INT32 X;	//24 bit
	INT32 Y;	//13 bit
	INT32 B;	//26 bit
	INT32 INPUTS;	//24 bit
	INT32 MEMVAL;
	INT32 FRC_REG;	//13 bit
	INT32 Y_REG;		//24 bit
	UINT32 ADDR;
	UINT32 ADRS_REG;	//13 bit

	void (*DoSteps)();
#endif
};

void SCSPDSP_Init(_SCSPDSP *DSP);
void SCSPDSP_SetSample(_SCSPDSP *DSP,INT32 sample,int SEL,int MXL);
void SCSPDSP_Step(_SCSPDSP *DSP);
void SCSPDSP_Start(_SCSPDSP *DSP);




//#endif