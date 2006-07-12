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

#include "ppc_itp/ppc.h"

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
#include "file.h"

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

#ifdef _MSC_VER

#define BSWAP16(x) _byteswap_ushort(x)
#define BSWAP32(x) _byteswap_ulong(x)

#else

static UINT16 BSWAP16(UINT16 d)
{
	return(((d >> 8) & 0x00FF) |
		   ((d << 8) & 0xFF00));
}

static UINT32 BSWAP32(UINT32 d)
{
	return(((d >> 24) & 0x000000FF) |
		   ((d >>  8) & 0x0000FF00) |
		   ((d <<  8) & 0x00FF0000) |
		   ((d << 24) & 0xFF000000));
}

#endif

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

typedef enum
{
	P1_BUTTON_1,
	P1_BUTTON_2,
	P1_BUTTON_3,
	P1_BUTTON_4,
	P1_BUTTON_5,
	P1_BUTTON_6,
	P1_BUTTON_7,
	P1_BUTTON_8,
	P2_BUTTON_1,
	P2_BUTTON_2,
	P2_BUTTON_3,
	P2_BUTTON_4,
	P2_BUTTON_5,
	P2_BUTTON_6,
	P2_BUTTON_7,
	P2_BUTTON_8,
	P1_JOYSTICK_UP,
	P1_JOYSTICK_DOWN,
	P1_JOYSTICK_LEFT,
	P1_JOYSTICK_RIGHT,
	P2_JOYSTICK_UP,
	P2_JOYSTICK_DOWN,
	P2_JOYSTICK_LEFT,
	P2_JOYSTICK_RIGHT,
} GAME_BUTTON;

typedef enum
{
	ANALOG_AXIS_1,
	ANALOG_AXIS_2,
	ANALOG_AXIS_3,
	ANALOG_AXIS_4,
	ANALOG_AXIS_5,
	ANALOG_AXIS_6,
	ANALOG_AXIS_7,
	ANALOG_AXIS_8,
} GAME_ANALOG;

typedef struct
{
	struct
	{
		UINT8 control_set;
		UINT8 control_bit;
		GAME_BUTTON mapping;
		int enabled;
	} button[16];

	struct
	{
		GAME_ANALOG mapping;
		int enabled;
		UINT8 center;
	} analog_axis[8];

} GAME_CONTROLS;

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
	CHAR	rom_path[512];
	CHAR	rom_list[512];
	CHAR	backup_path[512];
	BOOL	fps_limit;
	BOOL	show_fps;

    /*
     * Game Configuration
     *
     * Information on the game currently being emulated. The user has no
     * control over this stuff -- it's internal to the emulator only.
     */

    CHAR    game_id[8+1];   // game ID string (max 8 chars + null terminator)
	CHAR	game_name[128];
    INT     step;           // hardware step (0x15 = Step 1.5, etc.)
    INT     bridge;         // type of PCIBMC (1=MPC105, 2=MPC106)
    FLAGS   flags;          // game info flags
	BOOL	has_lightgun;

	// Game controls
	GAME_CONTROLS controls;

} CONFIG;

enum
{
	ROMTYPE_NONE = 0,
	ROMTYPE_PROG0 = 1,
	ROMTYPE_PROG1,
	ROMTYPE_PROG2,
	ROMTYPE_PROG3,
	ROMTYPE_CROM00,
	ROMTYPE_CROM01,
	ROMTYPE_CROM02,
	ROMTYPE_CROM03,
	ROMTYPE_CROM10,
	ROMTYPE_CROM11,
	ROMTYPE_CROM12,
	ROMTYPE_CROM13,
	ROMTYPE_CROM20,
	ROMTYPE_CROM21,
	ROMTYPE_CROM22,
	ROMTYPE_CROM23,
	ROMTYPE_CROM30,
	ROMTYPE_CROM31,
	ROMTYPE_CROM32,
	ROMTYPE_CROM33,
	ROMTYPE_VROM00,
	ROMTYPE_VROM01,
	ROMTYPE_VROM02,
	ROMTYPE_VROM03,
	ROMTYPE_VROM04,
	ROMTYPE_VROM05,
	ROMTYPE_VROM06,
	ROMTYPE_VROM07,
	ROMTYPE_VROM10,
	ROMTYPE_VROM11,
	ROMTYPE_VROM12,
	ROMTYPE_VROM13,
	ROMTYPE_VROM14,
	ROMTYPE_VROM15,
	ROMTYPE_VROM16,
	ROMTYPE_VROM17,
	ROMTYPE_SPROG,
	ROMTYPE_SROM0,
	ROMTYPE_SROM1,
	ROMTYPE_SROM2,
	ROMTYPE_SROM3,
	ROMTYPE_DSBPROG,
	ROMTYPE_DSBROM0,
	ROMTYPE_DSBROM1,
	ROMTYPE_DSBROM2,
	ROMTYPE_DSBROM3,
};

typedef struct
{
	int		type;
    char	name[20];
    int		size;
    UINT32	crc32;
	int		load;
} ROMFILE;

typedef struct {
    UINT32  address;
    UINT32  data;
    INT     size;   // 8, 16, or 32
} PATCH;

typedef struct
{
	char	game[20];
	char	parent[20];
	char	title[256];
	char	manufacturer[32];
    int		year;
    int		step;
    int     bridge;
	int		cromsize;
    FLAGS	flags;
	int		num_patches;
	PATCH	patch[64];

	ROMFILE rom[64];

	GAME_CONTROLS controls;

} ROMSET;

typedef struct
{
	char id[60];
	UINT32 int_id;
} STRING_ID;

static int get_string_id(const char *string, STRING_ID *idtable)
{
	int i=0;
	
	while (strlen(idtable[i].id) > 0)
	{
		if (_stricmp(string, idtable[i].id) == 0)
		{
			return idtable[i].int_id;
		}
		i++;
	}

	return -1;
}

/*
 * m3_config
 *
 * A global variable accessible by all parts of the emulator after emulation
 * begins successfully.
 */

extern CONFIG m3_config;

extern void message(int, char * fmt, ...);
extern void error(char * fmt, ...);

extern BOOL parse_config(const char *config_name);
extern int parse_romlist(char *romlist_name, ROMSET *_romset);

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

extern BOOL model3_init(void);
extern void model3_shutdown(void);

extern BOOL model3_load(void);

extern void model3_reset(void);
extern void model3_run_frame(void);
extern void model3_add_irq(UINT8);
extern void model3_remove_irq(UINT8);

extern BOOL model3_save_state(CHAR *);
extern BOOL model3_load_state(CHAR *);

extern void model3_load_eeprom(void);
extern void model3_save_eeprom(void);
extern void model3_load_bram(void);
extern void model3_save_bram(void);

extern UINT8 m3_ppc_read_8(UINT32 a);
extern UINT16 m3_ppc_read_16(UINT32 a);
extern UINT32 m3_ppc_read_32(UINT32 a);
extern UINT64 m3_ppc_read_64(UINT32 a);
extern void m3_ppc_write_8(UINT32 a, UINT8 d);
extern void m3_ppc_write_16(UINT32 a, UINT16 d);
extern void m3_ppc_write_32(UINT32 a, UINT32 d);
extern void m3_ppc_write_64(UINT32 a, UINT64 d);

extern void model3_dma_transfer(UINT32 src, UINT32 dst, int length, BOOL swap_words);

/******************************************************************/

#endif  // INCLUDED_MODEL3_H
