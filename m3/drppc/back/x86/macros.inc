;*******************************************************************************
;
; back/x86/macros.inc
;
; Miscellaneous macros for assembler portions of the backend. Useable by Nasm
; 0.98 and later.
;*******************************************************************************

bits 32

section .text

;*******************************************************************************
;
; CSYMBOL symbol
;
; It declares a global C call-able symbol. 'symbol' is the identifier as should
; be seen from C.
;*******************************************************************************

%imacro CSYMBOL 1
	global _%1
	align 16
	_%1:
	%1:
%endmacro
