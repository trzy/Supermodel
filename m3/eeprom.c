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
 * 93C46 EEPROM emulation.
 */

/* rewrite me! */

#include "model3.h"

typedef struct EEPROM {

	UINT16 eeprom[64];	/* data buffer */
	UINT32 res;			/* result (ready/data) */
	UINT32 com;			/* current command */
	UINT32 addr;		/* current address */
	UINT32 latch;		/* current input bit data */
	UINT32 serial;		/* current serial data */
	UINT32 count;		/* current serial count */
	UINT32 locked;		/* lock status */

} EEPROM;

static EEPROM eeprom;

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

    fwrite(eeprom.eeprom, sizeof(UINT16), 64, fp);
    fwrite(&eeprom.res, sizeof(UINT32), 1, fp);
    fwrite(&eeprom.com, sizeof(UINT32), 1, fp);
    fwrite(&eeprom.addr, sizeof(UINT32), 1, fp);
    fwrite(&eeprom.latch, sizeof(UINT32), 1, fp);
    fwrite(&eeprom.serial, sizeof(UINT32), 1, fp);
    fwrite(&eeprom.count, sizeof(UINT32), 1, fp);
    fwrite(&eeprom.locked, sizeof(UINT32), 1, fp);
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
    fread(eeprom.eeprom, sizeof(UINT16), 64, fp);
    fread(&eeprom.res, sizeof(UINT32), 1, fp);
    fread(&eeprom.com, sizeof(UINT32), 1, fp);
    fread(&eeprom.addr, sizeof(UINT32), 1, fp);
    fread(&eeprom.latch, sizeof(UINT32), 1, fp);
    fread(&eeprom.serial, sizeof(UINT32), 1, fp);
    fread(&eeprom.count, sizeof(UINT32), 1, fp);
    fread(&eeprom.locked, sizeof(UINT32), 1, fp);
}

void eeprom_load(char * fn)
{
	FILE * f;

	if((f = fopen(fn, "rb")) != NULL)
	{
		if((int)fread(eeprom.eeprom, 2, 64, f) != 64*2)
			memset(eeprom.eeprom, 0, 64*2);

		fclose(f);
	}
}

void eeprom_save(char * fn)
{
	FILE * f;

	if((f = fopen(fn, "wb")) != NULL)
	{
		fwrite(eeprom.eeprom, 2, 64, f);
		fclose(f);
	}
}

void eeprom_reset(void)
{
	memset(eeprom.eeprom, 0, 64 * 2);

	eeprom.res = 1;
	eeprom.com = 0;
	eeprom.addr = 0;
	eeprom.latch = 0;
	eeprom.serial = 0;
	eeprom.count = 0;
	eeprom.locked = 0;
}

/******************************************************************/
/* Access                                                         */
/******************************************************************/

void eeprom_write(UINT8 we, UINT8 clk, UINT8 cs)
{
	if(we)
	{
		/* eeprom enabled */

		if(clk == 0)
		{
			/* no clock, write latch */

			eeprom.latch = cs;
			eeprom.res = 1;
			return;
		}

		/* pulse clock */

		eeprom.serial <<= 1;
		eeprom.serial |= eeprom.latch;
		eeprom.count++;
		eeprom.res = 1;

		LOG("error.log", "EEPROM : write - we=%i clk=%i cs=%i - latch=%i res=%i - cnt=%i ser=%X - com =%i\n",
		we, clk, cs, eeprom.latch, eeprom.res, eeprom.count, eeprom.serial, eeprom.com);

		if(eeprom.count == 4){

			/* command transferred, ignore MSB */

			eeprom.com = eeprom.serial & 7;

			if(	eeprom.com != 4 &&	/* extended */
				eeprom.com != 5 &&	/* write */
				eeprom.com != 6){	/* read */

				printf("ERROR: unknown eeprom com %i\n", eeprom.com);
				exit(1);

				LOG("error.log", "EEPROM : com=%i\n", eeprom.com);

			}

		}else
		if(eeprom.com == 4 && eeprom.count == 6){

			/* extended command transferred, mask in */

			eeprom.com = (eeprom.com << 2) | (eeprom.serial & 3);

			LOG("error.log", "EEPROM : com=%i (extended)\n", eeprom.com);

			switch(eeprom.com){

			case 16: eeprom.locked = 1; break; /* lock */
			case 19: eeprom.locked = 0; break; /* unlock */
			default:
				printf("ERROR: unknown eeprom ext com %i\n", eeprom.com);
				exit(1);

			}

		}else
		if((eeprom.com == 5 || eeprom.com == 6) && eeprom.count == 10){

			/* read/write address transferred */

			eeprom.addr = eeprom.serial & 63;

			if(eeprom.com == 5)
				eeprom.eeprom[eeprom.addr] = 0;

			LOG("error.log", "EEPROM : addr=%i\n", eeprom.addr);

		}else
		if(eeprom.com == 5 && eeprom.count > 10){

			/* writing */

			LOG("error.log", "EEPROM : writing bit %i = %i\n", 15 - (eeprom.count - 11), eeprom.latch);

			eeprom.eeprom[eeprom.addr] |= ((eeprom.latch & 1) << (15 - (eeprom.count - 11)));
			eeprom.res = 1;

			if(eeprom.count == 26){

				LOG("error.log", "EEPROM : write finished = %04X\n", eeprom.eeprom[eeprom.addr]);

				eeprom.com = 0;
				eeprom.addr = 0;
				eeprom.latch = 0;
				eeprom.serial = 0;
				eeprom.count = 0;
			}

		}else
		if(eeprom.com == 6 && eeprom.count > 10){

			/* reading */

			LOG("error.log", "EEPROM : reading bit %i\n", 15 - (eeprom.count - 11));

			eeprom.res = eeprom.eeprom[eeprom.addr];
			eeprom.res = (eeprom.res >> (15 - (eeprom.count - 11))) & 1;

			if(eeprom.count == 26){

				LOG("error.log", "EEPROM: read finished\n");

				eeprom.com = 0;
				eeprom.addr = 0;
				eeprom.latch = 0;
				eeprom.serial = 0;
				eeprom.count = 0;
			}
		}

	}else{

		/* eeprom disabled */

		LOG("error.log", "EEPROM : reset - we=%i clk=%i cs=%i - latch=%i res=%i - cnt=%i ser=%X - com =%i\n",
		we, clk, cs, eeprom.latch, eeprom.res, eeprom.count, eeprom.serial, eeprom.com);

		eeprom.res = 1;
		eeprom.com = 0;
		eeprom.addr = 0;
		eeprom.latch = 0;
		eeprom.serial = 0;
		eeprom.count = 0;
	}
}

UINT8 eeprom_read(void)
{
	LOG("error.log", "EEPROM : read result = %i\n", eeprom.res);

	return(eeprom.res);
}

