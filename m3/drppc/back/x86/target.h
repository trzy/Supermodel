/*
 * back/x86/target.h
 *
 * X86 header file as target CPU.
 */

#ifndef INCLUDED_TARGET_H
#define INCLUDED_TARGET_H

#include "toplevel.h"

#define LITTLE_ENDIAN	0
#define BIG_ENDIAN		1

#define TARGET_ENDIANESS	LITTLE_ENDIAN

extern INT	Target_Init (CODE_CACHE * cache);

#endif // INCLUDED_TARGET_H
