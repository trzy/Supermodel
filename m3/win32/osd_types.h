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
 * osd_types.h
 *
 * Fundamental data types.
 */

#ifndef INCLUDED_OSD_TYPES_H
#define INCLUDED_OSD_TYPES_H

typedef unsigned int        FLAGS;      // for storage of bit flags

#ifdef __GCC__

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

#ifdef _MSC_VER // Microsoft VisualC++ (for versions 6.0 or older, non-C99)
typedef signed __int64      INT64;
typedef unsigned __int64    UINT64;
#else           // assume a C99-compliant compiler
typedef signed long long	INT64;
typedef unsigned long long	UINT64;
#endif

#endif

typedef float               FLOAT32;    // single precision (32-bit)
typedef double              FLOAT64;    // double precision (64-bit)

#endif  // INCLUDED_OSD_TYPES_H

