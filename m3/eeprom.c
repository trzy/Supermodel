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

//TODO: Check to make sure code is functioning correctly. See note in
// eeprom_write()

/*
 * eeprom.c
 *
 * 93C46 EEPROM emulation.
 */

#include "model3.h"

#define LOG_EEPROM        // enable EEPROM logging

/******************************************************************/
/* Privates                                                       */
/******************************************************************/

typedef struct EEPROM {

	UINT32	buff[256];

	UINT	_di;
	UINT	_do;
	UINT	_cs;
	UINT	_clk;

	UINT	count;
	UINT32	serial;
	UINT	command;
	UINT	addr;

	UINT	locked;

} EEPROM;

static EEPROM	eeprom;

/******************************************************************/
/* Interface                                                      */
/******************************************************************/

/*
 * void eeprom_save_state(FILE *fp);
 *
 * Saves the state of the EEPROM to a file.
 *
 * Parameters:
 *      fp = File to save to.
 */

void eeprom_save_state(FILE *fp)
{
    /*
     * Each struct member is written out individually to prevent padding
     * issues to occur (we're using different compilers for development)
     */

	if(fp != NULL)
	{
		fwrite(eeprom.buff, sizeof(UINT32), 256, fp);
		fwrite(&eeprom._di, sizeof(UINT), 1, fp);
		fwrite(&eeprom._do, sizeof(UINT), 1, fp);
		fwrite(&eeprom._cs, sizeof(UINT), 1, fp);
		fwrite(&eeprom._clk, sizeof(UINT), 1, fp);
		fwrite(&eeprom.count, sizeof(UINT), 1, fp);
		fwrite(&eeprom.serial, sizeof(UINT32), 1, fp);
		fwrite(&eeprom.command, sizeof(UINT), 1, fp);
		fwrite(&eeprom.addr, sizeof(UINT), 1, fp);
		fwrite(&eeprom.locked, sizeof(UINT), 1, fp);
	}
}

/*
 * void eeprom_load_state(FILE *fp);
 *
 * Loads the state of the EEPROM from a file.
 *
 * Parameters:
 *      fp = File to load from.
 */

void eeprom_load_state(FILE *fp)
{
	if(fp != NULL)
	{
		fread(eeprom.buff, sizeof(UINT32), 256, fp);
		fread(&eeprom._di, sizeof(UINT), 1, fp);
		fread(&eeprom._do, sizeof(UINT), 1, fp);
		fread(&eeprom._cs, sizeof(UINT), 1, fp);
		fread(&eeprom._clk, sizeof(UINT), 1, fp);
		fread(&eeprom.count, sizeof(UINT), 1, fp);
		fread(&eeprom.serial, sizeof(UINT32), 1, fp);
		fread(&eeprom.command, sizeof(UINT), 1, fp);
		fread(&eeprom.addr, sizeof(UINT), 1, fp);
		fread(&eeprom.locked, sizeof(UINT), 1, fp);
	}
}

INT eeprom_load(char * fn)
{
	FILE * f;
	INT i = MODEL3_ERROR;

    message(0, "loading EEPROM from %s", fn);

	if((f = fopen(fn, "rb")) != NULL)
	{
		i = fread(eeprom.buff, sizeof(UINT32), 256, f);
		fclose(f);

		if(i == 256)
			return(MODEL3_OKAY);
	}

	memset(eeprom.buff, 0xFF, sizeof(UINT32)*256);

	return(i);
}

INT eeprom_save(char * fn)
{
	FILE * f;
	INT i = MODEL3_ERROR;

	message(0, "saving EEPROM to %s", fn);

	if((f = fopen(fn, "wb")) != NULL)
	{
		i = fwrite(eeprom.buff, sizeof(UINT32), 256, f);
		fclose(f);

		if(i == 256)
			return(MODEL3_OKAY);
	}

	return(i);
}

void eeprom_reset(void)
{
	memset(&eeprom, 0, sizeof(EEPROM));
	memset(eeprom.buff, 0xFF, sizeof(UINT32)*256);
	eeprom.locked = 1;
}


/******************************************************************/
/* Access                                                         */
/******************************************************************/

void eeprom_write(UINT cs, UINT clk, UINT di, UINT we)
{
	#ifdef LOG_EEPROM
//    message(0, "EEPROM write: cs=%i clk=%i di=%i we=%i", cs, clk, di, we);
	#endif

    //
    // NOTE: I removed this because the behavior is almost certainly wrong.
    // CS is held during the duration of any operation and at the end of the
    // operation, we should clear count and possibly command.
    //
    // I'm not sure if the code is bug-free. Some checking and revision may
    // be required.
    //
#if 0
	if(cs)
	{
		// reset

        eeprom.count = 0;
		eeprom.command = 0;
		return;
	}
#endif

	if(!we)
		return;

	if(clk && !eeprom._clk)
	{
		eeprom.serial <<= 1;
		eeprom.serial |= (di) ? 1 : 0;

		#ifdef LOG_EEPROM
//        message(0, "EEPROM serial = %08X [%i]", eeprom.serial, eeprom.count);
		#endif

		eeprom.count++;

		if(eeprom.count == 4)
		{
			// command sent

			eeprom.command = eeprom.serial & 7;

#if 0
			switch(eeprom.command)
			{
			case 0x6:
				#ifdef LOG_EEPROM
                message(0, "EEPROM read");
				#endif
				break;
			case 0x5:
				#ifdef LOG_EEPROM
                message(0, "EEPROM write");
				#endif
				if(eeprom.locked)
					error("EEPROM locked write\n");
				break;
			case 0x7:
				error("EEPROM erase\n");
			case 0x4:
				#ifdef LOG_EEPROM
				message(0, "EEPROM ex\n");
				#endif
				break;
			default:
				error("EEPROM invalid (%X)\n", eeprom.command);
			}
#endif
		}
		else if(eeprom.command == 4 && eeprom.count == 10)
		{
			// extended command sent

			eeprom.command = (eeprom.serial >> 4) & 0x1F;

			switch(eeprom.command)
			{
			case 0x10:	// EEPROM lock
				#ifdef LOG_EEPROM
				message(0, "EEPROM lock");
				#endif
				eeprom.locked = 1;
				break;
			case 0x13:	// EEPROM unlock
				#ifdef LOG_EEPROM
				message(0, "EEPROM unlock");
				#endif
				eeprom.locked = 0;
				break;
			default:
				error("EEPROM: invalid extended (%X)\n", eeprom.command);
			}

			eeprom.count = 0;
			eeprom.serial = 0;
		}
		else if(eeprom.command == 5)
		{
			// EEPROM write

			if(eeprom.count == (4+6))
			{
				eeprom.addr = eeprom.serial & 0x3F;

				#ifdef LOG_EEPROM
				message(0, "EEPROM write, addr=%02X", eeprom.addr);
				#endif
			}
			else if(eeprom.count == (4+6+16))
			{
				eeprom.buff[eeprom.addr] = eeprom.serial & 0xFFFF;
				eeprom.count = 0;
				eeprom.serial = 0;

				#ifdef LOG_EEPROM
				message(0, "EEPROM write, addr=%02X, data=%04X", eeprom.addr, eeprom.buff[eeprom.addr]);
				#endif
			}
		}
		else if(eeprom.command == 6)
		{
			// EEPROM read

			if(eeprom.count == 4+6)
			{
				eeprom.addr = eeprom.serial & 0x3F;

				#ifdef LOG_EEPROM
				message(0, "EEPROM read, addr=%02X", eeprom.addr);
				#endif
			}
			else if(eeprom.count > (4+6) && eeprom.count <= (4+6+16))
			{
				#ifdef LOG_EEPROM
				message(0, "EEPROM read, addr=%02X, reading bit %i", eeprom.addr, eeprom.count-(4+6+1));
				#endif

				eeprom._do = (eeprom.buff[eeprom.addr] >> (15 - (eeprom.count-(4+6+1)))) & 1;

				if(eeprom.count == (4+6+16))
				{
					#ifdef LOG_EEPROM
					message(0, "EEPROM read, addr=%02X, finished", eeprom.addr);
					#endif

					eeprom.count = 0;
					eeprom.serial = 0;
				}

				goto eeprom_done;
			}
		}
	}

	eeprom._do = 1;

eeprom_done:

	eeprom._di = di;
	eeprom._cs = cs;
	eeprom._clk = clk;
}

UINT8 eeprom_read(void)
{
	#ifdef LOG_EEPROM
//    message(0, "EEPROM read");
	#endif

	return(eeprom._do);
}
