/*
 * back/x86/native.h
 *
 * Header file for native.asm
 */

#ifndef INCLUDED_NATIVE_H
#define INCLUDED_NATIVE_H

extern UINT32	ebp_base_ptr;

extern void		CallNative(void (*)(void));

#endif // INCLUDED_NATIVE_H
