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
 * rom_list.h
 *
 * ROM information structures. This file is only supposed to be included by
 * model3.c -- please see it for more details on how this file is used.
 */

/*
 * All the CROMs and VROMs were placed in _loading_ order (not IC# order).
 * Not sure about SROMs and DSB ROMs though.
 * Unused ROMs' (i.e. DSB ROMs, or VROMs for upgrade ROMSETs) size must be zero.
 * They're detected in load_romfile and ignored.
 * If DSB ROMs are non-zero, DSB1 is assumed to be present; absent otherwise.
 */

/******************************************************************/
/* Lost World, The                                                */
/******************************************************************/

{
	"LOSTWORLD",
	"",
	"The Lost World",
	"SEGA",
	1996,
    0x15,
    GAME_OWN_GUN,
	/* CROM */
    {   { "19936.20",    512*1024, 0x2F1CA664 },
        { "19937.19",    512*1024, 0x9DBF5712 },
        { "19938.18",    512*1024, 0x38AFE27A },
        { "19939.17",    512*1024, 0x8788B939 }, },
	/* CROM0 */
    {   { "MPR19921.004", 4*1024*1024, 0x9AF3227F },
        { "MPR19920.003", 4*1024*1024, 0x8DF33574 },
        { "MPR19919.002", 4*1024*1024, 0xFF119949 },
        { "MPR19918.001", 4*1024*1024, 0x95B690E9 }, },
	/* CROM1 */
    {   { "MPR19925.008", 4*1024*1024, 0xCFA4BB49 },
        { "MPR19924.007", 4*1024*1024, 0x4EE3DDC5 },
        { "MPR19923.006", 4*1024*1024, 0xED515CB2 },
        { "MPR19922.005", 4*1024*1024, 0x4DFD7FC6 }, },
	/* CROM2 */
    {   { "MPR19929.012", 4*1024*1024, 0x16491F63 },
        { "MPR19928.011", 4*1024*1024, 0x9AFD5D4A },
        { "MPR19927.010", 4*1024*1024, 0x0C96EF11 },
        { "MPR19926.009", 4*1024*1024, 0x05A232E0 }, },
	/* CROM3 */
    {   { "MPR19933.016", 4*1024*1024, 0x8E2ACD3B },
        { "MPR19932.015", 4*1024*1024, 0x04389385 },
        { "MPR19931.014", 4*1024*1024, 0x448A5007 },
        { "MPR19930.013", 4*1024*1024, 0xB598C2F2 }, },
	/* VROMs */
    {   { "19903.27", 2*1024*1024, 0xFE575871 },
        { "19902.26", 2*1024*1024, 0x178BD471 },
        { "19905.29", 2*1024*1024, 0x6FA122EE },
        { "19904.28", 2*1024*1024, 0x57951D7D },
        { "19907.31", 2*1024*1024, 0x84A425CD },
        { "19906.30", 2*1024*1024, 0xA5B16DD9 },
        { "19909.33", 2*1024*1024, 0x8FCA65F9 },
        { "19908.32", 2*1024*1024, 0x7702AA7C },
        { "19911.35", 2*1024*1024, 0xCA26A48D },
        { "19910.34", 2*1024*1024, 0x1EF585E2 },
        { "19913.37", 2*1024*1024, 0xC003049E },
        { "19912.36", 2*1024*1024, 0xFFE000E0 },
        { "19915.39", 2*1024*1024, 0xFD0F2A2B },
        { "19914.38", 2*1024*1024, 0x3C21A953 },
        { "19917.41", 2*1024*1024, 0x3035833B },
        { "19916.40", 2*1024*1024, 0x10B0C52E }, },
	/* SROMs */
    {   { "19940.21",    512*1024, 0xB06FFE5F },
        { "19934.22", 2*1024*1024, 0x661B09D0 },
        { "19935.24", 2*1024*1024, 0x6436E483 }, },
	/* DSB ROMs */
	{	{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },	},
},

/******************************************************************/
/* Scud Race                                                      */
/******************************************************************/

{
	"SCUDRACE",
	"",
	"Scud Race",
	"SEGA",
	1996,
    0x15,
    GAME_OWN_STEERING_WHEEL,
	/* CROM */
	{	{ "EPR19734.20",    512*1024, 0xBE897336 },
		{ "EPR19733.19",    512*1024, 0x6565E29A },
		{ "EPR19732.18",    512*1024, 0x23E864BB },
		{ "EPR19731.17",    512*1024, 0x3EE6447E },	},
	/* CROM0 */
	{	{ "MPR19661.04", 4*1024*1024, 0x8E3FD241 },
		{ "MPR19660.03", 4*1024*1024, 0xD999C935 },
		{ "MPR19659.02", 4*1024*1024, 0xC47E7002 },
		{ "MPR19658.01", 4*1024*1024, 0xD523235C },	},
	/* CROM1 */
	{	{ "MPR19665.08", 4*1024*1024, 0xF98C87F9 },
		{ "MPR19664.07", 4*1024*1024, 0xD9D11294 },
		{ "MPR19663.06", 4*1024*1024, 0xF6AF1CA4 },
		{ "MPR19662.05", 4*1024*1024, 0x3C700EFF },	},
	/* CROM2 */
	{	{ "MPR19669.12", 4*1024*1024, 0xCDC43C61 },
		{ "MPR19668.11", 4*1024*1024, 0x0B4DD8D5 },
		{ "MPR19667.10", 4*1024*1024, 0xA8676799 },
		{ "MPR19666.09", 4*1024*1024, 0xB52DC97F },	},
	/* CROM3 */
	{	{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },	},
	/* VROMs */
	{	{ "MPR19673.27", 2*1024*1024, 0x156ABAA9 },
		{ "MPR19672.26", 2*1024*1024, 0x588C29FD },
		{ "MPR19675.29", 2*1024*1024, 0xFF113396 },
		{ "MPR19674.28", 2*1024*1024, 0xC7B0F98C },
		{ "MPR19677.31", 2*1024*1024, 0xC6AC0347 },
		{ "MPR19676.30", 2*1024*1024, 0xFD852EAD },
		{ "MPR19679.33", 2*1024*1024, 0xE126C3E3 },
		{ "MPR19678.32", 2*1024*1024, 0xB8810CFE },
		{ "MPR19681.35", 2*1024*1024, 0xC949325F },
		{ "MPR19680.34", 2*1024*1024, 0x00EA5CEF },
		{ "MPR19683.37", 2*1024*1024, 0xE5856419 },
		{ "MPR19682.36", 2*1024*1024, 0xCE5Ca065 },
		{ "MPR19685.39", 2*1024*1024, 0x42B49304 },
		{ "MPR19684.38", 2*1024*1024, 0x56F6EC97 },
		{ "MPR19687.41", 2*1024*1024, 0x776CE694 },
		{ "MPR19686.40", 2*1024*1024, 0x84EED592 },	},
	/* SROMs */
	{
		{ "EPR19692.21",    512*1024, 0xA94F5521 },
		{ "MPR19670.22", 4*1024*1024, 0xBD31CC06 },
		{ "MPR19671.24", 4*1024*1024, 0x8E8526AB },	},
	/* DSB ROMs */
	{	{ "EPR19612.02",    128*1024, 0x13978FD4 },
		{ "MPR19603.57", 2*1024*1024, 0xB1B1765F },
		{ "MPR19604.58", 2*1024*1024, 0x6AC85B49 },
		{ "MPR19605.59", 2*1024*1024, 0xBEC891EB },
		{ "MPR19606.60", 2*1024*1024, 0xADAD46B2 },	},
},

/******************************************************************/
/* Virtua Fighter 3                                               */
/******************************************************************/

{
	"VF3",
	"",
	"Virtua Fighter 3",
	"SEGA",
	1996,
    0x10,
    0,
	/* CROM */
	{	{ "EP19230C.20",    512*1024, 0x736A9431 },
		{ "EP19229C.19",    512*1024, 0x731B6B78 },
		{ "EP19228C.18",    512*1024, 0x9C5727E2 },
		{ "EP19227C.17",    512*1024, 0xA7DF4D75 },	},
	/* CROM0 */
	{	{ "MPR19196.04", 2*1024*1024, 0x02876E86 },
		{ "MPR19195.03", 2*1024*1024, 0x4ACDA4F4 },
		{ "MPR19194.02", 2*1024*1024, 0x0348A314 },
		{ "MPR19193.01", 2*1024*1024, 0xC2C78EC5 },	},
	/* CROM1 */
	{	{ "MPR19200.08", 2*1024*1024, 0x30BA68F2 },
		{ "MPR19199.07", 2*1024*1024, 0xFCC7384D },
		{ "MPR19198.06", 2*1024*1024, 0x4A77B8D4 },
		{ "MPR19197.05", 2*1024*1024, 0x844F6C00 },	},
	/* CROM2 */
	{	{ "MPR19204.12", 2*1024*1024, 0xFCD5C563 },
		{ "MPR19203.11", 2*1024*1024, 0xE0628262 },
		{ "MPR19202.10", 2*1024*1024, 0x85D18403 },
		{ "MPR19201.09", 2*1024*1024, 0x296C18F2 },	},
	/* CROM3 */
	{	{ "MPR19208.16", 2*1024*1024, 0x35262344 },
		{ "MPR19207.15", 2*1024*1024, 0x15E16AAA },
		{ "MPR19206.14", 2*1024*1024, 0x06272D18 },
		{ "MPR19205.13", 2*1024*1024, 0x30DB905A },	},
	/* VROMs */
	{	{ "MPR19212.27", 2*1024*1024, 0x75036234 },
		{ "MPR19211.26", 2*1024*1024, 0x9C8F5DF1 },
		{ "MPR19214.29", 2*1024*1024, 0xA6F5576B },
		{ "MPR19213.28", 2*1024*1024, 0x67B123CF },
		{ "MPR19216.31", 2*1024*1024, 0x201BB1ED },
		{ "MPR19215.30", 2*1024*1024, 0xC6FD9F0D },
		{ "MPR19218.33", 2*1024*1024, 0xCFF91953 },
		{ "MPR19217.32", 2*1024*1024, 0x4DADD41A },
		{ "MPR19220.35", 2*1024*1024, 0xE62924D0 },
		{ "MPR19219.34", 2*1024*1024, 0xC610D521 },
		{ "MPR19222.37", 2*1024*1024, 0x61A6AA7D },
		{ "MPR19221.36", 2*1024*1024, 0x24F83E3C },
		{ "MPR19224.39", 2*1024*1024, 0x0A79A1BD },
		{ "MPR19223.38", 2*1024*1024, 0x1A8C1980 },
		{ "MPR19226.41", 2*1024*1024, 0x00091722 },
		{ "MPR19225.40", 2*1024*1024, 0x91A985EB },	},
	/* SROMs */
	{	{ "EPR19231.21",    128*1024, 0xF2AE2558 },
		{ "MPR19209.22", 2*1024*1024, 0x616470C8 },
		{ "MPR19211.26", 2*1024*1024, 0x450CDEF8 },	},
	/* DSB ROMs */
	{	{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },	},
},

/******************************************************************/
/* Virtua Fighter 3 Team Battle                                   */
/******************************************************************/

{
	"VF3TB",
	"VF3",
	"Virtua Fighter 3 Team Battle",
	"SEGA",
	1996,
    0x10,
    0,
	/* CROM */
	{	{ "EPR20129.20",    512*1024, 0x0DB897CE },
		{ "EPR20128.19",    512*1024, 0xFFBDBDC5 },
		{ "EPR20127.18",    512*1024, 0x5C0F694B },
		{ "EPR20126.17",    512*1024, 0x27ECD3B0 },	},
	/* CROM0 */
	{	{ "MPR20133.04", 2*1024*1024, 0xD9A10A89 },
		{ "MPR20132.03", 2*1024*1024, 0x91EBC0FB },
		{ "MPR20131.02", 2*1024*1024, 0xD86EC71B },
		{ "MPR20130.01", 2*1024*1024, 0x19E1EACA },	},
	/* CROM1 */
	{	{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },	},
	/* CROM2 */
	{	{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },	},
	/* CROM3 */
	{	{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },	},
	/* VROMs */
	{	{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },	},
	/* SROMs */
	{	{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },	},
	/* DSB ROMs */
	{	{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },	},
},

/******************************************************************/
/* Virtua Striker 2 '99                                           */
/******************************************************************/

{
	"VS2V991",
	"",
	"Virtua Striker 2 '99",
	"SEGA",
	1996,
    0x21,
    0,
	/* CROM */
	{	{ "EP21538B.20", 1*1024*1024, 0xB3F0CE2A },
		{ "EP21537B.19", 1*1024*1024, 0xA8B3FA5C },
		{ "EP21536B.18", 1*1024*1024, 0x1F2BD190 },
		{ "EP21535B.17", 1*1024*1024, 0x76C5FA8E },	},
	/* CROM0 */
	{	{ "MPR21500.04", 2*1024*1024, 0x02E9B828 },
		{ "MPR21499.03", 2*1024*1024, 0xB83E347F },
		{ "MPR21498.02", 2*1024*1024, 0x3D95041B },
		{ "MPR21497.01", 2*1024*1024, 0xF8FDA298 },	},
	/* CROM1 */
	{	{ "MPR21504.08", 2*1024*1024, 0x189F60FD },
		{ "MPR21503.07", 2*1024*1024, 0x14E8A726 },
		{ "MPR21502.06", 2*1024*1024, 0x37E5F1E5 },
		{ "MPR21501.05", 2*1024*1024, 0x2EB3AABD },	},
	/* CROM2 */
	{	{ "MPR21508.12", 2*1024*1024, 0x2E32CDEB },
		{ "MPR21507.11", 2*1024*1024, 0xA7DB6D0C },
		{ "MPR21506.10", 2*1024*1024, 0x80381466 },
		{ "MPR21505.09", 2*1024*1024, 0xEB61EC5E },	},
	/* CROM3 */
	{	{ "MPR21512.16", 2*1024*1024, 0x44B9687B },
		{ "MPR21511.15", 2*1024*1024, 0x36507BF6 },
		{ "MPR21510.14", 2*1024*1024, 0x2C0D1948 },
		{ "MPR21509.13", 2*1024*1024, 0xA5ED3225 },	},
	/* VROMs */
	{	{ "MPR21516.27", 2*1024*1024, 0x8971A753 },
		{ "MPR21515.26", 2*1024*1024, 0x8CE9910B },
		{ "MPR21518.29", 2*1024*1024, 0x4134026C },
		{ "MPR21517.28", 2*1024*1024, 0x55A4533B },
		{ "MPR21520.31", 2*1024*1024, 0xC53BE8CC },
		{ "MPR21519.30", 2*1024*1024, 0xEF6757DE },
		{ "MPR21522.33", 2*1024*1024, 0xE3B79973 },
		{ "MPR21521.32", 2*1024*1024, 0xABB501DC },
		{ "MPR21524.35", 2*1024*1024, 0x8633B6E9 },
		{ "MPR21523.34", 2*1024*1024, 0xFE4D1EAC },
		{ "MPR21526.37", 2*1024*1024, 0x5FE5F9B0 },
		{ "MPR21525.36", 2*1024*1024, 0x3C490167 },
		{ "MPR21528.39", 2*1024*1024, 0x4E346A6C },
		{ "MPR21527.38", 2*1024*1024, 0x10D0FE7E },
		{ "MPR21530.41", 2*1024*1024, 0x78400D5E },
		{ "MPR21529.40", 2*1024*1024, 0x9A731A00 },	},
	/* SROMs */
	{	{ "EP21539A.21",    512*1024, 0xA1D3E00E },
		{ "MPR21513.22", 2*1024*1024, 0x5CCD606A },
		{ "MPR21514.24", 2*1024*1024, 0x101ACBA9 },	},
	/* DSB ROMs */
	{	{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },	},
},

/******************************************************************/
/* Virtual On 2                                                   */
/******************************************************************/

{
	"VON2",
	"",
	"Virtua On 2 - Oratorio Tangram",
	"SEGA",
	1996,
    0x20,
    0,
	/* CROM */

    {   { "CROM0.U20", 2*1024*1024, 0x3EA4DE9F },
        { "CROM1.160", 2*1024*1024, 0xAE82CB35 },
        { "CROM2.U18", 2*1024*1024, 0x1FC15431 },
        { "CROM3.160", 2*1024*1024, 0x59D9C974 }, },
	/* CROM0 */
    {   { "CROM00-3.U4", 4*1024*1024, 0x81F96649 },
        { "CROM01-3.U3", 4*1024*1024, 0xB8FD56BA },
        { "CROM02-3.U2", 4*1024*1024, 0x107309E0 },
        { "CROM03-3.U1", 4*1024*1024, 0xE8586380 }, },
	/* CROM1 */
    {   { "CROM10-3.U8", 4*1024*1024, 0x763EF905 },
        { "CROM11-3.U7", 4*1024*1024, 0x858E6BBA },
        { "CROM12-3.U6", 4*1024*1024, 0x64C6FBB6 },
        { "CROM13-3.U5", 4*1024*1024, 0x8373CAB3 }, },
	/* CROM2 */
    {   { "CROM20-3.U12", 4*1024*1024, 0xB80175B9 },
        { "CROM21-3.U11", 4*1024*1024, 0x14BF8964 },
        { "CROM22-3.U10", 4*1024*1024, 0x466BEE13 },
        { "CROM23-3.U9",  4*1024*1024, 0xF0A471E9 }, },
	/* CROM3 */
    {   { "CROM30-3.U16", 4*1024*1024, 0x7130CB61 },
        { "CROM31-3.U15", 4*1024*1024, 0x50E6189E },
        { "CROM32-3.U14", 4*1024*1024, 0xD961D385 },
        { "CROM33-3.U13", 4*1024*1024, 0xEDB63E7B }, },
	/* VROMs */
	{	{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },	},
	/* SROMs */
	{	{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },	},
	/* DSB ROMs */
	{	{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },
		{ "",                      0, 0x00000000 },	},
},

