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
 * Supermodel.h
 * 
 * Program-wide header file.
 */
 
#ifndef INCLUDED_SUPERMODEL_H
#define INCLUDED_SUPERMODEL_H

/******************************************************************************
 Program-Wide Definitions
******************************************************************************/

#define SUPERMODEL_VERSION	"0.2-WIP"	// version string 


/******************************************************************************
 OS-Dependent (OSD) Items
 
 Everything here must be provided by the OSD layer. Include files should be
 located in the OSD directories for each port.
******************************************************************************/

// stricmp() is non-standard, apparently...
#ifdef _MSC_VER	// MS VisualC++
	#define stricmp	_stricmp
#else			// assume GCC
	#define stricmp	strcasecmp
#endif

/* 
 * Fundamental Data Types:
 *
 *		BOOL	Boolean (w/ TRUE = FAIL = 1, OKAY = FALSE = 0).
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
 */
#include "Types.h"

/*
 * Error and Debug Logging
 */
 
/*
 * DebugLog(fmt, ...):
 *
 * Prints debugging information. The OSD layer may choose to print this to a
 * log file, the screen, neither, or both. Newlines and other formatting codes
 * must be explicitly included.
 *
 * Parameters:
 *		fmt		A format string (the same as printf()).
 *		...		Variable number of arguments, as required by format string.
 */
extern void	DebugLog(const char *fmt, ...);

/*
 * ErrorLog(fmt, ...):
 *
 * Prints error information. Errors need not require program termination and
 * may simply be informative warnings to the user. Newlines should not be 
 * included in the format string -- they are automatically added at the end of
 * a line.
 *
 * Parameters:
 *		fmt		A format string (the same as printf()).
 *		...		Variable number of arguments, as required by format string.
 *
 * Returns:
 *		Must always return FAIL.
 */
extern BOOL	ErrorLog(const char *fmt, ...);

/*
 * InfoLog(fmt, ...);
 *
 * Prints information to the error log file but does not print to stderr. This
 * is useful for logging non-error information. Newlines are automatically
 * appended.
 *
 * Parameters:
 *		fmt		Format string (same as printf()).
 *		...		Variable number of arguments as required by format string.
 */
extern void InfoLog(const char *fmt, ...);


/******************************************************************************
 Header Files
 
 All primary header files for modules used throughout Supermodel are included 
 here, except for external packages and APIs.
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Games.h"
#include "ROMLoad.h"
#include "INIFile.h"
#include "BlockFile.h"
#include "Graphics/Render2D.h"
#include "Graphics/Render3D.h"
#include "Graphics/Shader.h"
#include "CPU/PowerPC/PPCDisasm.h"
#include "CPU/PowerPC/ppc.h"
#ifdef SUPERMODEL_SOUND
#include "CPU/68K/Turbo68K.h"
#endif // SUPERMODEL_SOUND
#include "Inputs/Input.h"
#include "Inputs/Inputs.h"
#include "Inputs/InputSource.h"
#include "Inputs/InputSystem.h"
#include "Inputs/InputTypes.h"
#include "Inputs/MultiInputSource.h"
#include "Model3/Bus.h"
#include "Model3/IRQ.h"
#include "Model3/PCI.h"
#include "Model3/53C810.h"
#include "Model3/MPC10x.h"
#include "Model3/RTC72421.h"
#include "Model3/93C46.h"
#include "Model3/TileGen.h"
#include "Model3/Real3D.h"
#include "Sound/SCSP.h"
#include "Model3/SoundBoard.h"
#include "Model3/Model3.h"

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
	return(((d >> 8) & 0x00FF) |
		   ((d << 8) & 0xFF00));
}

static inline UINT32 FLIPENDIAN32(UINT32 d)
{
	return(((d >> 24) & 0x000000FF) |
		   ((d >>  8) & 0x0000FF00) |
		   ((d <<  8) & 0x00FF0000) |
		   ((d << 24) & 0xFF000000));
}

#endif	// INCLUDED_SUPERMODEL_H
