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
 C2000000			32-bit	-W		DMA Source
 C2000004			32-bit	-W		DMA Destination
 C2000008			32-bit	-W		DMA Length
 C200000C			8-bit	RW		DMA Status AxxxxxxB (A and B must be cleared)
 C200000D			8-bit	-W		DMA Control? xxxxxxx? (bit0 clears C200000C bit0)
 C200000E			8-bit	-W		DMA Config?
 C2000010			32-bit	-W		0x80000000 | (addr & 0xFC)
 C2000014			32-bit	R-		DMA Status?
*/

#include "model3.h"

/******************************************************************/
/* Private Variables                                              */
/******************************************************************/

static UINT8 dma_regs[0x40];

/******************************************************************/
/* Interface                                                      */
/******************************************************************/

void dma_shutdown(void)
{

}

void dma_init(void)
{

}

void dma_reset(void)
{

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
    // nothing to save currently...
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
    // nothing to load currently...
}

/******************************************************************/
/* Access                                                         */
/******************************************************************/

UINT8 dma_read_8(UINT32 a)
{
    return 0xFF;
}

UINT32 dma_read_32(UINT32 a)
{
    return 0xFFFFFFFF;
}

void dma_write_8(UINT32 a, UINT8 d)
{

}

void dma_write_32(UINT32 a, UINT32 d)
{

}

