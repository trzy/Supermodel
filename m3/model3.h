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
 * model3.h
 *
 * Model 3 header. This serves as the primary header file for all modules to
 * include.
 */

#ifndef INCLUDED_MODEL3_H
#define INCLUDED_MODEL3_H

/******************************************************************/
/* Program Information                                            */
/******************************************************************/

#define VERSION         "0.01"  // program version
#define VERSION_MAJOR   0x00    // major version number (BCD)
#define VERSION_MINOR   0x01    // minor version number (BCD)

/******************************************************************/
/* Includes                                                       */
/******************************************************************/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
//#include "zlib.h"

#include "osd.h"    // provided by the port to define data types, etc.

#include "ppc/ppc.h"

//#include "m68k/m68k.h"

#include "controls.h"
#include "dma.h"
#include "dsb1.h"
#include "eeprom.h"
#include "bridge.h"
#include "r3d.h"
#include "render.h"
#include "rtc.h"
#include "scsi.h"
#include "scsp.h"
#include "tilegen.h"

#include "osd_common.h" // defines OSD interface (must be included last)

/******************************************************************/
/* Helpful Macros                                                 */
/******************************************************************/

#define MODEL3_OKAY		0	/* Everything went okay */
#define MODEL3_ERROR	-1	/* Some error happened */

#define MODEL3_SCREEN_WIDTH		496
#define MODEL3_SCREEN_HEIGHT	384

#define SAFE_FREE(p)	{ if(p != NULL) free(p); p = NULL; }

#define PPC_PC  ppc_get_reg(PPC_REG_PC)
#define PPC_LR  ppc_get_reg(PPC_REG_LR)

INLINE UINT16 BSWAP16(UINT16 d)
{
	return(((d >> 8) & 0x00FF) |
		   ((d << 8) & 0xFF00));
}

INLINE UINT32 BSWAP32(UINT32 d)
{
	return(((d >> 24) & 0x000000FF) |
		   ((d >>  8) & 0x0000FF00) |
		   ((d <<  8) & 0x00FF0000) |
		   ((d << 24) & 0xFF000000));
}

/******************************************************************/
/* Configuration Structure                                        */
/******************************************************************/

/*
 * Game Flags
 *
 * Identify various properties that the games can have.
 */

#define GAME_OWN_STEERING_WHEEL (1 << 0)    // analog steering wheel
#define GAME_OWN_GUN            (1 << 1)    // light gun
#define GAME_OWN_DSB1           (1 << 2)    // DSB1 sound board

/*
 * CONFIG Structure
 *
 * Emulator configuration and game information.
 */

typedef struct
{
    /*
     * Emulator Configuration
     *
     * Determines the behavior of the emulator itself. These fields can be
     * loaded, saved, and modified. The user has control over these options.
     */

    BOOL    log_enabled;    // whether to log (no effect if _LOG_ not defined)
	BOOL	fullscreen;
	BOOL	triple_buffer;
	INT		width;
	INT		height;
	BOOL	stretch;
    FLAGS   layer_enable;

    /*
     * Game Configuration
     *
     * Information on the game currently being emulated. The user has no
     * control over this stuff -- it's internal to the emulator only.
     */

    CHAR    game_id[8+1];   // game ID string (max 8 chars + null terminator)
    INT     step;           // hardware step (0x15 = Step 1.5, etc.)
    FLAGS   flags;          // game info flags

} CONFIG;

/*
 * m3_config
 *
 * A global variable accessible by all parts of the emulator after emulation
 * begins successfully.
 */

extern CONFIG m3_config;

extern void message(UINT flags, char * fmt, ...);
extern void error(char * fmt, ...);

/*
 * Profile
 */

extern void profile_section_entry(CHAR * name);
extern void profile_section_exit(CHAR * name);
extern UINT64 profile_get_stat(CHAR * name);
extern void profile_reset_sect(CHAR * name);
extern void profile_cleanup(void);
extern void profile_print(CHAR * string);

#ifdef _PROFILE_
#define	PROFILE_SECT_ENTRY(n)	profile_section_entry(n);
#define PROFILE_SECT_EXIT(n)	profile_section_exit(n);
#define PROFILE_SECT_RESET(n)	profile_reset_sect(n);
#else // !_PROFILE_
#define PROFILE_SECT_ENTRY(n)
#define PROFILE_SECT_EXIT(n)
#define PROFILE_SECT_RESET(n)
#endif // _PROFILE_

/******************************************************************/
/* Functions                                                      */
/******************************************************************/

#ifdef _LOG_
#define LOG_INIT	_log_init
#define LOG			_log
#else   // !_LOG_
#define LOG_INIT
#define LOG
#endif  // _LOG

extern void _log(char * path, char * fmt, ...);
extern void _log_init(char * path);

extern void m3_init(void);
extern void m3_shutdown(void);

extern void m3_unload_rom(void);
extern BOOL m3_load_rom(CHAR *);

extern void m3_reset(void);
extern void m3_run_frame(void);
extern void m3_add_irq(UINT8);
extern void m3_remove_irq(UINT8);

extern BOOL m3_save_state(CHAR *);
extern BOOL m3_load_state(CHAR *);

extern void m3_load_eeprom(void);
extern void m3_save_eeprom(void);
extern void m3_load_bram(void);
extern void m3_save_bram(void);

/******************************************************************/

#endif  // INCLUDED_MODEL3_H
