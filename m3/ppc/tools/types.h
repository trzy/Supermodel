/*
 * types.h
 *
 * Fundamental data types.
 */

#ifndef INCLUDED_TYPES_H
#define INCLUDED_TYPES_H


typedef unsigned int        FLAGS;  // for storing flags
typedef int                 BOOL;   // boolean flag (at least 1 bit)

typedef unsigned int        UINT;   // unsigned integer (at least 32 bits,
                                    // preferrably the optimal integer and
                                    // address size for given architecture)
typedef signed int          INT;    // signed integer (same as above)
typedef unsigned char       UCHAR;  // unsigned character (should be 8 bits)
typedef char                CHAR;   // character (intended for text) --
                                    // no explicit signedness

typedef unsigned long long  UINT64; // unsigned 64-bit integer (quad word)
typedef signed long long    INT64;  // signed 64-bit integer (quad word)
typedef unsigned int        UINT32; // unsigned 32-bit integer (double word)
typedef signed int          INT32;  // signed 32-bit integer (double word)
typedef unsigned short      UINT16; // unsigned 16-bit integer (word)
typedef signed short        INT16;  // signed 16-bit integer (word)
typedef unsigned char       UINT8;  // unsigned 8-bit integer (byte)
typedef signed char         INT8;   // signed 8-bit integer (byte)


#endif  // INCLUDED_TYPES_H
