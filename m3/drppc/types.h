/*
 * types.h
 *
 * Fundamental data types.
 */

#ifndef INCLUDED_TYPES_H
#define INCLUDED_TYPES_H

typedef int                 BOOL;

typedef unsigned int        UINT;
typedef signed int          INT;
typedef unsigned char       UCHAR;
typedef char                CHAR;

typedef unsigned int		FLAGS;

#ifdef __GNUC__
typedef unsigned long long  UINT64;
typedef signed long long    INT64;
#else
typedef unsigned __int64    UINT64;
typedef signed __int64      INT64;
#endif
typedef unsigned int        UINT32;
typedef signed int          INT32;
typedef unsigned short      UINT16;
typedef signed short        INT16;
typedef unsigned char       UINT8;
typedef signed char         INT8;

typedef float				FLOAT32;
typedef double				FLOAT64;

#define FALSE	0
#define TRUE	1

#endif  // INCLUDED_TYPES_H
