; Vertex Shader for Model 3 emulator
; Written by Ville Linde
;
;
; Constants:
; c0 - c3: Projection-space transformation matrix
; c4 - c7: View-space transformation matrix for normals
; c32: Directional light vector (sun) (in eye-space !)
; c33: Directional light diffuse color
; c34: Directional light ambient color

; Input registers:
; v0: position
; v1: normal
; v2: diffuse color
; v3: texture coordinates
; v4: specular, luminosity in alpha

vs.1.1

dcl_position	v0
dcl_normal	v1
dcl_color0	v2
dcl_texcoord0	v3
dcl_color1	v4

def c91, 0.0f, 0.0f, 1.0f, 0.0f		; Eye vector (always constant in view-space)
def c92, 127.0f, 1.0f, 0.0f, 0.0f
def c93, 1.0f, 1.0f, 1.0f, 1.0f
def c94, 0.0f, 0.0f, 0.0f, 0.0f
def c95, 1.0f, 1.0f, 1.0f, 0.5f

; transform position to projection-space

dp4 r0.x, v0, c0
dp4 r0.y, v0, c1
dp4 r0.z, v0, c2
dp4 r0.w, v0, c3

mov oPos, r0		; write position

; transform normal to view-space

;mov r1, v1
;mov r10, v0
;mov r1.w, c93.x
;mov r10.w, c93.x

;add r1, r1, r10
;mul r1, r1, c95.wwww	// / 2
;mul r9, r1, r1		// x^2,y^2,z^2
;add r9.x, r9.x, r9.y
;add r9.x, r9.x, r9.z
;rsq r9.x, r9.x
;mul r10, r1, r9.xxxx

;m3x3 r1, r10, c4
m3x3 r1, v1, c4

; calculate parallel lighting
mov r11, -c32		; r11 = L

dp3 r3.x, r1, r11	; calculate N dot L vector

max r4, r3.x, c94.x	; clamp negative values to 0

; self-luminance
add r4, r4, v4.aaaa	; add luminance
add r4, r4, c34.x	; add ambient

min r4, r4, c93.xxxx	; clamp to 1.0f
max r4, r4, c94.xxxx	; clamp to 0.0f

mul r4, r4, v2		; modulate color with calculated intensity
mov r4.w, v2.w

mov oD0, r4		; write color value

mov oT0, v3		; write texcoords