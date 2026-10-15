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
 * 53C810Disasm.cpp
 *
 * NCR 53C810 SCRIPTS disassembler. Also covers the 53C825 (used by the
 * Real3D Pro-1000), which adds wide SCSI chained moves (CHMOV).
 *
 * All instruction types are decoded: block move, I/O (SELECT, WAIT SELECT,
 * DISCONNECT, SET, CLEAR, etc.), read/write register, transfer control (JUMP,
 * CALL, RETURN, INT, INTFLY), memory move, and load/store. The meaning of the
 * I/O instructions and phase comparisons depends on whether the chip is in
 * initiator or target mode, which must be specified.
 *
 * Syntax loosely follows the NCR assembler (NASM). Relative jump addresses are
 * printed as offsets followed by the absolute address they resolve to.
 */

#include <cstdio>
#include <cstring>
#ifdef STANDALONE
#include <cstdlib>
#endif
#include "Types.h"

#define DISASM_VERSION  "1.2"


/******************************************************************************
 Disassembler Function
******************************************************************************/

/*
 * Register names (53C810 and 53C825). Unnamed registers are printed as
 * hexadecimal addresses.
 */
static const char *s_regName[0x80] =
{
	"scntl0",   "scntl1",   "scntl2",   "scntl3",   "scid",     "sxfer",    "sdid",     "gpreg",	// 00
	"sfbr",     "socl",     "ssid",     "sbcl",     "dstat",    "sstat0",   "sstat1",   "sstat2",	// 08
	"dsa0",     "dsa1",     "dsa2",     "dsa3",     "istat",    NULL,       NULL,       NULL,		// 10
	"ctest0",   "ctest1",   "ctest2",   "ctest3",   "temp0",    "temp1",    "temp2",    "temp3",	// 18
	"dfifo",    "ctest4",   "ctest5",   "ctest6",   "dbc0",     "dbc1",     "dbc2",     "dcmd",		// 20
	"dnad0",    "dnad1",    "dnad2",    "dnad3",    "dsp0",     "dsp1",     "dsp2",     "dsp3",		// 28
	"dsps0",    "dsps1",    "dsps2",    "dsps3",    "scratcha0","scratcha1","scratcha2","scratcha3",// 30
	"dmode",    "dien",     "sbr",      "dcntl",    "adder0",   "adder1",   "adder2",   "adder3",	// 38
	"sien0",    "sien1",    "sist0",    "sist1",    "slpar",    "swide",    "macntl",   "gpcntl",	// 40
	"stime0",   "stime1",   "respid0",  "respid1",  "stest0",   "stest1",   "stest2",   "stest3",	// 48
	"sidl0",    "sidl1",    NULL,       NULL,       "sodl0",    "sodl1",    NULL,       NULL,		// 50
	"sbdl0",    "sbdl1",    NULL,       NULL,       "scratchb0","scratchb1","scratchb2","scratchb3",// 58
	NULL,       NULL,       NULL,       NULL,       NULL,       NULL,       NULL,       NULL,		// 60
	NULL,       NULL,       NULL,       NULL,       NULL,       NULL,       NULL,       NULL,		// 68
	NULL,       NULL,       NULL,       NULL,       NULL,       NULL,       NULL,       NULL,		// 70
	NULL,       NULL,       NULL,       NULL,       NULL,       NULL,       NULL,       NULL		// 78
};

static const char *s_phase[] = { "data_out", "data_in", "command", "status", "res4", "res5", "message_out", "message_in" };

static const char *RegName(unsigned reg, char *buf)
{
	reg &= 0x7F;
	if (s_regName[reg])
		return s_regName[reg];
	sprintf(buf, "reg%02X", reg);
	return buf;
}

// Sign-extends a 24-bit value
static INT32 SignExtend24(UINT32 x)
{
	return (INT32) ((x & 0x00FFFFFF) ^ 0x00800000) - 0x00800000;
}

// Formats a jump address, which is either absolute or relative to the next
// instruction (addr is the address of the next instruction)
static char *FormatAddress(char *p, UINT32 operand, bool relative, UINT32 next)
{
	if (relative)
	{
		INT32 offset = SignExtend24(operand);
		return p + sprintf(p, "rel(%c%X) ; -> %08X", offset < 0 ? '-' : '+', offset < 0 ? -offset : offset, next + offset);
	}
	return p + sprintf(p, "%08X", operand);
}

// Formats the condition of a transfer control instruction
static char *FormatCondition(char *p, UINT32 op, bool targetMode)
{
	bool carry = (op & 0x00200000) != 0;
	bool ifTrue = (op & 0x00080000) != 0;
	bool compData = (op & 0x00040000) != 0;
	bool compPhase = (op & 0x00020000) != 0;
	bool wait = (op & 0x00010000) != 0;

	if (!carry && !compData && !compPhase)
		return p + sprintf(p, ifTrue ? "" : ", never");

	p += sprintf(p, ", %s%s ", wait ? "when" : "if", ifTrue ? "" : " not");
	if (carry)
		return p + sprintf(p, "carry");
	if (compPhase)
	{
		// In target mode, the phase comparison tests ATN instead
		p += sprintf(p, "%s", targetMode ? "atn" : s_phase[(op >> 24) & 7]);
		if (compData)
			p += sprintf(p, " and ");
	}
	if (compData)
	{
		unsigned mask = (op >> 8) & 0xFF;
		p += sprintf(p, "%02X", op & 0xFF);
		if (mask)
			p += sprintf(p, " and mask %02X", mask);
	}
	return p;
}

/*
 * DisassembleSCRIPTS(op, addr, mnem, targetMode):
 *
 * Disassembles one SCRIPTS instruction.
 *
 * Parameters:
 *		op			Three consecutive words.
 *		addr		Current instruction address.
 *		mnem		Buffer to write instruction mnemonic to (at least 128 bytes).
 *					If no instruction was successfully decoded, will be set to
 *					'\0'.
 *		targetMode	If true, decodes instructions as executed in target mode
 *					(SCNTL0.TRG set), which changes the meaning of the I/O
 *					instructions, block move opcodes, and phase comparisons.
 *
 * Returns:
 *		Number of 32-bit words decoded (1, 2, or 3) or 0 if the instruction was
 *		not recognized.
 */
int DisassembleSCRIPTS(UINT32 op[3], UINT32 addr, char *mnem, bool targetMode)
{
	static const char	*ioInitiator[] = { "select", "wait disconnect", "wait reselect", "set", "clear" };
	static const char	*ioTarget[] = { "reselect", "disconnect", "wait select", "set", "clear" };
	static const char	*transfer[] = { "jump", "call", "return", "int", NULL, NULL, NULL, NULL };
	char				*p = mnem;
	char				regBuf[8];

	mnem[0] = '\0';

	switch (op[0] >> 30)
	{
	case 0:
	{
		// Block Move. The opcode bit selects MOVE in one mode and CHMOV (wide
		// SCSI chained move) in the other.
		bool indirect = (op[0] & 0x20000000) != 0;
		bool tableIndirect = (op[0] & 0x10000000) != 0;
		bool opcode = (op[0] & 0x08000000) != 0;
		const char *name = (opcode != targetMode) ? "move" : "chmov";
		const char *phase = s_phase[(op[0] >> 24) & 7];
		const char *with = targetMode ? "with" : "when";

		if (tableIndirect)
		{
			INT32 offset = SignExtend24(op[1]);
			sprintf(p, "%s from %c%X, %s %s", name, offset < 0 ? '-' : '+', offset < 0 ? -offset : offset, with, phase);
		}
		else
			sprintf(p, "%s %X, %s%08X, %s %s", name, op[0] & 0x00FFFFFF, indirect ? "ptr " : "", op[1], with, phase);
		return 2;
	}

	case 1:
	{
		unsigned opcode = (op[0] >> 27) & 7;
		if (opcode >= 5)
		{
			// Read/Write Register
			static const char *operation[] = { "", "shl", "|", "^", "&", "shr", "+", "+ (carry)" };
			unsigned oper = (op[0] >> 24) & 7;
			bool useSFBR = (op[0] & 0x00800000) != 0;
			unsigned data = (op[0] >> 8) & 0xFF;
			const char *reg = RegName((op[0] >> 16) & 0x7F, regBuf);
			char operand[32];

			if (useSFBR)
				sprintf(operand, "sfbr");
			else
				sprintf(operand, "%02X", data);

			if (opcode == 5)		// SFBR (op data) -> register
			{
				if (oper == 0)
					sprintf(p, "move %s to %s", useSFBR ? "sfbr" : operand, reg);
				else if (oper == 1 || oper == 5)
					sprintf(p, "move sfbr %s to %s", operation[oper], reg);
				else
					sprintf(p, "move sfbr %s %s to %s", operation[oper], operand, reg);
			}
			else if (opcode == 6)	// register (op data) -> SFBR
			{
				if (oper == 0)
					sprintf(p, "move %s to sfbr", operand);
				else if (oper == 1 || oper == 5)
					sprintf(p, "move %s %s to sfbr", reg, operation[oper]);
				else
					sprintf(p, "move %s %s %s to sfbr", reg, operation[oper], operand);
			}
			else					// register (op data) -> register
			{
				if (oper == 0)
					sprintf(p, "move %s to %s", operand, reg);
				else if (oper == 1 || oper == 5)
					sprintf(p, "move %s %s to %s", reg, operation[oper], reg);
				else
					sprintf(p, "move %s %s %s to %s", reg, operation[oper], operand, reg);
			}
			return 2;
		}

		// I/O
		const char *name = targetMode ? ioTarget[opcode] : ioInitiator[opcode];
		p += sprintf(p, "%s", name);
		if (opcode == 3 || opcode == 4)
		{
			// SET/CLEAR: list the affected signals and flags
			bool first = true;
			const struct { UINT32 bit; const char *name; } flags[] = { { 0x00000400, "carry" }, { 0x00000200, "target" }, { 0x00000040, "ack" }, { 0x00000008, "atn" } };
			for (auto &flag: flags)
			{
				if (op[0] & flag.bit)
				{
					p += sprintf(p, "%s%s", first ? " " : " and ", flag.name);
					first = false;
				}
			}
			return 2;
		}
		if (opcode == 1)
			return 2;	// (WAIT) DISCONNECT has no operands

		// SELECT, RESELECT, WAIT SELECT, WAIT RESELECT
		bool atn = (op[0] & 0x04000000) != 0;
		bool tableIndirect = (op[0] & 0x02000000) != 0;
		bool relative = (op[0] & 0x01000000) != 0;
		if (atn && opcode == 0)
			p += sprintf(p, " atn");
		if (opcode == 0)	// (RE)SELECT names a SCSI ID
		{
			if (tableIndirect)
			{
				INT32 offset = SignExtend24(op[0]);
				p += sprintf(p, " from %c%X,", offset < 0 ? '-' : '+', offset < 0 ? -offset : offset);
			}
			else
				p += sprintf(p, " id %u,", (op[0] >> 16) & 0xF);
		}
		p += sprintf(p, " ");
		FormatAddress(p, op[1], relative, addr + 8);
		return 2;
	}

	case 2:
	{
		// Transfer Control: JUMP, CALL, RETURN, INT, INTFLY
		unsigned opcode = (op[0] >> 27) & 7;
		bool relative = (op[0] & 0x00800000) != 0;
		if (!transfer[opcode])
			return 0;
		if (opcode == 3 && (op[0] & 0x00100000))
			p += sprintf(p, "intfly");
		else
			p += sprintf(p, "%s", transfer[opcode]);
		if (opcode == 3)
			p += sprintf(p, " %08X", op[1]);
		else if (opcode != 2)	// RETURN has no address
		{
			p += sprintf(p, " ");
			p = FormatAddress(p, op[1], relative, addr + 8);
		}
		FormatCondition(p, op[0], targetMode);
		return 2;
	}

	default:
	{
		if ((op[0] & 0xE0000000) == 0xC0000000)
		{
			// Memory Move
			sprintf(p, "move memory%s %X, %08X, %08X", (op[0] & 0x01000000) ? " no flush" : "", op[0] & 0x00FFFFFF, op[1], op[2]);
			return 3;
		}

		// Load/Store
		bool dsaRelative = (op[0] & 0x10000000) != 0;
		bool load = (op[0] & 0x01000000) != 0;
		bool noFlush = (op[0] & 0x02000000) != 0;
		unsigned count = op[0] & 7;
		const char *reg = RegName((op[0] >> 16) & 0x7F, regBuf);
		char where[32];
		if (dsaRelative)
		{
			INT32 offset = SignExtend24(op[1]);
			sprintf(where, "dsa%c%X", offset < 0 ? '-' : '+', offset < 0 ? -offset : offset);
		}
		else
			sprintf(where, "%08X", op[1]);
		if (load)
			sprintf(p, "load %s, %u, %s", reg, count, where);
		else
			sprintf(p, "store%s %s, %u, %s", noFlush ? " no flush" : "", reg, count, where);
		return 2;
	}
	}
}


/******************************************************************************
 Standalone Disassembler

 Define STANDALONE to build a command line-driven SCRIPTS disassembler.
******************************************************************************/

#ifdef STANDALONE

static void PrintUsage(void)
{
    puts("scriptsd Version " DISASM_VERSION " by Bart Trzynadlowski: NCR 53C810 SCRIPTS Disassembler");
    puts("Usage:    scriptsd <file> [options]");
    puts("Options:  -?,-h       Show this help text");
    puts("          -s <offset> Start offset (hexadecimal)");
    puts("          -l <num>    Number of instructions (Default=16)");
    puts("          -o <addr>   Set origin (hexadecimal)");
    puts("          -t          Decode as executed in target mode");
    exit(0);
}

/*
 * main(argc, argv):
 *
 * Standalone SCRIPTS disassembler.
 */
int main(int argc, char **argv)
{
    char    	mnem[128];
    FILE    	*fp;
    UINT8   	*buffer;
    unsigned	start = 0, len, org, file = 0;
    UINT32  	op[3];
    bool    	len_specified = 0, org_specified = 0, target_mode = 0;
    char    	*c;


    if (argc <= 1)
        PrintUsage();

    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-?"))
            PrintUsage();
        else if (!strcmp(argv[i], "-s"))
        {
            ++i;
            if (i >= argc)
                fprintf(stderr, "scriptsd: warning: no argument to %s\n", "-s");
            else
                start = strtoul(argv[i], &c, 16);
        }
        else if (!strcmp(argv[i], "-l"))
        {
            ++i;
            if (i >= argc)
                fprintf(stderr, "scriptsd: warning: no argument to %s\n", "-l");
            else
            {
                len = atoi(argv[i]);
                len_specified = 1;
            }
        }
        else if (!strcmp(argv[i], "-o"))
        {
            ++i;
            if (i >= argc)
                fprintf(stderr, "scriptsd: warning: no argument to %s\n", "-o");
            else
            {
                org = strtoul(argv[i], &c, 16);
                org_specified = 1;
            }
        }
        else if (!strcmp(argv[i], "-t"))
            target_mode = 1;
        else
            file = i;
    }

    if (!file)
    {
        fprintf(stderr, "scriptsd: no input file specified\n");
        exit(1);
    }

    /*
     * Load file
     */

    if ((fp = fopen(argv[file], "rb")) == NULL)
    {
        fprintf(stderr, "scriptsd: failed to open file: %s\n", argv[file]);
        exit(1);
    }
    fseek(fp, 0, SEEK_END);
    unsigned fsize = ftell(fp);
    rewind(fp);

    // Allocate some extra padding for operands
    if ((buffer = (UINT8 *) calloc(fsize+4*3, sizeof(UINT8))) == NULL)
    {
        fprintf(stderr, "scriptsd: not enough memory to load input file: %s, %lu bytes\n", argv[file], (unsigned long) fsize);
        fclose(fp);
        exit(1);
    }
    fread(buffer, sizeof(UINT8), fsize, fp);
    fclose(fp);

    if (!len_specified)
        len = 16;

    if (!org_specified)
        org = start;

    /*
     * Disassemble!
     */

	unsigned offset = start;
 	for (unsigned i = 0; (i<len) && (offset<fsize); i++)
 	{
 		op[0] = (buffer[offset+3]<<24)|(buffer[offset+2]<<16)|(buffer[offset+1]<<8)|buffer[offset+0];
 		op[1] = (buffer[offset+7]<<24)|(buffer[offset+6]<<16)|(buffer[offset+5]<<8)|buffer[offset+4];
 		op[2] = (buffer[offset+11]<<24)|(buffer[offset+10]<<16)|(buffer[offset+9]<<8)|buffer[offset+8];

 		unsigned num = DisassembleSCRIPTS(op, org, mnem, target_mode);
 		if (0 == num)
 		{
 			printf("%08X: %08X                   ?\n", org, op[0]);
 			offset += 4;
 			org += 4;
 		}
 		else
 		{
 			if (num == 3)
 				printf("%08X: %08X %08X %08X %s\n", org, op[0], op[1], op[2], mnem);
 			else if (num == 2)
 				printf("%08X: %08X %08X          %s\n", org, op[0], op[1], mnem);
 			else
 				printf("%08X: %08X                     %s\n", org, op[0], mnem);
 			offset += num*4;
 			org += num*4;
 		}
 	}

    free(buffer);

    return 0;
}

#endif
