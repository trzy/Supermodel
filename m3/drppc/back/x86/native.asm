;*******************************************************************************
;*
;* back/x86/native.asm
;*
;* Assembly implementation of native generated code call, memory access routines
;* and memory region caching (self modifying code).
;*******************************************************************************

bits 32

%imacro CSYMBOL 1
	global _%1
	align 16
	_%1:
	%1:
%endmacro

section .data

global _ebp_base_ptr
_ebp_base_ptr		dd	0x00000000;

section .text

;*******************************************************************************
;*
;* void CallNative (void (* Block)(void));
;*
;* Calls the specified BB code, initializing EBP so that the very first 256
;* bytes of the context can be addressed using EBP-relative operands, with a
;* 8-bit offset, to cut down the code size.
;*
;*******************************************************************************

ALIGN 16
CSYMBOL	CallNative

	pushad
	mov		ebp, dword [_ebp_base_ptr]
	call	dword [esp+36]
	popad

	ret

;*******************************************************************************
;*
;*******************************************************************************

CSYMBOL Target_MMap_Setup
	ret

;*******************************************************************************
;*
;*******************************************************************************

CSYMBOL Target_MMap_Clean
	ret

;*******************************************************************************
;*
;*******************************************************************************

CSYMBOL Target_MMap_FindFetch
	ret

CSYMBOL Target_MMap_FindRead8
	ret

CSYMBOL Target_MMap_FindRead16
	ret

CSYMBOL Target_MMap_FindRead32
	ret

CSYMBOL Target_MMap_FindWrite8
	ret

CSYMBOL Target_MMap_FindWrite16
	ret

CSYMBOL Target_MMap_FindWrite32
	ret

;*******************************************************************************
;*
;*******************************************************************************

CSYMBOL Target_MMap_GenericRead8
	ret

CSYMBOL Target_MMap_GenericRead16
	ret

CSYMBOL Target_MMap_GenericRead32
	ret

CSYMBOL Target_MMap_GenericWrite8
	ret

CSYMBOL Target_MMap_GenericWrite16
	ret

CSYMBOL Target_MMap_GenericWrite32
	ret
