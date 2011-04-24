/*
 * Sega Model 3 Emulator
 * Copyright (C) 2003 Bart Trzynadlowski, Ville Linde, Stefano Teso
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License Version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program (license.txt); if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * scsiop.h
 *
 * NCR 53C810 SCSI controller SCRIPTS emulation. This file is intended to be
 * included by scsi.c only.
 */

#ifndef INCLUDED_SCSIOP_H
#define INCLUDED_SCSIOP_H

/*
 * Macros
 */

#define INSTRUCTION(name)   static BOOL name()

#define FETCH(dest) do { dest = read_32(REG32(0x2c)); REG32(0x2c) += 4; dest = BSWAP32(dest); } while (0);

#define DBC     (REG32(0x24) & 0xffffff)
#define DSPS    (REG32(0x30))
#define TEMP    (REG32(0x1c))


/*
 * Opcode Table
 *
 * Use the upper 8 bits of a SCRIPTS instruction as an index into this table.
 */

static BOOL (*op_table[256])();


/*
 * Move Memory
 */

#ifdef _PROFILE_
extern UINT is_dma;
#endif

INSTRUCTION(move_memory)
{
    UINT32  source = DSPS, dest;
    INT     num_bytes = DBC;

	PROFILE_SECT_ENTRY("scsi");

	#ifdef _PROFILE_
	is_dma = 1;
	#endif

    FETCH(TEMP);
    dest = TEMP;

//    message(0, "%08X (%08X): SCSI move memory: %08X->%08X, %X", ppc_get_reg(PPC_REG_PC), ppc_get_reg(PPC_REG_LR), source, dest, num_bytes);

    /*
     * Perform a 32-bit copy if possible and if necessary, finish off with a
     * byte-for-byte copy
     */

    /*for (i = 0; i < num_bytes / 4; i++)
    {
        write_32(dest, read_32(source));
        dest += 4;
        source += 4;
    }*/

	model3_dma_transfer(source, dest, num_bytes, FALSE);

    if (num_bytes & 3)
    {
		error("unaligned SCSI op !!!\n");
        num_bytes &= 3;
        while (num_bytes--)
            write_8(dest++, read_8(source++));
    }

	source += num_bytes;
	dest += num_bytes;

	#ifdef _PROFILE_
	is_dma = 0;
	#endif

    REG32(0x24) &= 0xff000000;  // update DBC (is any of this necessary???)
    DSPS = source;
    TEMP = dest;

	PROFILE_SECT_EXIT("scsi");

    return 0;
}

/*
 * INT and INTFLY
 */

INSTRUCTION(int_and_intfly)
{
    if ((DBC & 0x100000))   // INTFLY -- not yet emulated!
        return 1;

    if (((REG32(0x24) >> 21) & 1) || ((REG32(0x24) >> 18) & 1) || ((REG32(0x24) >> 17) & 1))
        return 1;           // conditional tests not yet emulated

    REG8(0x14) |= 0x01;     // DIP=1
    scripts_exec = 0;       // halt execution

    /*
     * NOTE: I haven't seen any games rely on this, so if you observe SCSI
     * problems, remove this to see if it helps.
     */

    REG8(0x14) |= 0x01;     // DIP=1
	REG8(0x0c) |= 0x04;		// SIR=1
	//model3_add_irq(0x60);
    ppc_set_irq_line(1);
    return 0;
}

/*
 * Invalid Instruction
 */

INSTRUCTION(invalid)
{    
//    message(0, "INVALID SCSI INSTRUCTION @ %08X", REG32(0x2c));
    return 1;
}

/*
 * void scsi_run(INT num);
 *
 * Runs SCSI SCRIPTS instructions until the code halts or the requested number
 * of instructions is executed.
 *
 * Parameters:
 *      num = Number of instructions to execute.
 */

void scsi_run(INT num)
{
    LOG("model3.log", "SCSI exec %08X\n", REG32(0x2c));
    if (!scripts_exec)
        return;

    if ((REG8(0x3B) & 0x10))    // single step mode
        num = 1;

    while (num-- && scripts_exec)
    {
        UINT32  save_addr = REG32(0x2c);    // for debugging

        FETCH(REG32(0x24)); // first double word into DCMD and DBC
        FETCH(DSPS);        // second double word into DSPS       

        if ((*op_table[REG32(0x24) >> 24])())
            error("SCSI: invalid instruction @ %08X", save_addr);
    }

    scripts_exec = 0;

    if ((REG8(0x3B) & 0x10))    // single step mode, trigger interrupt
    {        
        REG8(0x0c) |= 0x08;     // SSI=1
        REG8(0x14) |= 0x01;     // DIP=1
        ppc_set_irq_line(1);    // what about Model 3 IRQ status?
    }
}

/*
 * insert():
 *
 * Inserts an instruction into the appropriate elements of the opcode table
 * under the control of a mask.
 */

static void insert(UINT8 match, UINT8 mask, BOOL (*handler)())
{
    UINT32  i;

    for (i = 0; i < 256; i++)
    {
        if ((i & mask) == match)
            op_table[i] = handler;
    }
}

/*
 * build_opcode_table():
 *
 * Installs instruction handlers.
 */

static void build_opcode_table(void)
{
    insert(0, 0, &invalid);
    insert(0xc0, 0xfe, &move_memory);
    insert(0x98, 0xf8, &int_and_intfly);
}

#endif  // INCLUDED_SCSIOP_H
