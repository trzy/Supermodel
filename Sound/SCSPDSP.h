//#ifndef SCSPDSP_H
#define SCSPDSP_H

//#define DYNDSP
#define DYNOPT	1		//set to 1 to enable optimization of recompiler


//the DSP Context
struct _SCSPDSP
{
//Config
	unsigned short *SCSPRAM;
	unsigned int RBP;	//Ring buf pointer
	unsigned int RBL;	//Delay ram (Ring buffer) size in words

//context	
	
	signed short COEF[64];		//16 bit signed
	unsigned short MADRS[32];	//offsets (in words), 16 bit
	unsigned short MPRO[128*4];	//128 steps 64 bit
	signed int TEMP[128];	//TEMP regs,24 bit signed
	signed int MEMS[32];	//MEMS regs,24 bit signed
	unsigned int DEC;

//input
	signed int MIXS[16];	//MIXS, 24 bit signed
	signed short EXTS[2];	//External inputs (CDDA)	16 bit signed

//output
	signed short EFREG[16];	//EFREG, 16 bit signed
	
	bool Stopped;
	int LastStep;
#ifdef DYNDSP
	signed int ACC;	//26 bit
	signed int SHIFTED;	//24 bit
	signed int X;	//24 bit
	signed int Y;	//13 bit
	signed int B;	//26 bit
	signed int INPUTS;	//24 bit
	signed int MEMVAL;
	signed int FRC_REG;	//13 bit
	signed int Y_REG;		//24 bit
	unsigned int ADDR;
	unsigned int ADRS_REG;	//13 bit

	void (*DoSteps)();
#endif
};

void SCSPDSP_Init(_SCSPDSP *DSP);
void SCSPDSP_SetSample(_SCSPDSP *DSP,signed int sample,int SEL,int MXL);
void SCSPDSP_Step(_SCSPDSP *DSP);
void SCSPDSP_Start(_SCSPDSP *DSP);




//#endif