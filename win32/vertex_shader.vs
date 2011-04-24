vs_3_0

; parameters
; c0...c3		world-view-projection matrix
; c4...c7		world-view matrix
; c16			sun light vector
; c17			sun ambient, diffuse intensity

; input declarations
dcl_position		v0
dcl_color0			v1
dcl_texcoord0		v2
dcl_texcoord1		v3
dcl_texcoord2		v4
dcl_normal			v5
dcl_color1			v6

; output declarations
dcl_position	o0.xyzw
dcl_color0		o1.xyzw
dcl_texcoord0	o2.xyzw
dcl_texcoord1	o3.xyzw
dcl_texcoord2	o4.xyzw
dcl_color1		o5.xyzw

; constants
def c128, 0.0f, 1.0f, 0.5f, 1.0f

; transform position to projection space
;dp4 o0.x, v0, c0
;dp4 o0.y, v0, c1
;dp4 o0.z, v0, c2
;dp4 o0.w, v0, c3
dp4 r20.x, v0, c0
dp4 r20.y, v0, c1
dp4 r20.z, v0, c2
dp4 r20.w, v0, c3

mov o0, r20

; calculate parallel lighting
m3x3 r4.xyz, v5, c4			; transform N

mov r2, -c16
dp3 r3, r4, r2				; N dot L
max r3, r3, c128.xxxx		; clamp negative values to 0

mul r3, r3, c17.xxxx		; scale with diffuse intensity
add r3, r3, c17.yyyy		; add ambient intensity

min r3, r3, c128.yyyy

if_ne v6.xxxx, c128.xxxx	; if v6.x == 0 -> apply lighting, if v6.x != 0 -> self-luminance
mov r3.xyz, v6.xxx			; self-luminance
endif

mov r3.w, c128.w			; multiply alpha with 1

mul o1, v1, r3				; modulate and write color
;mov o1, r3

; move texture parameters
mov r10,v2
mov r10.w, r20.w
mov o2, r10
;mov o2, v2
mov o3, v3
mov o4, v4

; parameters (X=texture enable)
mov o5.x, v6.g