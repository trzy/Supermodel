;*******************************************************************************
; back/x86/x86_mmap.asm
;
; Assembly implementation of memory access routines, and 'reverse patchup'
; native-code-to-C wrappers.
;*******************************************************************************

bits 32

%include "back/x86/macros.inc"

section .text

;*******************************************************************************
; Init/Shutdown 
;*******************************************************************************

CSYMBOL X86_MMap_Setup
	ret

CSYMBOL X86_MMap_Clean
	ret

;*******************************************************************************
; Memory Region Retrival
;*******************************************************************************

CSYMBOL X86_MMap_FindFetch
	ret

CSYMBOL X86_MMap_FindRead8
	ret

CSYMBOL X86_MMap_FindRead16
	ret

CSYMBOL X86_MMap_FindRead32
	ret

CSYMBOL X86_MMap_FindWrite8
	ret

CSYMBOL X86_MMap_FindWrite16
	ret

CSYMBOL X86_MMap_FindWrite32
	ret

;*******************************************************************************
; Generic Memory Access
;*******************************************************************************

CSYMBOL X86_MMap_GenericRead8
	ret

CSYMBOL X86_MMap_GenericRead16
	ret

CSYMBOL X86_MMap_GenericRead32
	ret

CSYMBOL X86_MMap_GenericWrite8
	ret

CSYMBOL X86_MMap_GenericWrite16
	ret

CSYMBOL X86_MMap_GenericWrite32
	ret

;*******************************************************************************
; Access Wrappers
;*******************************************************************************

CSYMBOL X86_WrapRead8
	ret

CSYMBOL X86_WrapRead16
	ret

CSYMBOL X86_WrapRead32
	ret

CSYMBOL X86_WrapWrite8
	ret

CSYMBOL X86_WrapWrite16
	ret

CSYMBOL X86_WrapWrite32
	ret
