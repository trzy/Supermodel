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
 * Games.cpp
 * 
 * Model 3 game and ROM information.
 */

#include "Supermodel.h"


const struct GameInfo	Model3GameList[] =
{
	// Virtua Fighter 3
	{
		"vf3",
		"Virtua Fighter 3",
		"Sega",
		1996,
		0x10,
		0x200000,	// 2 MB of fixed CROM
		TRUE,		// 64 MB of banked CROM (needs to be mirrored)
		0x2000000,	// 32 MB of VROM (will need to be mirrored)
		GAME_INPUT_COMMON|GAME_INPUT_JOYSTICK1|GAME_INPUT_JOYSTICK2|GAME_INPUT_FIGHTING,
		
		{
			// Fixed CROM
			{ "CROM",	"epr-19230c.20", 0x736a9431, 0x80000,	2, 0x0600000,	8,	TRUE },
			{ "CROM", 	"epr-19229c.19", 0x731b6b78, 0x80000,	2, 0x0600002,	8, 	TRUE },
			{ "CROM", 	"epr-19228c.18", 0x9c5727e2, 0x80000,	2, 0x0600004,	8, 	TRUE },
			{ "CROM", 	"epr-19227c.17", 0xa7df4d75, 0x80000,	2, 0x0600006,	8, 	TRUE },
			
			// Banked CROM0
			{ "CROMxx",	"mpr-19196.4", 0xf386b850, 0x400000,	2, 0x0000000,	8,	TRUE },
			{ "CROMxx",	"mpr-19195.3", 0xbd5e27a3, 0x400000,	2, 0x0000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-19194.2", 0x66254702, 0x400000,	2, 0x0000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-19193.1", 0x7bab33d2, 0x400000,	2, 0x0000006,	8, 	TRUE },
			
			// Banked CROM1
			{ "CROMxx",	"mpr-19200.8", 0x74941091, 0x400000,	2, 0x1000000,	8,	TRUE },
			{ "CROMxx",	"mpr-19199.7", 0x9f80d6fe, 0x400000,	2, 0x1000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-19198.6", 0xd8ee5032, 0x400000,	2, 0x1000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-19197.5", 0xa22d76c9, 0x400000,	2, 0x1000006,	8, 	TRUE },
			
			// Banked CROM2
			{ "CROMxx",	"mpr-19204.12", 0x2f93310a, 0x400000,	2, 0x2000000,	8,	TRUE },
			{ "CROMxx",	"mpr-19203.11", 0x0afa6334, 0x400000,	2, 0x2000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-19202.10", 0xaaa086c6, 0x400000,	2, 0x2000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-19201.9",  0x7c4a8c31, 0x400000,	2, 0x2000006,	8, 	TRUE },
			
			// Banked CROM3
			{ "CROMxx",	"mpr-19208.16", 0x08f30f71, 0x400000,	2, 0x3000000,	8,	TRUE },
			{ "CROMxx",	"mpr-19207.15", 0x2ce1612d, 0x400000,	2, 0x3000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-19206.14", 0x71a98d73, 0x400000,	2, 0x3000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-19205.13", 0x199c328e, 0x400000,	2, 0x3000006,	8, 	TRUE },
			
			// Video ROM
			{ "VROM",	"mpr-19211.26", 0x9c8f5df1, 0x200000, 	2, 0, 			32,	FALSE },
			{ "VROM",	"mpr-19212.27", 0x75036234, 0x200000, 	2, 2, 			32,	FALSE },
			{ "VROM",	"mpr-19213.28", 0x67b123cf, 0x200000, 	2, 4, 			32,	FALSE },
			{ "VROM",	"mpr-19214.29", 0xa6f5576b, 0x200000, 	2, 6, 			32,	FALSE },
			{ "VROM",	"mpr-19215.30", 0xc6fd9f0d, 0x200000, 	2, 8, 			32,	FALSE },
			{ "VROM",	"mpr-19216.31", 0x201bb1ed, 0x200000, 	2, 10, 			32,	FALSE },
			{ "VROM",	"mpr-19217.32", 0x4dadd41a, 0x200000, 	2, 12, 			32,	FALSE },
			{ "VROM",	"mpr-19218.33", 0xcff91953, 0x200000, 	2, 14, 			32,	FALSE },
			{ "VROM",	"mpr-19219.34", 0xc610d521, 0x200000, 	2, 16, 			32,	FALSE },
			{ "VROM",	"mpr-19220.35", 0xe62924d0, 0x200000, 	2, 18, 			32,	FALSE },
			{ "VROM",	"mpr-19221.36", 0x24f83e3c, 0x200000, 	2, 20, 			32,	FALSE },
			{ "VROM",	"mpr-19222.37", 0x61a6aa7d, 0x200000, 	2, 22, 			32,	FALSE },
			{ "VROM",	"mpr-19223.38", 0x1a8c1980, 0x200000, 	2, 24, 			32,	FALSE },
			{ "VROM",	"mpr-19224.39", 0x0a79a1bd, 0x200000, 	2, 26, 			32,	FALSE },
			{ "VROM",	"mpr-19225.40", 0x91a985eb, 0x200000, 	2, 28, 			32,	FALSE },
			{ "VROM",	"mpr-19226.41", 0x00091722, 0x200000, 	2, 30, 			32,	FALSE },
			
			{ NULL,	NULL, 0, 0, 0, 0, 0, FALSE }
		}
	},

	// Le Mans 24
	{
		"lemans24",
		"Le Mans 24",
		"Sega",
		1998,
		0x15,
		0x200000,	// 2 MB of fixed CROM
		TRUE,		// 64 MB of banked CROM (needs to be mirrored)
		0x2000000,	// 32 MB of VROM (will need to be mirrored)
		GAME_INPUT_COMMON|GAME_INPUT_VEHICLE|GAME_INPUT_VR|GAME_INPUT_SHIFT4,	// for now, Shift Up/Down mapped to Shift 3/4
		
		{
			// Fixed CROM
			{ "CROM",	"epr-19890.20", 0x9C16C3CC, 0x80000,	2, 0x0600000,	8,	TRUE },
			{ "CROM", 	"epr-19889.19", 0xD1F7E44C, 0x80000,	2, 0x0600002,	8, 	TRUE },
			{ "CROM", 	"epr-19888.18", 0x800D763D, 0x80000,	2, 0x0600004,	8, 	TRUE },
			{ "CROM", 	"epr-19887.17", 0x2842BB87, 0x80000,	2, 0x0600006,	8, 	TRUE },
			
			// Banked CROM0
			{ "CROMxx",	"mpr-19860.04", 0x19A1DDC7, 0x400000,	2, 0x0000000,	8,	TRUE },
			{ "CROMxx",	"mpr-19859.03", 0x15906869, 0x400000,	2, 0x0000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-19858.02", 0x993FA656, 0x400000,	2, 0x0000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-19857.01", 0x82C9FCFC, 0x400000,	2, 0x0000006,	8, 	TRUE },
			
			// Banked CROM1
			{ "CROMxx",	"mpr-19864.08", 0xC7BAAB2B, 0x400000,	2, 0x1000000,	8,	TRUE },
			{ "CROMxx",	"mpr-19863.07", 0x2B2619D0, 0x400000,	2, 0x1000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-19862.06", 0xB0F69AE4, 0x400000,	2, 0x1000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-19861.05", 0x6DDF21B3, 0x400000,	2, 0x1000006,	8, 	TRUE },
			
			// Banked CROM2
			{ "CROMxx",	"mpr-19868.12", 0x3C43D64F, 0x400000,	2, 0x2000000,	8,	TRUE },
			{ "CROMxx",	"mpr-19867.11", 0xAE610FC5, 0x400000,	2, 0x2000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-19866.10", 0xEDE5FC78, 0x400000,	2, 0x2000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-19865.09", 0xB2749D2B, 0x400000,	2, 0x2000006,	8, 	TRUE },
			
			// Video ROM
			{ "VROM",	"mpr-19871.26", 0x5168E02B, 0x200000, 	2, 0, 			32,	FALSE },
			{ "VROM",	"mpr-19872.27", 0x9E65FC06, 0x200000, 	2, 2, 			32,	FALSE },
			{ "VROM",	"mpr-19873.28", 0x0B15D7AB, 0x200000, 	2, 4, 			32,	FALSE },
			{ "VROM",	"mpr-19874.29", 0x6A28EC89, 0x200000, 	2, 6, 			32,	FALSE },
			{ "VROM",	"mpr-19875.30", 0xA03E1173, 0x200000, 	2, 8, 			32,	FALSE },
			{ "VROM",	"mpr-19876.31", 0xC93BB036, 0x200000, 	2, 10, 			32,	FALSE },
			{ "VROM",	"mpr-19877.32", 0xB1E3DF56, 0x200000, 	2, 12, 			32,	FALSE },
			{ "VROM",	"mpr-19878.33", 0xA2ACC111, 0x200000, 	2, 14, 			32,	FALSE },
			{ "VROM",	"mpr-19879.34", 0x90C1553F, 0x200000, 	2, 16, 			32,	FALSE },
			{ "VROM",	"mpr-19880.35", 0x42504e63, 0x200000, 	2, 18, 			32,	FALSE },
			{ "VROM",	"mpr-19881.36", 0xD06985CF, 0x200000, 	2, 20, 			32,	FALSE },
			{ "VROM",	"mpr-19882.37", 0xA86F2E2F, 0x200000, 	2, 22, 			32,	FALSE },
			{ "VROM",	"mpr-19883.38", 0x12895D6E, 0x200000, 	2, 24, 			32,	FALSE },
			{ "VROM",	"mpr-19884.39", 0x711EEBFB, 0x200000, 	2, 26, 			32,	FALSE },
			{ "VROM",	"mpr-19885.40", 0xD1AE5473, 0x200000, 	2, 28, 			32,	FALSE },
			{ "VROM",	"mpr-19886.41", 0x278AAE0B, 0x200000, 	2, 30, 			32,	FALSE },
			
			{ NULL,	NULL, 0, 0, 0, 0, 0, FALSE }
		}
	},
	
	// Scud Race
	{
		"scud",
		"Scud Race",
		"Sega",
		1996,
		0x15,
		0x200000,	// 2 MB of fixed CROM
		TRUE,		// 64 MB of banked CROM (needs to be mirrored)
		0x2000000,	// 32 MB of VROM (will need to be mirrored)
		GAME_INPUT_COMMON|GAME_INPUT_VEHICLE|GAME_INPUT_VR|GAME_INPUT_SHIFT4,
		
		{
			// Fixed CROM (mirroring behavior here is special and handled manually by CModel3)
			{ "CROM",	"epr-19734.20", 0xBE897336, 0x80000,	2, 0x0600000,	8,	TRUE },
			{ "CROM", 	"epr-19733.19", 0x6565E29A, 0x80000,	2, 0x0600002,	8, 	TRUE },
			{ "CROM", 	"epr-19732.18", 0x23E864BB, 0x80000,	2, 0x0600004,	8, 	TRUE },
			{ "CROM", 	"epr-19731.17", 0x3EE6447E, 0x80000,	2, 0x0600006,	8, 	TRUE },
			
			// Banked CROM0
			{ "CROMxx",	"mpr-19661.04", 0x8E3FD241, 0x400000,	2, 0x0000000,	8,	TRUE },
			{ "CROMxx",	"mpr-19660.03", 0xD999C935, 0x400000,	2, 0x0000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-19659.02", 0xC47E7002, 0x400000,	2, 0x0000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-19658.01", 0xD523235C, 0x400000,	2, 0x0000006,	8, 	TRUE },
			
			// Banked CROM1
			{ "CROMxx",	"mpr-19665.08", 0xF97C78F9, 0x400000,	2, 0x1000000,	8,	TRUE },
			{ "CROMxx",	"mpr-19664.07", 0xB9D11294, 0x400000,	2, 0x1000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-19663.06", 0xF6AF1CA4, 0x400000,	2, 0x1000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-19662.05", 0x3C700EFF, 0x400000,	2, 0x1000006,	8, 	TRUE },
			
			// Banked CROM2
			{ "CROMxx",	"mpr-19669.12", 0xCDC43C61, 0x400000,	2, 0x2000000,	8,	TRUE },
			{ "CROMxx",	"mpr-19668.11", 0x0B4DD8D5, 0x400000,	2, 0x2000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-19667.10", 0xA8676799, 0x400000,	2, 0x2000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-19666.09", 0xB53DC97F, 0x400000,	2, 0x2000006,	8, 	TRUE },
			
			// Video ROM
			{ "VROM",	"mpr-19672.26", 0x588C29FD, 0x200000, 	2, 0, 			32,	FALSE },
			{ "VROM",	"mpr-19673.27", 0x156ABAA9, 0x200000, 	2, 2, 			32,	FALSE },
			{ "VROM",	"mpr-19674.28", 0xC7B0F98C, 0x200000, 	2, 4, 			32,	FALSE },
			{ "VROM",	"mpr-19675.29", 0xFF113396, 0x200000, 	2, 6, 			32,	FALSE },
			{ "VROM",	"mpr-19676.30", 0xFD852EAD, 0x200000, 	2, 8, 			32,	FALSE },
			{ "VROM",	"mpr-19677.31", 0xC6AC0347, 0x200000, 	2, 10, 			32,	FALSE },
			{ "VROM",	"mpr-19678.32", 0xB8819CFE, 0x200000, 	2, 12, 			32,	FALSE },
			{ "VROM",	"mpr-19679.33", 0xE126C3E3, 0x200000, 	2, 14, 			32,	FALSE },
			{ "VROM",	"mpr-19680.34", 0x00EA5CEF, 0x200000, 	2, 16, 			32,	FALSE },
			{ "VROM",	"mpr-19681.35", 0xC949325F, 0x200000, 	2, 18, 			32,	FALSE },
			{ "VROM",	"mpr-19682.36", 0xCE5CA065, 0x200000, 	2, 20, 			32,	FALSE },
			{ "VROM",	"mpr-19683.37", 0xE5856419, 0x200000, 	2, 22, 			32,	FALSE },
			{ "VROM",	"mpr-19684.38", 0x56F6EC97, 0x200000, 	2, 24, 			32,	FALSE },
			{ "VROM",	"mpr-19685.39", 0x42B49304, 0x200000, 	2, 26, 			32,	FALSE },
			{ "VROM",	"mpr-19686.40", 0x84EED592, 0x200000, 	2, 28, 			32,	FALSE },
			{ "VROM",	"mpr-19687.41", 0x776CE694, 0x200000, 	2, 30, 			32,	FALSE },
			
			// Sound ROMs
			{ "SndProg","epr-19692.21",	0xA94F5521,	0x80000,	2, 0,			2,	TRUE },
			{ "Samples","mpr-19670.22", 0xBD31CC06, 0x400000,	2, 0x000000,	2,	FALSE },
			{ "Samples","mpr-19671.24", 0x8E8526AB, 0x400000,	2, 0x400000,	2,	FALSE },
			
			{ NULL,	NULL, 0, 0, 0, 0, 0, FALSE }
		}
	},
	
	// Scud Race Plus
	{
		"scudp",
		"Scud Race Plus",
		"Sega",
		1996,
		0x15,
		0x200000,	// 2 MB of fixed CROM
		TRUE,		// 64 MB of banked CROM (needs to be mirrored)
		0x2000000,	// 32 MB of VROM (will need to be mirrored)
		GAME_INPUT_COMMON|GAME_INPUT_VEHICLE|GAME_INPUT_VR|GAME_INPUT_SHIFT4,
		
		{
			// Fixed CROM (mirroring behavior here is special and handled manually by CModel3)
			{ "CROM",	"epr-20095a.20", 0x58C7E393, 0x80000,	2, 0x0600000,	8,	TRUE },
			{ "CROM", 	"epr-20094a.19", 0xDBF17A43, 0x80000,	2, 0x0600002,	8, 	TRUE },
			{ "CROM", 	"epr-20093a.18", 0x4ED2E35D, 0x80000,	2, 0x0600004,	8, 	TRUE },
			{ "CROM", 	"epr-20092a.17", 0xA94EC57E, 0x80000,	2, 0x0600006,	8, 	TRUE },
			
			// Banked CROM0
			{ "CROMxx",	"mpr-19661.04", 0x8E3FD241, 0x400000,	2, 0x0000000,	8,	TRUE },
			{ "CROMxx",	"mpr-19660.03", 0xD999C935, 0x400000,	2, 0x0000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-19659.02", 0xC47E7002, 0x400000,	2, 0x0000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-19658.01", 0xD523235C, 0x400000,	2, 0x0000006,	8, 	TRUE },
			
			// Banked CROM1
			{ "CROMxx",	"mpr-19665.08", 0xF97C78F9, 0x400000,	2, 0x1000000,	8,	TRUE },
			{ "CROMxx",	"mpr-19664.07", 0xB9D11294, 0x400000,	2, 0x1000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-19663.06", 0xF6AF1CA4, 0x400000,	2, 0x1000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-19662.05", 0x3C700EFF, 0x400000,	2, 0x1000006,	8, 	TRUE },
			
			// Banked CROM2
			{ "CROMxx",	"mpr-19669.12", 0xCDC43C61, 0x400000,	2, 0x2000000,	8,	TRUE },
			{ "CROMxx",	"mpr-19668.11", 0x0B4DD8D5, 0x400000,	2, 0x2000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-19667.10", 0xA8676799, 0x400000,	2, 0x2000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-19666.09", 0xB53DC97F, 0x400000,	2, 0x2000006,	8, 	TRUE },
			
			// Banked CROM3
			{ "CROMxx",	"mpr-20100.16", 0xC99E2C01, 0x400000,	2, 0x3000000,	8,	TRUE },
			{ "CROMxx",	"mpr-20099.15", 0xFC9BD7D9, 0x400000,	2, 0x3000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-20098.14", 0x8355FA41, 0x400000,	2, 0x3000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-20097.13", 0x269A9DBE, 0x400000,	2, 0x3000006,	8, 	TRUE },
			
			// Video ROM
			{ "VROM",	"mpr-19672.26", 0x588C29FD, 0x200000, 	2, 0, 			32,	FALSE },
			{ "VROM",	"mpr-19673.27", 0x156ABAA9, 0x200000, 	2, 2, 			32,	FALSE },
			{ "VROM",	"mpr-19674.28", 0xC7B0F98C, 0x200000, 	2, 4, 			32,	FALSE },
			{ "VROM",	"mpr-19675.29", 0xFF113396, 0x200000, 	2, 6, 			32,	FALSE },
			{ "VROM",	"mpr-19676.30", 0xFD852EAD, 0x200000, 	2, 8, 			32,	FALSE },
			{ "VROM",	"mpr-19677.31", 0xC6AC0347, 0x200000, 	2, 10, 			32,	FALSE },
			{ "VROM",	"mpr-19678.32", 0xB8819CFE, 0x200000, 	2, 12, 			32,	FALSE },
			{ "VROM",	"mpr-19679.33", 0xE126C3E3, 0x200000, 	2, 14, 			32,	FALSE },
			{ "VROM",	"mpr-19680.34", 0x00EA5CEF, 0x200000, 	2, 16, 			32,	FALSE },
			{ "VROM",	"mpr-19681.35", 0xC949325F, 0x200000, 	2, 18, 			32,	FALSE },
			{ "VROM",	"mpr-19682.36", 0xCE5CA065, 0x200000, 	2, 20, 			32,	FALSE },
			{ "VROM",	"mpr-19683.37", 0xE5856419, 0x200000, 	2, 22, 			32,	FALSE },
			{ "VROM",	"mpr-19684.38", 0x56F6EC97, 0x200000, 	2, 24, 			32,	FALSE },
			{ "VROM",	"mpr-19685.39", 0x42B49304, 0x200000, 	2, 26, 			32,	FALSE },
			{ "VROM",	"mpr-19686.40", 0x84EED592, 0x200000, 	2, 28, 			32,	FALSE },
			{ "VROM",	"mpr-19687.41", 0x776CE694, 0x200000, 	2, 30, 			32,	FALSE },
			
			{ NULL,	NULL, 0, 0, 0, 0, 0, FALSE }
		}
	},
	
	// The Lost World
	{
		"lostwsga",
		"The Lost World",
		"Sega",
		1997,
		0x15,
		0x200000,	// 2 MB of fixed CROM
		TRUE,		// 64 MB of banked CROM (needs to be mirrored)
		0x2000000,	// 32 MB of VROM
		GAME_INPUT_COMMON|GAME_INPUT_GUN1|GAME_INPUT_GUN2,
		
		{
			// Fixed CROM
			{ "CROM", 	"epr-19936.20", 0x2F1CA664, 0x80000,	2, 0x0600000,	8, TRUE },
			{ "CROM", 	"epr-19937.19", 0x9DBF5712, 0x80000,	2, 0x0600002,	8, TRUE },
			{ "CROM",	"epr-19938.18", 0x38AFE27A, 0x80000,	2, 0x0600004,	8, TRUE },
			{ "CROM",	"epr-19939.17", 0x8788B939, 0x80000,	2, 0x0600006,	8, TRUE },
			
			// Banked CROM0
			{ "CROMxx",	"mpr-19921.4",	0x9AF3227F, 0x400000,	2, 0x0000000,	8,	TRUE },
			{ "CROMxx",	"mpr-19920.3", 	0x8DF33574, 0x400000,	2, 0x0000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-19919.2", 	0xFF119949, 0x400000,	2, 0x0000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-19918.1", 	0x95B690E9, 0x400000,	2, 0x0000006,	8, 	TRUE },
			
			// Banked CROM1
			{ "CROMxx",	"mpr-19925.8", 	0xCFA4BB49, 0x400000,	2, 0x1000000,	8,	TRUE },
			{ "CROMxx",	"mpr-19924.7", 	0x4EE3DDC5, 0x400000,	2, 0x1000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-19923.6", 	0xED515CB2, 0x400000,	2, 0x1000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-19922.5", 	0x4DFD7FC6, 0x400000,	2, 0x1000006,	8, 	TRUE },
			
			// Banked CROM2
			{ "CROMxx",	"mpr-19929.12", 0x16491F63, 0x400000,	2, 0x2000000,	8,	TRUE },
			{ "CROMxx",	"mpr-19928.11", 0x9AFD5D4A, 0x400000,	2, 0x2000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-19927.10", 0x0C96EF11, 0x400000,	2, 0x2000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-19926.9", 	0x05A232E0, 0x400000,	2, 0x2000006,	8, 	TRUE },
			
			// Banked CROM3
			{ "CROMxx",	"mpr-19933.16", 0x8E2ACD3B, 0x400000,	2, 0x3000000,	8,	TRUE },
			{ "CROMxx",	"mpr-19932.15", 0x04389385, 0x400000,	2, 0x3000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-19931.14", 0x448A5007, 0x400000,	2, 0x3000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-19930.13", 0xB598C2F2, 0x400000,	2, 0x3000006,	8, 	TRUE },
			
			// Video ROM
			{ "VROM",	"mpr-19902.26", 0x178BD471, 0x200000, 	2, 0, 			32,	FALSE },
			{ "VROM",	"mpr-19903.27", 0xFE575871, 0x200000, 	2, 2, 			32,	FALSE },
			{ "VROM",	"mpr-19904.28", 0x57971D7D, 0x200000, 	2, 4, 			32,	FALSE },
			{ "VROM",	"mpr-19905.29", 0x6FA122EE, 0x200000, 	2, 6, 			32,	FALSE },
			{ "VROM",	"mpr-19906.30", 0xA5B16DD9, 0x200000, 	2, 8, 			32,	FALSE },
			{ "VROM",	"mpr-19907.31", 0x84A425CD, 0x200000, 	2, 10, 			32,	FALSE },
			{ "VROM",	"mpr-19908.32", 0x7702AA7C, 0x200000, 	2, 12, 			32,	FALSE },
			{ "VROM",	"mpr-19909.33", 0x8FCA65F9, 0x200000, 	2, 14, 			32,	FALSE },
			{ "VROM",	"mpr-19910.34", 0x1EF585E2, 0x200000, 	2, 16, 			32,	FALSE },
			{ "VROM",	"mpr-19911.35", 0xCA26A48D, 0x200000, 	2, 18, 			32,	FALSE },
			{ "VROM",	"mpr-19912.36", 0xFFE000E0, 0x200000, 	2, 20, 			32,	FALSE },
			{ "VROM",	"mpr-19913.37", 0xC003049E, 0x200000, 	2, 22, 			32,	FALSE },
			{ "VROM",	"mpr-19914.38", 0x3C21A953, 0x200000, 	2, 24, 			32,	FALSE },
			{ "VROM",	"mpr-19915.39", 0xFD0F2A2B, 0x200000, 	2, 26, 			32,	FALSE },
			{ "VROM",	"mpr-19916.40", 0x10B0C52E, 0x200000, 	2, 28, 			32,	FALSE },
			{ "VROM",	"mpr-19917.41", 0x3035833B, 0x200000, 	2, 30, 			32,	FALSE },
			
			{ NULL,	NULL, 0, 0, 0, 0, 0, FALSE }
		}
	},
	
	// Virtual On Oratorio Tangram
	{
		"von2",
		"Virtual On Oratorio Tangram",
		"Sega",
		1998,
		0x20,
		0x800000,	// 8 MB of fixed CROM
		TRUE,		// 64 MB of banked CROM (needs to be mirrored)
		0x4000000,	// 64 MB of VROM
		GAME_INPUT_COMMON|GAME_INPUT_TWIN_JOYSTICKS,
		
		{
			// Fixed CROM
			{ "CROM", 	"epr-20686b.20", 0x3EA4DE9F, 0x200000,	2, 0x0000000,	8, TRUE },
			{ "CROM", 	"epr-20685b.19", 0xAE82CB35, 0x200000,	2, 0x0000002,	8, TRUE },
			{ "CROM",	"epr-20684b.18", 0x1FC15431, 0x200000,	2, 0x0000004,	8, TRUE },
			{ "CROM",	"epr-20683b.17", 0x59D9C974, 0x200000,	2, 0x0000006,	8, TRUE },
			
			// Banked CROM0
			{ "CROMxx",	"mpr-20650.4",	0x81F96649, 0x400000,	2, 0x0000000,	8,	TRUE },
			{ "CROMxx",	"mpr-20649.3", 	0xb8FD56BA, 0x400000,	2, 0x0000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-20648.2", 	0x107309E0, 0x400000,	2, 0x0000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-20647.1", 	0xe8586380, 0x400000,	2, 0x0000006,	8, 	TRUE },
			
			// Banked CROM1
			{ "CROMxx",	"mpr-20654.8", 	0x763EF905, 0x400000,	2, 0x1000000,	8,	TRUE },
			{ "CROMxx",	"mpr-20653.7", 	0x858E6BBA, 0x400000,	2, 0x1000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-20652.6", 	0x64C6FBB6, 0x400000,	2, 0x1000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-20651.5", 	0x8373CAB3, 0x400000,	2, 0x1000006,	8, 	TRUE },
			
			// Banked CROM2
			{ "CROMxx",	"mpr-20658.12", 0xB80175B9, 0x400000,	2, 0x2000000,	8,	TRUE },
			{ "CROMxx",	"mpr-20657.11", 0x14BF8964, 0x400000,	2, 0x2000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-20656.10", 0x466BEE13, 0x400000,	2, 0x2000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-20655.9", 	0xF0A471E9, 0x400000,	2, 0x2000006,	8, 	TRUE },
			
			// Banked CROM3
			{ "CROMxx",	"mpr-20662.16", 0x7130CB61, 0x400000,	2, 0x3000000,	8,	TRUE },
			{ "CROMxx",	"mpr-20661.15", 0x50E6189E, 0x400000,	2, 0x3000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-20660.14", 0xD961D385, 0x400000,	2, 0x3000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-20659.13", 0xEDB63E7B, 0x400000,	2, 0x3000006,	8, 	TRUE },
			
			// Video ROM
			{ "VROM",	"mpr-20667.26", 0x321E006F, 0x400000, 	2, 0, 			32,	FALSE },
			{ "VROM",	"mpr-20668.27", 0xC2DD8053, 0x400000, 	2, 2, 			32,	FALSE },
			{ "VROM",	"mpr-20669.28", 0x63432497, 0x400000, 	2, 4, 			32,	FALSE },
			{ "VROM",	"mpr-20670.29", 0xF7B554FD, 0x400000, 	2, 6, 			32,	FALSE },
			{ "VROM",	"mpr-20671.30", 0xFEE1A49B, 0x400000, 	2, 8, 			32,	FALSE },
			{ "VROM",	"mpr-20672.31", 0xE4B8C6E6, 0x400000, 	2, 10, 			32,	FALSE },
			{ "VROM",	"mpr-20673.32", 0xE7B6403B, 0x400000, 	2, 12, 			32,	FALSE },
			{ "VROM",	"mpr-20674.33", 0x9BE22E13, 0x400000, 	2, 14, 			32,	FALSE },
			{ "VROM",	"mpr-20675.34", 0x6A7C3862, 0x400000, 	2, 16, 			32,	FALSE },
			{ "VROM",	"mpr-20676.35", 0xDD299648, 0x400000, 	2, 18, 			32,	FALSE },
			{ "VROM",	"mpr-20677.36", 0x3FC5F330, 0x400000, 	2, 20, 			32,	FALSE },
			{ "VROM",	"mpr-20678.37", 0x62F794A1, 0x400000, 	2, 22, 			32,	FALSE },
			{ "VROM",	"mpr-20679.38", 0x35A37C53, 0x400000, 	2, 24, 			32,	FALSE },
			{ "VROM",	"mpr-20680.39", 0x81FEC46E, 0x400000, 	2, 26, 			32,	FALSE },
			{ "VROM",	"mpr-20681.40", 0xD517873B, 0x400000, 	2, 28, 			32,	FALSE },
			{ "VROM",	"mpr-20682.41", 0x5B43250C, 0x400000, 	2, 30, 			32,	FALSE },
			
			{ NULL,	NULL, 0, 0, 0, 0, 0, FALSE }
		}
	},
	
	// Virtua Striker 2 '98
	{
		"vs298",
		"Virtua Striker 2 '98",
		"Sega",
		1998,
		0x20,
		0x400000,	// 2 MB of fixed CROM
		TRUE,		// 64 MB of banked CROM (needs to be mirrored)
		0x2000000,	// 32 MB of VROM
		GAME_INPUT_COMMON|GAME_INPUT_JOYSTICK1|GAME_INPUT_JOYSTICK2|GAME_INPUT_SOCCER,
		
		{
			// Fixed CROM
			{ "CROM", 	"epr-20920.20", 0x428D05FC, 0x100000,	2, 0x0400000,	8, TRUE },
			{ "CROM", 	"epr-20919.19", 0x7A0713D2, 0x100000,	2, 0x0400002,	8, TRUE },
			{ "CROM",	"epr-20918.18", 0x0E9CDC5B, 0x100000,	2, 0x0400004,	8, TRUE },
			{ "CROM",	"epr-20917.17", 0xC3BBB270, 0x100000,	2, 0x0400006,	8, TRUE },
			
			// Banked CROM0
			{ "CROMxx",	"mpr-19894.4",	0x09C065CC, 0x400000,	2, 0x0000000,	8,	TRUE },
			{ "CROMxx",	"mpr-19893.3", 	0x5C83DCAA, 0x400000,	2, 0x0000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-19892.2", 	0x8E5D3FE7, 0x400000,	2, 0x0000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-19891.1", 	0x9ECB0B39, 0x400000,	2, 0x0000006,	8, 	TRUE },
			
			// Banked CROM1
			{ "CROMxx",	"mpr-19776.8", 	0x5B31C7C1, 0x400000,	2, 0x1000000,	8,	TRUE },
			{ "CROMxx",	"mpr-19775.7", 	0xA6B32BD9, 0x400000,	2, 0x1000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-19774.6", 	0x1D61D287, 0x400000,	2, 0x1000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-19773.5", 	0x4E381AE7, 0x400000,	2, 0x1000006,	8, 	TRUE },
			
			// Banked CROM2
			{ "CROMxx",	"mpr-20898.12", 0x94040D37, 0x400000,	2, 0x2000000,	8,	TRUE },
			{ "CROMxx",	"mpr-20897.11", 0xC5CF067A, 0x400000,	2, 0x2000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-20896.10", 0xBF1CBD5E, 0x400000,	2, 0x2000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-20895.9", 	0x9B51CBF5, 0x400000,	2, 0x2000006,	8, 	TRUE },
			
			// Banked CROM3
			{ "CROMxx",	"mpr-20902.16", 0xF4D3FF3A, 0x400000,	2, 0x3000000,	8,	TRUE },
			{ "CROMxx",	"mpr-20901.15", 0x3492DDC8, 0x400000,	2, 0x3000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-20900.14", 0x7A38B571, 0x400000,	2, 0x3000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-20899.13", 0x65422425, 0x400000,	2, 0x3000006,	8, 	TRUE },
			
			// Video ROM
			{ "VROM",	"mpr-19787.26", 0x856CC4AD, 0x200000, 	2, 0, 			32,	FALSE },
			{ "VROM",	"mpr-19788.27", 0x72EF970A, 0x200000, 	2, 2, 			32,	FALSE },
			{ "VROM",	"mpr-19789.28", 0x076ADD9A, 0x200000, 	2, 4, 			32,	FALSE },
			{ "VROM",	"mpr-19790.29", 0x74CE238C, 0x200000, 	2, 6, 			32,	FALSE },
			{ "VROM",	"mpr-19791.30", 0x75A98F96, 0x200000, 	2, 8, 			32,	FALSE },
			{ "VROM",	"mpr-19792.31", 0x85C81633, 0x200000, 	2, 10, 			32,	FALSE },
			{ "VROM",	"mpr-19793.32", 0x7F288CC4, 0x200000, 	2, 12, 			32,	FALSE },
			{ "VROM",	"mpr-19794.33", 0xE0C1C370, 0x200000, 	2, 14, 			32,	FALSE },
			{ "VROM",	"mpr-19795.34", 0x90989B20, 0x200000, 	2, 16, 			32,	FALSE },
			{ "VROM",	"mpr-19796.35", 0x5D1AAB8D, 0x200000, 	2, 18, 			32,	FALSE },
			{ "VROM",	"mpr-19797.36", 0xF5EDC891, 0x200000, 	2, 20, 			32,	FALSE },
			{ "VROM",	"mpr-19798.37", 0xAE2DA90F, 0x200000, 	2, 22, 			32,	FALSE },
			{ "VROM",	"mpr-19799.38", 0x92B18AD7, 0x200000, 	2, 24, 			32,	FALSE },
			{ "VROM",	"mpr-19800.39", 0x4A57B16C, 0x200000, 	2, 26, 			32,	FALSE },
			{ "VROM",	"mpr-19801.40", 0xBEB79A00, 0x200000, 	2, 28, 			32,	FALSE },
			{ "VROM",	"mpr-19802.41", 0xF2C3A7B7, 0x200000, 	2, 30, 			32,	FALSE },
			
			{ NULL,	NULL, 0, 0, 0, 0, 0, FALSE }
		}
	},
	
	// Sega Rally 2
	{
		"srally2",
		"Sega Rally 2",
		"Sega",
		1998,
		0x20,
		0x800000,	// 8 MB of fixed CROM
		TRUE,		// 64 MB of banked CROM (needs to be mirrored)
		0x4000000,	// 64 MB of VROM
		GAME_INPUT_COMMON|GAME_INPUT_VEHICLE|GAME_INPUT_RALLY|GAME_INPUT_SHIFT4,
		
		{
			// Fixed CROM
			{ "CROM", 	"epr-20635.20", 0x7937473F, 0x200000,	2, 0x0000000,	8, TRUE },
			{ "CROM", 	"epr-20634.19", 0x45A09245, 0x200000,	2, 0x0000002,	8, TRUE },
			{ "CROM",	"epr-20633.18", 0xF5A24F24, 0x200000,	2, 0x0000004,	8, TRUE },
			{ "CROM",	"epr-20632.17", 0x6829A801, 0x200000,	2, 0x0000006,	8, TRUE },
			
			// Banked CROM0
			{ "CROMxx",	"mpr-20605.4",	0x00513401, 0x400000,	2, 0x0000000,	8,	TRUE },
			{ "CROMxx",	"mpr-20605.3", 	0x99C5F396, 0x400000,	2, 0x0000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-20603.2", 	0xAD0D8EB8, 0x400000,	2, 0x0000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-20602.1", 	0x60CFA72A, 0x400000,	2, 0x0000006,	8, 	TRUE },
			
			// Banked CROM1
			{ "CROMxx",	"mpr-20609.8", 	0xC03CC0E5, 0x400000,	2, 0x1000000,	8,	TRUE },
			{ "CROMxx",	"mpr-20608.7", 	0x0C9B0571, 0x400000,	2, 0x1000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-20607.6", 	0x6DA85AA3, 0x400000,	2, 0x1000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-20606.5", 	0x072498FD, 0x400000,	2, 0x1000006,	8, 	TRUE },
			
			// Banked CROM2
			{ "CROMxx",	"mpr-20613.12", 0x2938C0D9, 0x400000,	2, 0x2000000,	8,	TRUE },
			{ "CROMxx",	"mpr-20612.11", 0x721A44B6, 0x400000,	2, 0x2000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-20611.10", 0x5D9F8BA2, 0x400000,	2, 0x2000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-20610.9", 	0xB6E0FF4E, 0x400000,	2, 0x2000006,	8, 	TRUE },
			
			// Video ROM
			{ "VROM",	"mpr-20616.26", 0xE11DCF8B, 0x400000, 	2, 0, 			32,	FALSE },
			{ "VROM",	"mpr-20617.27", 0x96ACEF3F, 0x400000, 	2, 2, 			32,	FALSE },
			{ "VROM",	"mpr-20618.28", 0x6C281281, 0x400000, 	2, 4, 			32,	FALSE },
			{ "VROM",	"mpr-20619.29", 0x0FA65819, 0x400000, 	2, 6, 			32,	FALSE },
			{ "VROM",	"mpr-20620.30", 0xEE79585F, 0x400000, 	2, 8, 			32,	FALSE },
			{ "VROM",	"mpr-20621.31", 0x3A99148F, 0x400000, 	2, 10, 			32,	FALSE },
			{ "VROM",	"mpr-20622.32", 0x0618F056, 0x400000, 	2, 12, 			32,	FALSE },
			{ "VROM",	"mpr-20623.33", 0xCCF31B85, 0x400000, 	2, 14, 			32,	FALSE },
			{ "VROM",	"mpr-20624.34", 0x90F30936, 0x400000, 	2, 16, 			32,	FALSE },
			{ "VROM",	"mpr-20625.35", 0x04F804FA, 0x400000, 	2, 18, 			32,	FALSE },
			{ "VROM",	"mpr-20626.36", 0x2D6C97D6, 0x400000, 	2, 20, 			32,	FALSE },
			{ "VROM",	"mpr-20627.37", 0xA14EE871, 0x400000, 	2, 22, 			32,	FALSE },
			{ "VROM",	"mpr-20628.38", 0xBBA829A3, 0x400000, 	2, 24, 			32,	FALSE },
			{ "VROM",	"mpr-20629.39", 0xEAD2EB31, 0x400000, 	2, 26, 			32,	FALSE },
			{ "VROM",	"mpr-20630.40", 0xCC5881B8, 0x400000, 	2, 28, 			32,	FALSE },
			{ "VROM",	"mpr-20631.41", 0x5CB69FFD, 0x400000, 	2, 30, 			32,	FALSE },
			
			{ NULL,	NULL, 0, 0, 0, 0, 0, FALSE }
		}
	},

	// Daytona USA 2
	{
		"daytona2",
		"Daytona USA 2",
		"Sega",
		1998,
		0x21,
		0x800000,	// 8 MB of fixed CROM
		FALSE,		// 96 MB of banked CROM (do not mirror)
		0x4000000,	// 64 MB of VROM
		GAME_INPUT_COMMON|GAME_INPUT_VEHICLE|GAME_INPUT_VR|GAME_INPUT_SHIFT4,
		
		{
			// Fixed CROM
			{ "CROM", 	"epr-20864a.20", 0x5250F3A8, 0x200000,	2, 0x0000000,	8, TRUE },
			{ "CROM", 	"epr-20863a.19", 0x1DEB4686, 0x200000,	2, 0x0000002,	8, TRUE },
			{ "CROM",	"epr-20862a.18", 0xE1B2CA61, 0x200000,	2, 0x0000004,	8, TRUE },
			{ "CROM",	"epr-20861a.17", 0x89BA8E78, 0x200000,	2, 0x0000006,	8, TRUE },
			
			// Banked CROM0
			{ "CROMxx",	"mpr-20848.ic4",	0x5B6C8B7D, 0x800000,	2, 0x0000000,	8,	TRUE },
			{ "CROMxx",	"mpr-20847.ic3", 	0xEDA966EE, 0x800000,	2, 0x0000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-20846.ic2", 	0xF44C5C7A, 0x800000,	2, 0x0000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-20845.ic1", 	0x6037712C, 0x800000,	2, 0x0000006,	8, 	TRUE },
			
			// Banked CROM1
			{ "CROMxx",	"mpr-20852.ic8", 	0xD606AD38, 0x800000,	2, 0x2000000,	8,	TRUE },
			{ "CROMxx",	"mpr-20851.ic7", 	0x6E7A64B7, 0x800000,	2, 0x2000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-20850.ic6", 	0xCB73758A, 0x800000,	2, 0x2000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-20849.ic5", 	0x50DEE4AF, 0x800000,	2, 0x2000006,	8, 	TRUE },

			// Banked CROM2
			{ "CROMxx",	"mpr-20856.12", 0x0367A242, 0x400000,	2, 0x4000000,	8,	TRUE },
			{ "CROMxx",	"mpr-20855.11", 0xF1FF0794, 0x400000,	2, 0x4000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-20854.10", 0x68D94CDF, 0x400000,	2, 0x4000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-20853.9", 	0x3245EE68, 0x400000,	2, 0x4000006,	8, 	TRUE },

			// Banked CROM3 (note: appears at offset 0x6000000 rather than 0x5000000 as expected)
			{ "CROMxx",	"mpr-20860.16", 0xE5CE2939, 0x400000,	2, 0x6000000,	8,	TRUE },
			{ "CROMxx",	"mpr-20859.15", 0xE14F5C46, 0x400000,	2, 0x6000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-20858.14", 0x407FBAD5, 0x400000,	2, 0x6000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-20857.13", 0x1EAB9C62, 0x400000,	2, 0x6000006,	8, 	TRUE },
			
			// Video ROM
			{ "VROM",	"mpr-20870.26", 0x7C9E573D, 0x400000, 	2, 0, 			32,	FALSE },
			{ "VROM",	"mpr-20871.27", 0x47A1B789, 0x400000, 	2, 2, 			32,	FALSE },
			{ "VROM",	"mpr-20872.28", 0x2F55B423, 0x400000, 	2, 4, 			32,	FALSE },
			{ "VROM",	"mpr-20873.29", 0xC9000E48, 0x400000, 	2, 6, 			32,	FALSE },
			{ "VROM",	"mpr-20874.30", 0x26A9CCA2, 0x400000, 	2, 8, 			32,	FALSE },
			{ "VROM",	"mpr-20875.31", 0xBFEFD21E, 0x400000, 	2, 10, 			32,	FALSE },
			{ "VROM",	"mpr-20876.32", 0xFA701B87, 0x400000, 	2, 12, 			32,	FALSE },
			{ "VROM",	"mpr-20877.33", 0x2CD072F1, 0x400000, 	2, 14, 			32,	FALSE },
			{ "VROM",	"mpr-20878.34", 0xE6D5BC01, 0x400000, 	2, 16, 			32,	FALSE },
			{ "VROM",	"mpr-20879.35", 0xF1D727EC, 0x400000, 	2, 18, 			32,	FALSE },
			{ "VROM",	"mpr-20880.36", 0x8B370602, 0x400000, 	2, 20, 			32,	FALSE },
			{ "VROM",	"mpr-20881.37", 0x397322E7, 0x400000, 	2, 22, 			32,	FALSE },
			{ "VROM",	"mpr-20882.38", 0x9185BE51, 0x400000, 	2, 24, 			32,	FALSE },
			{ "VROM",	"mpr-20883.39", 0xD1E39E83, 0x400000, 	2, 26, 			32,	FALSE },
			{ "VROM",	"mpr-20884.40", 0x63C4639A, 0x400000, 	2, 28, 			32,	FALSE },
			{ "VROM",	"mpr-20885.41", 0x61C292CA, 0x400000, 	2, 30, 			32,	FALSE },
			
			{ NULL,	NULL, 0, 0, 0, 0, 0, FALSE }
		}
	},

	// Daytona USA 2 Power Edition
	{
		"dayto2pe",
		"Daytona USA 2 Power Edition",
		"Sega",
		1998,
		0x21,
		0x800000,	// 8 MB of fixed CROM
		FALSE,		// do not mirror banked CROM
		0x4000000,	// 64 MB of VROM
		GAME_INPUT_COMMON|GAME_INPUT_VEHICLE|GAME_INPUT_VR|GAME_INPUT_SHIFT4,
		
		{
			// Fixed CROM
			{ "CROM", 	"epr-21181.20", 0xBF0007ED, 0x200000,	2, 0x0000000,	8, TRUE },
			{ "CROM", 	"epr-21180.19", 0x6E7B98ED, 0x200000,	2, 0x0000002,	8, TRUE },
			{ "CROM",	"epr-21179.18", 0xD5FFB4D6, 0x200000,	2, 0x0000004,	8, TRUE },
			{ "CROM",	"epr-21178.17", 0x230BF8AC, 0x200000,	2, 0x0000006,	8, TRUE },
			
			// Banked CROM0
			{ "CROMxx",	"mpr-21185.4",	0xB6D5D2A1, 0x400000,	2, 0x0000000,	8,	TRUE },
			{ "CROMxx",	"mpr-21184.3", 	0x25616403, 0x400000,	2, 0x0000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-21183.2", 	0xB4B44805, 0x400000,	2, 0x0000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-21182.1", 	0xBA8E667F, 0x400000,	2, 0x0000006,	8, 	TRUE },
			
			// Banked CROM1
			{ "CROMxx",	"mpr-21189.8", 	0xCB439C45, 0x400000,	2, 0x2000000,	8,	TRUE },
			{ "CROMxx",	"mpr-21188.7", 	0x753FC2A5, 0x400000,	2, 0x2000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-21187.6", 	0x3BD14EE6, 0x400000,	2, 0x2000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-21186.5", 	0xA6128662, 0x400000,	2, 0x2000006,	8, 	TRUE },

			// Banked CROM2
			{ "CROMxx",	"mpr-21193.12", 0x4638FEF4, 0x400000,	2, 0x4000000,	8,	TRUE },
			{ "CROMxx",	"mpr-21192.11", 0x60CBB1FA, 0x400000,	2, 0x4000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-21191.10", 0xA2BDCFE0, 0x400000,	2, 0x4000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-21190.9", 	0x984D56EB, 0x400000,	2, 0x4000006,	8, 	TRUE },

			// Banked CROM3
			{ "CROMxx",	"mpr-21197.16", 0x04015247, 0x400000,	2, 0x6000000,	8,	TRUE },
			{ "CROMxx",	"mpr-21196.15", 0x0AB46DB5, 0x400000,	2, 0x6000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-21195.14", 0x7F39761C, 0x400000,	2, 0x6000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-21194.13", 0x12C7A414, 0x400000,	2, 0x6000006,	8, 	TRUE },
			
			// Video ROM
			{ "VROM",	"mpr-21198.26", 0x42EC9ED4, 0x400000, 	2, 0, 			32,	FALSE },
			{ "VROM",	"mpr-21199.27", 0xFA28088C, 0x400000, 	2, 2, 			32,	FALSE },
			{ "VROM",	"mpr-21200.28", 0xFBB5AA1D, 0x400000, 	2, 4, 			32,	FALSE },
			{ "VROM",	"mpr-21201.29", 0xE6B13469, 0x400000, 	2, 6, 			32,	FALSE },
			{ "VROM",	"mpr-21202.30", 0xE6B4C2BE, 0x400000, 	2, 8, 			32,	FALSE },
			{ "VROM",	"mpr-21203.31", 0x32D08D33, 0x400000, 	2, 10, 			32,	FALSE },
			{ "VROM",	"mpr-21204.32", 0xEF18FE0A, 0x400000, 	2, 12, 			32,	FALSE },
			{ "VROM",	"mpr-21205.33", 0x4687BEA6, 0x400000, 	2, 14, 			32,	FALSE },
			{ "VROM",	"mpr-21206.34", 0xEC2D6884, 0x400000, 	2, 16, 			32,	FALSE },
			{ "VROM",	"mpr-21207.35", 0xEEAA510B, 0x400000, 	2, 18, 			32,	FALSE },
			{ "VROM",	"mpr-21208.36", 0xB222FEF0, 0x400000, 	2, 20, 			32,	FALSE },
			{ "VROM",	"mpr-21209.37", 0x170A28CE, 0x400000, 	2, 22, 			32,	FALSE },
			{ "VROM",	"mpr-21210.38", 0x460CEFE0, 0x400000, 	2, 24, 			32,	FALSE },
			{ "VROM",	"mpr-21211.39", 0xC84759CE, 0x400000, 	2, 26, 			32,	FALSE },
			{ "VROM",	"mpr-21212.40", 0x6F8A75E0, 0x400000, 	2, 28, 			32,	FALSE },
			{ "VROM",	"mpr-21213.41", 0xDE75BEC6, 0x400000, 	2, 30, 			32,	FALSE },
			
			{ NULL,	NULL, 0, 0, 0, 0, 0, FALSE }
		}
	},

	// Fighting Vipers 2
	{
		"fvipers2",
		"Fighting Vipers 2",
		"Sega",
		1998,
		0x20,
		0x800000,	// 8 MB of fixed CROM
		TRUE,
		//0x6000000,	// 64 MB of banked CROM
		0x4000000,	// 64 MB of VROM
		GAME_INPUT_COMMON|GAME_INPUT_JOYSTICK1|GAME_INPUT_JOYSTICK2|GAME_INPUT_FIGHTING,
		
		{
			// Fixed CROM
			{ "CROM", 	"epr-20599a.20", 0x9DF02AB9, 0x200000,	2, 0x0000000,	8, TRUE },
			{ "CROM", 	"epr-20598a.19", 0x87BD070F, 0x200000,	2, 0x0000002,	8, TRUE },
			{ "CROM",	"epr-20597a.18", 0x6FCEE322, 0x200000,	2, 0x0000004,	8, TRUE },
			{ "CROM",	"epr-20596a.17", 0x969AB801, 0x200000,	2, 0x0000006,	8, TRUE },
			
			// Banked CROM0
			{ "CROMxx",	"mpr-20563.4",	0x999848AC, 0x400000,	2, 0x0000000,	8,	TRUE },
			{ "CROMxx",	"mpr-20562.3", 	0x96E4942E, 0x400000,	2, 0x0000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-20561.2", 	0x38A0F112, 0x400000,	2, 0x0000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-20560.1", 	0xB0F6584D, 0x400000,	2, 0x0000006,	8, 	TRUE },
			
			// Banked CROM1
			{ "CROMxx",	"mpr-20567.8", 	0x80F4EBA7, 0x400000,	2, 0x1000000,	8,	TRUE },
			{ "CROMxx",	"mpr-20566.7", 	0x2901883B, 0x400000,	2, 0x1000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-20565.6", 	0xD6BBE638, 0x400000,	2, 0x1000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-20564.5", 	0xBE69FCA0, 0x400000,	2, 0x1000006,	8, 	TRUE },
			
			// Banked CROM2
			{ "CROMxx",	"mpr-20571.12", 0x40B459AF, 0x400000,	2, 0x2000000,	8,	TRUE },
			{ "CROMxx",	"mpr-20570.11", 0x2C0D91FC, 0x400000,	2, 0x2000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-20569.10", 0x136C014F, 0x400000,	2, 0x2000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-20568.9", 	0xFF23CF1C, 0x400000,	2, 0x2000006,	8, 	TRUE },

			// Banked CROM3
			{ "CROMxx",	"mpr-20575.16", 0xEBC99D8A, 0x400000,	2, 0x3000000,	8,	TRUE },
			{ "CROMxx",	"mpr-20574.15", 0x68567771, 0x400000,	2, 0x3000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-20573.14", 0xE0DEE793, 0x400000,	2, 0x3000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-20572.13", 0xD4A41A0B, 0x400000,	2, 0x3000006,	8, 	TRUE },
			
			// Video ROM
			{ "VROM",	"mpr-20580.26", 0x6D42775E, 0x400000, 	2, 0, 			32,	FALSE },
			{ "VROM",	"mpr-20581.27", 0xAC9EEC04, 0x400000, 	2, 2, 			32,	FALSE },
			{ "VROM",	"mpr-20582.28", 0xB202F7BD, 0x400000, 	2, 4, 			32,	FALSE },
			{ "VROM",	"mpr-20583.29", 0x0D6D508A, 0x400000, 	2, 6, 			32,	FALSE },
			{ "VROM",	"mpr-20584.30", 0xECCF4DE6, 0x400000, 	2, 8, 			32,	FALSE },
			{ "VROM",	"mpr-20585.31", 0xB383F4E5, 0x400000, 	2, 10, 			32,	FALSE },
			{ "VROM",	"mpr-20586.32", 0xE7CD5DFB, 0x400000, 	2, 12, 			32,	FALSE },
			{ "VROM",	"mpr-20587.33", 0xE2B2ABE1, 0x400000, 	2, 14, 			32,	FALSE },
			{ "VROM",	"mpr-20588.34", 0x84F4162D, 0x400000, 	2, 16, 			32,	FALSE },
			{ "VROM",	"mpr-20589.35", 0x4E653D02, 0x400000, 	2, 18, 			32,	FALSE },
			{ "VROM",	"mpr-20590.36", 0x527049BE, 0x400000, 	2, 20, 			32,	FALSE },
			{ "VROM",	"mpr-20591.37", 0x3BE20243, 0x400000, 	2, 22, 			32,	FALSE },
			{ "VROM",	"mpr-20592.38", 0xD7985B28, 0x400000, 	2, 24, 			32,	FALSE },
			{ "VROM",	"mpr-20593.39", 0xE670C4D3, 0x400000, 	2, 26, 			32,	FALSE },
			{ "VROM",	"mpr-20594.40", 0x35578240, 0x400000, 	2, 28, 			32,	FALSE },
			{ "VROM",	"mpr-20595.41", 0x1D4A2CAD, 0x400000, 	2, 30, 			32,	FALSE },
			
			{ NULL,	NULL, 0, 0, 0, 0, 0, FALSE }
		}
	},

	// Harley Davidson & L.A. Riders
	{
		"harley",
		"Harley Davidson & L.A. Riders",
		"Sega",
		1998,
		0x20,
		0x800000,	// 8 MB of fixed CROM
		TRUE,		// 40 MB of banked CROM
		0x4000000,	// 64 MB of VROM
		GAME_INPUT_COMMON|GAME_INPUT_VEHICLE,
		
		{
			// Fixed CROM
			{ "CROM",	"epr-20393a.17", 0xB5646556, 0x200000,	2, 0x0000006,	8,	TRUE },
			{ "CROM", 	"epr-20394a.18", 0xCE29E2B6, 0x200000,	2, 0x0000004,	8, 	TRUE },
			{ "CROM", 	"epr-20395a.19", 0x761F4976, 0x200000,	2, 0x0000002,	8, 	TRUE },
			{ "CROM", 	"epr-20396a.20", 0x16B0106B, 0x200000,	2, 0x0000000,	8, 	TRUE },
			
			// Banked CROM0
			{ "CROMxx",	"mpr-20361.1", 0xDDB66C2F, 0x400000,	2, 0x0000006,	8,	TRUE },
			{ "CROMxx",	"mpr-20362.2", 0xF7E60DFD, 0x400000,	2, 0x0000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-20363.3", 0x3E3CC6FF, 0x400000,	2, 0x0000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-20364.4", 0xA2A68EF2, 0x400000,	2, 0x0000000,	8, 	TRUE },
			
			// Banked CROM1
			{ "CROMxx",	"mpr-20365.5", 0x7DD50361, 0x400000,	2, 0x1000006,	8,	TRUE },
			{ "CROMxx",	"mpr-20366.6", 0x45E3850E, 0x400000,	2, 0x1000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-20367.7", 0x6C3F9748, 0x400000,	2, 0x1000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-20368.8", 0x100C9846, 0x400000,	2, 0x1000000,	8, 	TRUE },
			
			// Banked CROM3
			{ "CROMxx",	"epr-20409.13", 0x58CAAA75, 0x200000,	2, 0x3800006,	8,	TRUE },
			{ "CROMxx",	"epr-20410.14", 0x98B126F2, 0x200000,	2, 0x3800004,	8, 	TRUE },
			{ "CROMxx",	"epr-20411.15", 0x848DAAF7, 0x200000,	2, 0x3800002,	8, 	TRUE },
			{ "CROMxx",	"epr-20412.16", 0x0D51BB34, 0x200000,	2, 0x3800000,	8, 	TRUE },

			// Video ROM
			{ "VROM",	"mpr-20377.26", 0x4D2887E5, 0x400000, 	2, 0, 			32,	FALSE },
			{ "VROM",	"mpr-20378.27", 0x5AD7C0EC, 0x400000, 	2, 2, 			32,	FALSE },
			{ "VROM",	"mpr-20379.28", 0x1E51C9F0, 0x400000, 	2, 4, 			32,	FALSE },
			{ "VROM",	"mpr-20380.29", 0xE10D35AE, 0x400000, 	2, 6, 			32,	FALSE },
			{ "VROM",	"mpr-20381.30", 0x76CD36A2, 0x400000, 	2, 8, 			32,	FALSE },
			{ "VROM",	"mpr-20382.31", 0xF089AE37, 0x400000, 	2, 10, 			32,	FALSE },
			{ "VROM",	"mpr-20383.32", 0x9E96D3BE, 0x400000, 	2, 12, 			32,	FALSE },
			{ "VROM",	"mpr-20384.33", 0x5BDFBB52, 0x400000, 	2, 14, 			32,	FALSE },
			{ "VROM",	"mpr-20385.34", 0x12DB1729, 0x400000, 	2, 16, 			32,	FALSE },
			{ "VROM",	"mpr-20386.35", 0xDB2CCAF8, 0x400000, 	2, 18, 			32,	FALSE },
			{ "VROM",	"mpr-20387.36", 0xC5DDE91B, 0x400000, 	2, 20, 			32,	FALSE },
			{ "VROM",	"mpr-20388.37", 0xAEAA862E, 0x400000, 	2, 22, 			32,	FALSE },
			{ "VROM",	"mpr-20389.38", 0x49BB6593, 0x400000, 	2, 24, 			32,	FALSE },
			{ "VROM",	"mpr-20390.39", 0x1D4A8EFE, 0x400000, 	2, 26, 			32,	FALSE },
			{ "VROM",	"mpr-20391.40", 0x5DC452DC, 0x400000, 	2, 28, 			32,	FALSE },
			{ "VROM",	"mpr-20392.41", 0x892208CB, 0x400000, 	2, 30, 			32,	FALSE },
			
			{ NULL,	NULL, 0, 0, 0, 0, 0, FALSE }
		}
	},
	
	// L.A. Machineguns
	{
		"lamachin",
		"L.A. Machineguns",
		"Sega",
		1998,
		0x21,
		0x800000,	// 8 MB of fixed CROM
		TRUE,		// 48 MB of banked CROM (mirror)
		0x4000000,	// 64 MB of VROM
		GAME_INPUT_COMMON|GAME_INPUT_ANALOG_JOYSTICK,
		
		{
			// Fixed CROM
			{ "CROM",	"epr21486.20", 0x940637C2, 0x200000,	2, 0x0000006,	8,	TRUE },
			{ "CROM", 	"epr21485.19", 0x58102168, 0x200000,	2, 0x0000004,	8, 	TRUE },
			{ "CROM", 	"epr21484.18", 0xF68F7703, 0x200000,	2, 0x0000002,	8, 	TRUE },
			{ "CROM", 	"epr21483.17", 0x64DE433F, 0x200000,	2, 0x0000000,	8, 	TRUE },
			
			// Banked CROM0
			{ "CROMxx",	"mpr21454.4", 0x42BDC56C, 0x400000,	2, 0x0000006,	8,	TRUE },
			{ "CROMxx",	"mpr21453.3", 0x01AC050C, 0x400000,	2, 0x0000004,	8, 	TRUE },
			{ "CROMxx",	"mpr21452.2", 0x082D98AB, 0x400000,	2, 0x0000002,	8, 	TRUE },
			{ "CROMxx",	"mpr21451.1", 0x97FF94A7, 0x400000,	2, 0x0000000,	8, 	TRUE },
			
			// Banked CROM1
			{ "CROMxx",	"mpr21458.8", 0xB748F5A1, 0x400000,	2, 0x1000000,	8,	TRUE },
			{ "CROMxx",	"mpr21457.7", 0x2034DBD4, 0x400000,	2, 0x1000002,	8, 	TRUE },
			{ "CROMxx",	"mpr21456.6", 0x73A50547, 0x400000,	2, 0x1000004,	8, 	TRUE },
			{ "CROMxx",	"mpr21455.5", 0x0B4A3CC5, 0x400000,	2, 0x1000006,	8, 	TRUE },
			
			// Banked CROM2
			{ "CROMxx",	"mpr21462.12", 0x03D22EE8, 0x400000,	2, 0x2000000,	8,	TRUE },
			{ "CROMxx",	"mpr21462.11", 0x33D8F0DA, 0x400000,	2, 0x2000002,	8, 	TRUE },
			{ "CROMxx",	"mpr21461.10", 0x02268361, 0x400000,	2, 0x2000004,	8, 	TRUE },
			{ "CROMxx",	"mpr21460.9",  0x71A7B6B3, 0x400000,	2, 0x2000006,	8, 	TRUE },

			// Video ROM
			{ "VROM",	"mpr21467.26", 0x73635100, 0x400000, 	2, 0, 			32,	FALSE },
			{ "VROM",	"mpr21468.27", 0x462E5C81, 0x400000, 	2, 2, 			32,	FALSE },
			{ "VROM",	"mpr21469.28", 0x4BA3F192, 0x400000, 	2, 4, 			32,	FALSE },
			{ "VROM",	"mpr21470.29", 0x670F0DF5, 0x400000, 	2, 6, 			32,	FALSE },
			{ "VROM",	"mpr21471.30", 0x1F07E6E3, 0x400000, 	2, 8, 			32,	FALSE },
			{ "VROM",	"mpr21472.31", 0xE6DC64A3, 0x400000, 	2, 10, 			32,	FALSE },
			{ "VROM",	"mpr21473.32", 0xD1C9B54A, 0x400000, 	2, 12, 			32,	FALSE },
			{ "VROM",	"mpr21474.33", 0xAA2F19AE, 0x400000, 	2, 14, 			32,	FALSE },
			{ "VROM",	"mpr21475.34", 0xBAE9B381, 0x400000, 	2, 16, 			32,	FALSE },
			{ "VROM",	"mpr21476.35", 0x3833DF51, 0x400000, 	2, 18, 			32,	FALSE },
			{ "VROM",	"mpr21477.36", 0x46032C35, 0x400000, 	2, 20, 			32,	FALSE },
			{ "VROM",	"mpr21478.37", 0x35EF75B8, 0x400000, 	2, 22, 			32,	FALSE },
			{ "VROM",	"mpr21479.38", 0x783E8ECE, 0x400000, 	2, 24, 			32,	FALSE },
			{ "VROM",	"mpr21480.39", 0xC947BCB8, 0x400000, 	2, 26, 			32,	FALSE },
			{ "VROM",	"mpr21481.40", 0x6CE566AC, 0x400000, 	2, 28, 			32,	FALSE },
			{ "VROM",	"mpr21482.41", 0xE995F554, 0x400000, 	2, 30, 			32,	FALSE },
			
			{ NULL,	NULL, 0, 0, 0, 0, 0, FALSE }
		}
	},
	
	// The Ocean Hunter
	{
		"oceanhun",
		"The Ocean Hunter",
		"Sega",
		1998,
		0x21,
		0x800000,	// 8 MB of fixed CROM
		FALSE,		// 96 MB of banked CROM (do not mirror)
		0x4000000,	// 64 MB of VROM (will need to be mirrored)
		GAME_INPUT_COMMON|GAME_INPUT_ANALOG_JOYSTICK,
		
		{
			// Fixed CROM
			{ "CROM",	"epr21117.20", 0x3adfcb9d, 0x200000,	2, 0x0000006,	8,	TRUE },
			{ "CROM", 	"epr21116.19", 0x0bb9c107, 0x200000,	2, 0x0000004,	8, 	TRUE },
			{ "CROM", 	"epr21115.18", 0x69e31e85, 0x200000,	2, 0x0000002,	8, 	TRUE },
			{ "CROM", 	"epr21114.17", 0x58d985f1, 0x200000,	2, 0x0000000,	8, 	TRUE },

			// Banked CROM1
			{ "CROMxx",	"mpr21085.8", 0x5056ad33, 0x800000,	2, 0x2000006,	8,	TRUE },
			{ "CROMxx",	"mpr21084.7", 0xfdec6a23, 0x800000,	2, 0x2000004,	8, 	TRUE },
			{ "CROMxx",	"mpr21083.6", 0xc1c6b554, 0x800000,	2, 0x2000002,	8, 	TRUE },
			{ "CROMxx",	"mpr21082.5", 0x2b7224d3, 0x800000,	2, 0x2000000,	8, 	TRUE },
			
			// Banked CROM2
			{ "CROMxx",	"mpr21089.12", 0x2e8f88bd, 0x800000,	2, 0x4000000,	8,	TRUE },
			{ "CROMxx",	"mpr21088.11", 0x7ed71c8c, 0x800000,	2, 0x4000002,	8, 	TRUE },
			{ "CROMxx",	"mpr21087.10", 0xcff28641, 0x800000,	2, 0x4000004,	8, 	TRUE },
			{ "CROMxx",	"mpr21086.9",  0x3f12e1d0, 0x800000,	2, 0x4000006,	8, 	TRUE },

			// Banked CROM3
			{ "CROMxx",	"mpr21093.16", 0xbdfbf357, 0x800000,	2, 0x6000000,	8,	TRUE },
			{ "CROMxx",	"mpr21092.15", 0x5b1ced40, 0x800000,	2, 0x6000002,	8, 	TRUE },
			{ "CROMxx",	"mpr21091.14", 0x10671951, 0x800000,	2, 0x6000004,	8, 	TRUE },
			{ "CROMxx",	"mpr21090.13", 0x749d7979, 0x800000,	2, 0x6000006,	8, 	TRUE },

			// Video ROM
			{ "VROM",	"mpr21098.26", 0x91e71855, 0x400000, 	2, 0, 			32,	FALSE },
			{ "VROM",	"mpr21099.27", 0x308a2768, 0x400000, 	2, 2, 			32,	FALSE },
			{ "VROM",	"mpr21100.28", 0x5149b286, 0x400000, 	2, 4, 			32,	FALSE },
			{ "VROM",	"mpr21101.29", 0xe9ed4250, 0x400000, 	2, 6, 			32,	FALSE },
			{ "VROM",	"mpr21102.30", 0x06c6d4fc, 0x400000, 	2, 8, 			32,	FALSE },
			{ "VROM",	"mpr21103.31", 0x17c4b27a, 0x400000, 	2, 10, 			32,	FALSE },
			{ "VROM",	"mpr21104.32", 0xf6f80ffb, 0x400000, 	2, 12, 			32,	FALSE },
			{ "VROM",	"mpr21105.33", 0x99bdb52b, 0x400000, 	2, 14, 			32,	FALSE },
			{ "VROM",	"mpr21106.34", 0xad2b7981, 0x400000, 	2, 16, 			32,	FALSE },
			{ "VROM",	"mpr21107.35", 0xe108ff62, 0x400000, 	2, 18, 			32,	FALSE },
			{ "VROM",	"mpr21108.36", 0xcddc7a6e, 0x400000, 	2, 20, 			32,	FALSE },
			{ "VROM",	"mpr21109.37", 0x92d6141d, 0x400000, 	2, 22, 			32,	FALSE },
			{ "VROM",	"mpr21110.38", 0x4d6e3148, 0x400000, 	2, 24, 			32,	FALSE },
			{ "VROM",	"mpr21111.39", 0x0a046d7a, 0x400000, 	2, 26, 			32,	FALSE },
			{ "VROM",	"mpr21112.40", 0x9afd9feb, 0x400000, 	2, 28, 			32,	FALSE },
			{ "VROM",	"mpr21113.41", 0x864bf325, 0x400000, 	2, 30, 			32,	FALSE },
			
			{ NULL,	NULL, 0, 0, 0, 0, 0, FALSE }
		}
	},

	// Star Wars Trilogy
	{
		"swtrilgy",
		"Star Wars Trilogy",
		"Sega, LucasArts",
		1998,
		0x21,
		0x800000,	// 8 MB of fixed CROM
		TRUE,		// 48 MB of banked CROM (mirror)
		0x4000000,	// 64 MB of VROM
		GAME_INPUT_COMMON|GAME_INPUT_ANALOG_JOYSTICK,
		
		{
			// Fixed CROM
			{ "CROM", 	"epr-21382a.20",	0x69BAF117, 0x200000,	2, 0x0000000,	8, TRUE },
			{ "CROM", 	"epr-21381a.19", 	0x2DD34E28, 0x200000,	2, 0x0000002,	8, TRUE },
			{ "CROM",	"epr-21380a.18", 	0x780FB4E7, 0x200000,	2, 0x0000004,	8, TRUE },
			{ "CROM",	"epr-21379a.17", 	0x24DC1555, 0x200000,	2, 0x0000006,	8, TRUE },
			
			// Banked CROM0
			{ "CROMxx",	"mpr-21342.04",		0x339525CE, 0x400000,	2, 0x0000000,	8,	TRUE },
			{ "CROMxx",	"mpr-21341.03", 	0xB2A269E4, 0x400000,	2, 0x0000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-21340.02", 	0xAD36040E, 0x400000,	2, 0x0000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-21339.01", 	0xC0CE5037, 0x400000,	2, 0x0000006,	8, 	TRUE },
			
			// Banked CROM1
			{ "CROMxx",	"mpr-21346.08", 	0xC8733594, 0x400000,	2, 0x1000000,	8,	TRUE },
			{ "CROMxx",	"mpr-21345.07", 	0x6C183A21, 0x400000,	2, 0x1000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-21344.06", 	0x87453D76, 0x400000,	2, 0x1000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-21343.05", 	0x12552D07, 0x400000,	2, 0x1000006,	8, 	TRUE },
			
			// Banked CROM2
			{ "CROMxx",	"mpr-21350.12", 	0x486195E7, 0x400000,	2, 0x2000000,	8,	TRUE },
			{ "CROMxx",	"mpr-21349.11", 	0x3D39454B, 0x400000,	2, 0x2000002,	8, 	TRUE },
			{ "CROMxx",	"mpr-21348.10", 	0x1F7CC5F5, 0x400000,	2, 0x2000004,	8, 	TRUE },
			{ "CROMxx",	"mpr-21347.09", 	0xECB6B934, 0x400000,	2, 0x2000006,	8, 	TRUE },
			
			// Video ROM
			{ "VROM",	"mpr-21359.26", 	0x34EF4122, 0x400000, 	2, 0, 			32,	FALSE },
			{ "VROM",	"mpr-21360.27", 	0x2882B95E, 0x400000, 	2, 2, 			32,	FALSE },
			{ "VROM",	"mpr-21361.28", 	0x9B61C3C1, 0x400000, 	2, 4, 			32,	FALSE },
			{ "VROM",	"mpr-21362.29", 	0x01A92169, 0x400000, 	2, 6, 			32,	FALSE },
			{ "VROM",	"mpr-21363.30", 	0xE7D18FED, 0x400000, 	2, 8, 			32,	FALSE },
			{ "VROM",	"mpr-21364.31", 	0xCB6A5468, 0x400000, 	2, 10, 			32,	FALSE },
			{ "VROM",	"mpr-21365.32", 	0xAD5449D8, 0x400000, 	2, 12, 			32,	FALSE },
			{ "VROM",	"mpr-21366.33", 	0xDEFB6B95, 0x400000, 	2, 14, 			32,	FALSE },
			{ "VROM",	"mpr-21367.34", 	0xDFD51029, 0x400000, 	2, 16, 			32,	FALSE },
			{ "VROM",	"mpr-21368.35", 	0xAE90FD21, 0x400000, 	2, 18, 			32,	FALSE },
			{ "VROM",	"mpr-21369.36", 	0xBF17EEB4, 0x400000, 	2, 20, 			32,	FALSE },
			{ "VROM",	"mpr-21370.37", 	0x2321592A, 0x400000, 	2, 22, 			32,	FALSE },
			{ "VROM",	"mpr-21371.38", 	0xA68782FD, 0x400000, 	2, 24, 			32,	FALSE },
			{ "VROM",	"mpr-21372.39", 	0xFC3F4E8B, 0x400000, 	2, 26, 			32,	FALSE },
			{ "VROM",	"mpr-21373.40", 	0xB76AD261, 0x400000, 	2, 28, 			32,	FALSE },
			{ "VROM",	"mpr-21374.41", 	0xAE6C4D28, 0x400000, 	2, 30, 			32,	FALSE },
			
			{ NULL,	NULL, 0, 0, 0, 0, 0, FALSE }
		}
	},
	
	// Emergency Car Ambulance
	{
		"eca",
		"Emergency Car Ambulance",
		"Sega",
		1998,
		0x21,
		0x800000,	// 8 MB of fixed CROM
		TRUE,		// 48 MB of banked CROM (mirror)
		0x4000000,	// 64 MB of VROM
		GAME_INPUT_COMMON|GAME_INPUT_VEHICLE,
		
		{
			// Fixed CROM
			{ "CROM", 	"epr22898.20",	0xEFB96701, 0x200000,	2, 0x0000000,	8, TRUE },
			{ "CROM", 	"epr22897.19", 	0x9755DD8C, 0x200000,	2, 0x0000002,	8, TRUE },
			{ "CROM",	"epr22896.18", 	0x0FF828A8, 0x200000,	2, 0x0000004,	8, TRUE },
			{ "CROM",	"epr22895.17", 	0x07DF16A0, 0x200000,	2, 0x0000006,	8, TRUE },
			
			// Banked CROM0
			{ "CROMxx",	"mpr22873.4",	0xDD406330, 0x400000,	2, 0x0000000,	8,	TRUE },
			{ "CROMxx",	"mpr22872.3", 	0x4FDE63A1, 0x400000,	2, 0x0000002,	8, 	TRUE },
			{ "CROMxx",	"mpr22871.2", 	0xCF5BB5B5, 0x400000,	2, 0x0000004,	8, 	TRUE },
			{ "CROMxx",	"mpr22870.1", 	0x52054043, 0x400000,	2, 0x0000006,	8, 	TRUE },
			
			// Banked CROM1
			{ "CROMxx",	"mpr22877.8", 	0xE53B8764, 0x400000,	2, 0x1000000,	8,	TRUE },
			{ "CROMxx",	"mpr22876.7", 	0xA7561249, 0x400000,	2, 0x1000002,	8, 	TRUE },
			{ "CROMxx",	"mpr22875.6", 	0x1BB5C018, 0x400000,	2, 0x1000004,	8, 	TRUE },
			{ "CROMxx",	"mpr22874.5", 	0x5E990497, 0x400000,	2, 0x1000006,	8, 	TRUE },
			
			// Banked CROM3
			{ "CROMxx",	"mpr22885.16", 	0x3525B46D, 0x400000,	2, 0x3000000,	8,	TRUE },
			{ "CROMxx",	"mpr22884.15", 	0x254C3B63, 0x400000,	2, 0x3000002,	8, 	TRUE },
			{ "CROMxx",	"mpr22883.14", 	0x86D90148, 0x400000,	2, 0x3000004,	8, 	TRUE },
			{ "CROMxx",	"mpr22882.13", 	0xB161416F, 0x400000,	2, 0x3000006,	8, 	TRUE },
			
			// Video ROM
			{ "VROM",	"mpr22854.26", 	0x97A23D16, 0x400000, 	2, 0, 			32,	FALSE },
			{ "VROM",	"mpr22855.27", 	0x7249CDC9, 0x400000, 	2, 2, 			32,	FALSE },
			{ "VROM",	"mpr22856.28", 	0x9C0D1D1B, 0x400000, 	2, 4, 			32,	FALSE },
			{ "VROM",	"mpr22857.29", 	0x44E6CE2B, 0x400000, 	2, 6, 			32,	FALSE },
			{ "VROM",	"mpr22858.30", 	0x0AF40AAE, 0x400000, 	2, 8, 			32,	FALSE },
			{ "VROM",	"mpr22859.31", 	0xC64F0158, 0x400000, 	2, 10, 			32,	FALSE },
			{ "VROM",	"mpr22860.32", 	0x053AF14B, 0x400000, 	2, 12, 			32,	FALSE },
			{ "VROM",	"mpr22861.33", 	0xD26343DA, 0x400000, 	2, 14, 			32,	FALSE },
			{ "VROM",	"mpr22862.34", 	0x38347C14, 0x400000, 	2, 16, 			32,	FALSE },
			{ "VROM",	"mpr22863.35", 	0x28B558E6, 0x400000, 	2, 18, 			32,	FALSE },
			{ "VROM",	"mpr22864.36", 	0x31ED02F6, 0x400000, 	2, 20, 			32,	FALSE },
			{ "VROM",	"mpr22865.37", 	0x3E3A211A, 0x400000, 	2, 22, 			32,	FALSE },
			{ "VROM",	"mpr22866.38", 	0xA863A3C8, 0x400000, 	2, 24, 			32,	FALSE },
			{ "VROM",	"mpr22867.39", 	0x1CE6C7B2, 0x400000, 	2, 26, 			32,	FALSE },
			{ "VROM",	"mpr22868.40", 	0x2DB40CF8, 0x400000, 	2, 28, 			32,	FALSE },
			{ "VROM",	"mpr22869.41", 	0xC6D62634, 0x400000, 	2, 30, 			32,	FALSE },
			
			{ NULL,	NULL, 0, 0, 0, 0, 0, FALSE }
		}
	},

	
	// Terminate list
	{
		"",
		NULL,
		NULL,
		0,
		0,
		0,
		0,
		0,
		0,
		
		{
			{ NULL, NULL, 0, 0, 0, 0, FALSE },
			{ NULL, NULL, 0, 0, 0, 0, FALSE },
			{ NULL, NULL, 0, 0, 0, 0, FALSE },
			{ NULL, NULL, 0, 0, 0, 0, FALSE }
		}
	}
};
