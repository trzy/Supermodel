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
 * eeprom.c
 *
 * 93C46 EEPROM emulation courtesy of R. Belmont.
 *
 * NOTE: Save state code assumes enums will be the same across all compilers
 * and systems. The eeprom_store[] array is endian-dependent.
 */

/***************************************************************************
 eeprom.cpp  - handles a serial eeprom with 64 16-bit addresses and 4-bit commands
 	       as seen in st-v, system 32, many konami boards, etc.

	       originally written: May 20, 2000 by R. Belmont for "sim"
	       update: Jan 27, 2001 (RB): implemented "erase" opcode,
	       				  made opcode fetch check for start bit
					  like the real chip.
 		       Mar 26, 2001 (FF): added get_image to access the eeprom
		       Jan 28, 2004 (Bart): Interface changes to work with Supermodel.
                      Crudely simulated busy status by setting olatch to 0 in
                      ES_DESELECT.
 ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "model3.h"

#define LOG_EEPROM 0		// log channel for EEPROM

static INT32    eeprom_state;

static UINT16   eeprom_store[64];   // 6 address bits times 16 bits per location
static UINT8    opcode;     // current opcode
static UINT8    adr;        // current address in eeprom_store
static UINT8    curbit;     // current bit # in read/write/addr fetch operation
static UINT8    lastclk;    // last clock
static UINT8    dlatch;     // input data latch
static UINT8    olatch;     // output data latch
static UINT16   curio;

// states for the eeprom state machine
enum
{
    ES_DESELECT = 0,// chip select not asserted
	ES_ASSERT,		// chip select asserted, waiting for opcode
	ES_OPCODE0,		// got opcode bit 0
	ES_OPCODE1,		// got opcode bit 1
	ES_OPCODE2,		// got opcode bit 2
	ES_ADRFETCH,	// fetching address
	ES_READBITS,	// reading bits, 1 out every time clock goes high
	ES_WRITEBITS,	// writing bits, 1 in every time clock goes high
    ES_WAITING      // done with read/write, waiting for deselect to finalize
};

/*
 * UINT8 eeprom_read_bit(void);
 *
 * Reads a data bit from the EEPROM.
 *
 * Returns:
 *      Latched data bit.
 */

UINT8 eeprom_read_bit(void)
{
	return olatch;
}

/*
 * void eeprom_set_ce(UINT8 ce);
 *
 * Passes the status of the chip enable.
 *
 * Parameters:
 *      ce = Chip enable bit (must be 0 or 1.)
 */

void eeprom_set_ce(UINT8 ce)
{
	// chip enable
	if (ce)
	{
		// asserting CE - if deselected,
		// select.  if selected already, 
		// ignore
		if (eeprom_state == ES_DESELECT)
		{
//			printf("EEPROM: asserting chip enable\n");
            eeprom_state = ES_ASSERT;
			opcode = 0;
		}
	}
	else
	{
		// deasserting CE
//		printf("EEPROM: deasserting chip enable\n");
        eeprom_state = ES_DESELECT;
	}
}

/*
 * void eeprom_set_data(UINT8 data);
 *
 * Sets the data bit latch.
 *
 * Parameters:
 *      data = Data bit (must be 0 or 1.)
 */

void eeprom_set_data(UINT8 data)
{
	dlatch = (data&0x01);	// latch the data
}

/*
 * void eeprom_set_clk(UINT8 clk);
 *
 * Sets the clock pin.
 *
 * Parameters:
 *      clk = Clock (zero or non-zero.)
 */

void eeprom_set_clk(UINT8 clk)
{
//	logWrite(LOG_EEPROM, "EEPROM: clock %d, data %d, state %d\n", clk, dlatch, eeprom_state);
	// things only happen on the rising edge of the clock
	if ((!lastclk) && (clk))
	{
		switch (eeprom_state)
		{
			case ES_DESELECT:
                olatch = 0;
				break;

			case ES_ASSERT:
			case ES_OPCODE0:
			case ES_OPCODE1:
			case ES_OPCODE2:
				// first command bit must be a "1" in the real device
				if ((eeprom_state == ES_OPCODE0) && (dlatch == 0))
				{
                    LOG("model3.log", "EEPROM: waiting for start bit\n");
                    olatch = 1; // assert READY
				}
				else
				{
					opcode <<= 1;
					opcode |= dlatch;
                    LOG("model3.log", "EEPROM: opcode fetch state, currently %x\n", opcode);
					eeprom_state++;
				}

				curbit = 0;
				adr = 0;
				break;

			case ES_ADRFETCH:	// fetch address
				adr <<= 1;
				adr |= dlatch;
                LOG("model3.log", "EEPROM: address fetch state, cur addr = %x\n", adr);
				curbit++;
				if (curbit == 6)
				{
					switch (opcode)
					{
						case 4:	// write enable - go back to accept a new opcode immediately
							opcode = 0;
							eeprom_state = ES_ASSERT;
							printf("EEPROM: write enable\n");
							break;

						case 5: // write
							eeprom_state = ES_WRITEBITS;
							olatch = 0;	// indicate not ready yet
							curbit = 0;
							curio = 0;
							break;

						case 6:	// read
							eeprom_state = ES_READBITS;
							curbit = 0;
							curio = eeprom_store[adr];
							printf("EEPROM: read %04x from address %d\n", curio, adr);
							break;

						case 7:	// erase location to ffff and assert READY
							eeprom_store[adr] = 0xffff;
							olatch = 1;
                            eeprom_state = ES_WAITING;
							printf("EEPROM: erase at address %d\n", adr);
							break;

						default:
                            LOG("model3.log", "Unknown EEPROM opcode %d!\n", opcode);
                            eeprom_state = ES_WAITING;
							olatch = 1;	// assert READY anyway
							break;
					}
				}
				break;

			case ES_READBITS:
				olatch = ((curio & 0x8000)>>15);
				curio <<= 1;

				curbit++;
				if (curbit == 16)
				{
                    eeprom_state = ES_WAITING;
				}
				break;

			case ES_WRITEBITS:
				curio <<= 1;
				curio |= dlatch;

				curbit++;
				if (curbit == 16)
				{
                    olatch = 1;     // indicate success
					eeprom_store[adr] = curio;
                    LOG("model3.log", "EEPROM: Wrote %04x to address %d\n", curio, adr);
                    eeprom_state = ES_WAITING;
				}
				break;

            case ES_WAITING:
				break;
		}
	}

	lastclk = clk;
}

/*
 * void eeprom_save_state(FILE *fp);
 *
 * Save the EEPROM state.
 *
 * Parameters:
 *      fp = File to save to.
 */

void eeprom_save_state(FILE *fp)
{
    if (fp != NULL)
    {
        fwrite(&eeprom_state, sizeof(INT32), 1, fp);
        fwrite(eeprom_store, sizeof(UINT16), 64, fp);
        fwrite(&opcode, sizeof(UINT8), 1, fp);
        fwrite(&adr, sizeof(UINT8), 1, fp);
        fwrite(&curbit, sizeof(UINT8), 1, fp);
        fwrite(&lastclk, sizeof(UINT8), 1, fp);
        fwrite(&dlatch, sizeof(UINT8), 1, fp);
        fwrite(&olatch, sizeof(UINT8), 1, fp);
        fwrite(&curio, sizeof(UINT16), 1, fp);
    }
}

/*
 * void eeprom_load_state(FILE *fp);
 *
 * Load the EEPROM state.
 *
 * Parameters:
 *      fp = File to load from.
 */

void eeprom_load_state(FILE *fp)
{
    if (fp != NULL)
    {
        fread(&eeprom_state, sizeof(INT32), 1, fp);
        fread(eeprom_store, sizeof(UINT16), 64, fp);
        fread(&opcode, sizeof(UINT8), 1, fp);
        fread(&adr, sizeof(UINT8), 1, fp);
        fread(&curbit, sizeof(UINT8), 1, fp);
        fread(&lastclk, sizeof(UINT8), 1, fp);
        fread(&dlatch, sizeof(UINT8), 1, fp);
        fread(&olatch, sizeof(UINT8), 1, fp);
        fread(&curio, sizeof(UINT16), 1, fp);
    }
}

/*
 * INT eeprom_load(CHAR *fname);
 *
 * Loads data from the file specified. If the file does not exist, the EEPROM
 * data is initialized with all 1's.
 *
 * Parameters:
 *      fname = File name.
 *
 * Returns:
 *      MODEL3_OKAY  = Success.
 *      MODEL3_ERROR = Error reading from the file.
 */

INT eeprom_load(CHAR *fname)
{
    FILE    *fp;
    INT     error_code = MODEL3_ERROR;

    message(0, "loading EEPROM from %s", fname);

    if ((fp = fopen(fname, "rb")) != NULL)
	{
        error_code = fread(eeprom_store, sizeof(UINT16), 64, fp);
        fclose(fp);

        if (error_code == 64)
            return MODEL3_OKAY;
	}

    memset(eeprom_store, 0xFF, sizeof(UINT16) * 64);

    return error_code;
}

/*
 * INT eeprom_save(CHAR *fname);
 *
 * Writes out the EEPROM data to the specified file.
 *
 * Parameters:
 *      fname = File name to write.
 *
 * Returns:
 *      MODEL3_OKAY  = Success.
 *      MODEL3_ERROR = Unable to open or write file.
 */

INT eeprom_save(CHAR *fname)
{
    FILE    *fp;
    INT     i = MODEL3_ERROR;

    message(0, "saving EEPROM to %s", fname);

    if ((fp = fopen(fname, "wb")) != NULL)
	{
        i = fwrite(eeprom_store, sizeof(UINT16), 64, fp);
        fclose(fp);

        if (i == 64)
            return MODEL3_OKAY;
	}

    return i;
}

/*
 * void eeprom_reset(void);
 *
 * Resets the EEPROM emulation.
 */

void eeprom_reset(void)
{
	eeprom_state = ES_DESELECT;
	lastclk = 0;
}
