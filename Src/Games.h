/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011 Bart Trzynadlowski 
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
 * Games.h
 * 
 * Header file containing Model 3 game and ROM file information.
 */
 
#ifndef INCLUDED_GAMES_H
#define INCLUDED_GAMES_H


#include "ROMLoad.h"	// ROMInfo structure


/******************************************************************************
 Definitions
******************************************************************************/

// Input flags
#define GAME_INPUT_VEHICLE			0x001	// game has vehicle controls
#define GAME_INPUT_JOYSTICK1		0x002	// game has joystick 1 
#define GAME_INPUT_JOYSTICK2		0x004	// game has joystick 2
#define GAME_INPUT_FIGHTING			0x008	// game has fighting game controls
#define GAME_INPUT_VR				0x010	// game has VR view buttons
#define GAME_INPUT_RALLY			0x020	// game has rally car controls
#define GAME_INPUT_GUN1				0x040	// game has gun 1
#define GAME_INPUT_SHIFT4			0x080	// game has 4-speed shifter
#define GAME_INPUT_ANALOG_JOYSTICK	0x100	// game has analog joystick
#define GAME_INPUT_TWIN_JOYSTICKS	0x200	// game has twin joysticks
#define GAME_INPUT_SOCCER			0x400	// game has soccer controls


/******************************************************************************
 Data Structures
******************************************************************************/

/*
 * GameInfo:
 *
 * Describes a Model 3 game. List is terminated when title == NULL.
 */
struct GameInfo
{
	// Game information
	const char		id[9];			// 8-character game identifier (also serves as zip archive file name)
	const char		*title;			// complete game title
	const char		*mfgName;		// name of manufacturer
	unsigned		year;			// year released (in decimal)
	int				step;			// Model 3 hardware stepping: 0x10 = 1.0, 0x15 = 1.5, 0x20 = 2.0, 0x21 = 2.1
	unsigned		cromSize;		// size of fixed CROM (up to 8 MB)
	BOOL			mirrorLow64MB;	// mirror low 64 MB of banked CROM space to upper 64 MB
	unsigned		vromSize;		// size of video ROMs (32 or 64 MB; if the latter, will have to be mirrored)
	unsigned		inputFlags;		// game input types
	
	// ROM files
	struct ROMInfo	ROM[37];
};


/******************************************************************************
 Model 3 Game List
 
 All games supported by Supermodel. All ROMs are loaded according to their 
 native endianness. That is, the PowerPC ROMs are loaded just as a real 
 PowerPC would see them. The emulator may reorder the bytes on its own for 
 performance reasons (but the ROMs are not specified that way here).
******************************************************************************/

extern const struct GameInfo Model3GameList[];


#endif	// INCLUDED_GAMES_H
