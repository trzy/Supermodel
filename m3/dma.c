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
 * dma.c
 *
 * Step 2.0+ DMA device emulation. We're unsure whether or not this is a SCSI
 * controller; it appears to only be used for its DMA capabilities and is
 * thought to be a Sega custom part.
 */

/*
 * 0xC2000000   32-bit  -W      DMA Source
 * 0xC2000004   32-bit  -W      DMA Destination
 * 0xC2000008   32-bit  -W      DMA Length (in words)
 * 0xC200000C   8-bit   RW      DMA Status? AxxxxxxB (A and B must be cleared)
 * 0xC200000D   8-bit   -W      DMA Control? xxxxxxx? (bit0 clears C200000C bit0)
 * 0xC200000E   8-bit   -W      DMA Config?
 * 0xC2000010   32-bit  -W      DMA Command? (Written, result appears at 0x14)
 * 0xC2000014   32-bit  R-      DMA Command Result
 */

#include "model3.h"

/******************************************************************/
/* Private Variables                                              */
/******************************************************************/

/*
 * Registers
 */

static UINT8 dma_regs[0x20];

/*
 * Read/Write Handlers
 *
 * These are used as an interface to the rest of the system.
 */

static UINT32   (*read_32)(UINT32);
static void     (*write_32)(UINT32, UINT32);

/******************************************************************/
/* DMA Transfer                                                   */
/******************************************************************/

/*
 * do_dma():
 *
 * Performs a DMA transfer. The paramaters are expected to be in the
 * appropriate registers.
 */

static void do_dma(void)
{
    UINT32  src, dest, len;

    src = *(UINT32 *) &dma_regs[0x00];
    dest = *(UINT32 *) &dma_regs[0x04];
    len = (*(UINT32 *) &dma_regs[0x08]) * 4;

    while (len)
    {
        write_32(dest, read_32(src));
        src += 4;
        dest += 4;
        len -= 4;
    }

    *(UINT32 *) &dma_regs[0x08] = 0;    // not sure if this is necessary
}

/******************************************************************/
/* Interface                                                      */
/******************************************************************/

/*
 * void dma_shutdown(void);
 *
 * Shuts down the DMA device emulation.
 */

void dma_shutdown(void)
{
}

/*
 * void dma_init(UINT32 (*read32)(UINT32), void (*write32)(UINT32, UINT32));
 *
 * Initializes the DMA device emulation by setting the memory access handlers.
 *
 * Parameters:
 *      read32  = Handler to use for 32-bit reads.
 *      write32 = Handler to use for 32-bit writes.
 */

void dma_init(UINT32 (*read32)(UINT32), void (*write32)(UINT32, UINT32))
{
    read_32 = read32;
    write_32 = write32;
}

/*
 * void dma_reset(void);
 *
 * Resets the DMA device.
 */

void dma_reset(void)
{
    memset(dma_regs, 0, 0x20);
}

/*
 * void dma_save_state(FILE *fp);
 *
 * Saves the state of the DMA device (for Step 2.0 or higher games only) to a
 * file.
 *
 * Parameters:
 *      fp = File to save to.
 */

void dma_save_state(FILE *fp)
{
    if (m3_config.step < 0x20)
        return;
    fwrite(dma_regs, sizeof(UINT8), 0x20, fp);
}

/*
 * void dma_load_state(FILE *fp);
 *
 * Loads the state of the DMA device (for Step 2.0 or higher games only) from
 * a file.
 *
 * Parameters:
 *      fp = File to load from.
 */

void dma_load_state(FILE *fp)
{
    if (m3_config.step < 0x20)
        return;
    fread(dma_regs, sizeof(UINT8), 0x20, fp);
}

/******************************************************************/
/* Access                                                         */
/******************************************************************/

/*
 * UINT8 dma_read_8(UINT32 a);
 *
 * Reads a byte from the DMA device.
 *
 * Parameters:
 *      a = Address.
 *
 * Returns:
 *      Data read.
 */

UINT8 dma_read_8(UINT32 a)
{
    return dma_regs[a & 0x1F];
}

/*
 * UINT32 dma_read_32(UINT32 a);
 *
 * Reads a word from the DMA device.
 *
 * Parameters:
 *      a = Address.
 *
 * Returns:
 *      Data read.
 */

UINT32 dma_read_32(UINT32 a)
{
    return BSWAP32(*(UINT32 *) &dma_regs[a & 0x1F]);
}

/*
 * void dma_write_8(UINT32 a, UINT8 d);
 *
 * Writes a byte to the DMA device.
 *
 * Parameters:
 *      a = Address.
 *      d = Data to write.
 */

void dma_write_8(UINT32 a, UINT8 d)
{
    dma_regs[a & 0x1F] = d;
//    message(0, "%08X: Unknown DMA write8, %08X = %02X", ppc_get_reg(PPC_REG_PC), a, d);
}

/*
 * void dma_write_32(UINT32 a, UINT32 d);
 *
 * Writes a word to the DMA device.
 *
 * Parameters:
 *      a = Address.
 *      d = Data to write.
 */

void dma_write_32(UINT32 a, UINT32 d)
{
    d = BSWAP32(d); // this is a little endian device, only bswap here once

    *(UINT32 *) &dma_regs[a & 0x1F] = d;

    switch (a & 0x1F)
    {
    case 0x00:  // DMA Source
//        message(0, "DMA SRC = %08X", d);
        return;
    case 0x04:  // DMA Destination
//        message(0, "DMA DST = %08X", d);
        return;
    case 0x08:  // DMA Length
//        message(0, "DMA LEN = %X", d * 4);
        do_dma();
        return;
    case 0x10:  // DMA Command

        /*
         * Virtual On 2 has been observed to write commands to reg 0x10 and
         * then expects particular values back from 0x14.
         *
         * Command 0x80000000 is a little strange. It is issued and twice and
         * each time, a bit of the result is expected to be different. This
         * is crudely simulated by flipping a bit.
         */

        if (d == 0x20000000)
            *(UINT32 *) &dma_regs[0x14] = 0x16C311DB;
        else if (d == 0x80000000)
        {
            static UINT32   result = 0x02000000;
            result ^= 0x02000000;
            *(UINT32 *) &dma_regs[0x14] = result;
        }
        else
            message(0, "%08X: Unknown DMA command, %08X", ppc_get_reg(PPC_REG_PC), d);

        return;
    }

    message(0, "%08X: Unknown DMA write32, %08X = %08X", ppc_get_reg(PPC_REG_PC), a, d);
}

