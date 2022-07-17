/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011-2012 Bart Trzynadlowski, Nik Henson
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
 * Supermodel.h
 * 
 * Program-wide header file.
 */
 
#ifndef INCLUDED_SUPERMODEL_H
#define INCLUDED_SUPERMODEL_H

// Used throughout Supermodel
#include <cstdio>
#include <cstdlib>
#include <cstring>


/******************************************************************************
 Program-Wide Definitions
******************************************************************************/

#include "Version.h"


/******************************************************************************
 OS-Dependent (OSD) Items
 
 Everything here must be provided by the OSD layer. The following include files
 must be located in the OSD directories for each port:
 
 	Types.h			Defines fundamental data types.
 	OSDConfig.h		COSDConfig class (OSD-specific configuration settings).
******************************************************************************/

// stricmp() is non-standard, apparently...
#ifdef _MSC_VER	// MS VisualC++
	#define stricmp	_stricmp
#elif defined(__GNUC__)
	#define stricmp	strcasecmp
#endif

// 32-bit rotate left
#ifdef _MSC_VER // MS VisualC++ - use VS intrinsic function _rotl
	#define rotl(val, shift) val = _rotl(val, shift)
#elif defined(__GNUC__)	// GCC should optimise following to asm
	#define rotl(val, shift) val = (val>>shift)|(val<<(32-shift))
#endif

/* 
 * Fundamental Data Types:
 *
 *		UINT64	Unsigned 64-bit integer.
 *		INT64	Signed 64-bit integer.
 *		UINT32	Unsigned 32-bit integer.
 *		INT32	Signed 32-bit integer.
 *		UINT16	Unsigned 16-bit integer.
 *		INT16	Signed 16-bit integer.
 *		UINT8	Unsigned 8-bit integer.
 *		INT8	Signed 8-bit integer.
 *		FLOAT32	Single-precision, 32-bit floating point number.
 *		FLOAT64	Double-precision, 64-bit floating point number.
 *
 * Boolean Values:
 *
 *		OKAY	0
 *		FAIL	1
 *
 * Types.h is used within C++ and C modules, so it must NOT include any C++-
 * specific stuff. Some modules may choose to include it directly rather than
 * use Supermodel.h, so it must exist.
 */
#include "Types.h"		// located in OSD/<port>/ directory


// Error logging interface
#include "OSD/Logger.h"


/******************************************************************************
 Helpful Macros and Inlines
******************************************************************************/

/*
 * FLIPENDIAN16(data):
 * FLIPENDIAN32(data):
 *
 * Flips the endianness of the data (reverses bytes).
 *
 * Parameters:
 *		data	Word or half-word to flip.
 *
 * Returns:
 *		Flipped word.
 */
static inline UINT16 FLIPENDIAN16(UINT16 d)
{
	return ((d>>8) | (d<<8));
}

static inline UINT32 FLIPENDIAN32(UINT32 d)
{
	return ((d>>24) | ((d<<8)&0x00FF0000) | ((d>>8)&0x0000FF00) | (d<<24));
}

#endif	// INCLUDED_SUPERMODEL_H
