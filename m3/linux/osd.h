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
 * linux/osd.h
 *
 * Linux OSD header.
 */

#ifndef INCLUDED_LINUX_OSD_H
#define INCLUDED_LINUX_OSD_H

/******************************************************************/
/* OSD Includes                                                   */
/******************************************************************/

#include <GL/gl.h>
#include <GL/glu.h>

/******************************************************************/
/* OSD Fundamental Data Types                                     */
/******************************************************************/

typedef unsigned int        FLAGS;      // for storage of bit flags

#ifdef __GNUC__

typedef int                 BOOL;       // 0 (false) or non-zero (true)

typedef signed int          INT;        // optimal signed integer
typedef unsigned int        UINT;       // optimal unsigned integer 
typedef signed char         CHAR;       // signed character (for text)
typedef unsigned char       UCHAR;      // unsigned character

typedef signed char         INT8;
typedef unsigned char		UINT8;
typedef signed short		INT16;
typedef unsigned short		UINT16;
typedef signed int			INT32;
typedef unsigned int		UINT32;

typedef signed long long	INT64;
typedef unsigned long long	UINT64;

#define TRUE (1)
#define FALSE (0)

#define stricmp strcasecmp

#endif

typedef float               FLOAT32;    // single precision (32-bit)
typedef double              FLOAT64;    // double precision (64-bit)

/******************************************************************/
/* OSD Definitions                                                */
/******************************************************************/

#define INLINE static __inline__

#define min(a,b) ((a) < (b) ? (a) : (b))

#endif  // INCLUDED_LINUX_OSD_H

