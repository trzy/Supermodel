#ifndef _POLY_HEADER_H_
#define _POLY_HEADER_H_

#include "Types.h"

namespace New3D {

/*
Polygon Data

0x00:
xxxxxx-- -------- -------- --------		Specular
------x- -------- -------- --------		Clockwise winding
-------x xxxxxxxx xxxxxx-- --------		Polygon number (not always present)
-------- -------- ------xx --------		Discard
-------- -------- -------- x-------		Enable specular
-------- -------- -------- -x------		0 = Triangle, 1 = Quad
-------- -------- -------- --x-----		Poly is points
-------- -------- -------- ---x----		Anti-aliasing
-------- -------- -------- ----x---		Vertex 3 shared from previous polygon
-------- -------- -------- -----x--		Vertex 2 shared from previous polygon
-------- -------- -------- ------x-		Vertex 1 shared from previous polygon
-------- -------- -------- -------x		Vertex 0 shared from previous polygon

0x01: 
xxxxxxxx xxxxxxxx xxxxxxxx--------		Polygon normal X coordinate(2.22 fixed point)
-------- -------- -------- x-------		Edge on translucency (i guess anti-aliasing) 
-------- -------- -------- -x------		UV scale (0 = 13.3, 1 = 16.0)
-------- -------- -------- --x-----		Fixed shading (seems to only be enabled if lighting is enabled)
-------- -------- -------- ---x----		1 = Double-sided polygon
-------- -------- -------- ----x---		1 = smooth shading, 0 = flat shading
-------- -------- -------- -----x--		If set, this is the last polygon
-------- -------- -------- ------x-		Poly color, 1 = RGB, 0 = color table
-------- -------- -------- -------x		No los return ?

0x02: 
xxxxxxxx xxxxxxxx xxxxxxxx --------		Polygon normal Y coordinate(2.22 fixed point)
-------- -------- -------- xxx-----		Microtexture texture number 0-7
-------- -------- -------- ---x----		Microtexture enabled
-------- -------- -------- ----xx--		Microtexture min lod
-------- -------- -------- ------x-		Texture U mirror enable
-------- -------- -------- -------x		Texture V mirror enable

0x03: 
xxxxxxxx xxxxxxxx xxxxxxxx --------		Polygon normal Z coordinate(2.22 fixed point)
-------- -------- -------- x-------		X wrap smoothing
-------- -------- -------- -x------		Y wrap smoothing
-------- -------- -------- --xxx---		Texture width(in 8 - pixel tiles)
-------- -------- -------- -----xxx		Texture height(in 8 - pixel tiles)

0x04: 
xxxxxxxx xxxxxxxx xxxxxxxx --------		Color (RGB888 or two 12-bit indexes, sensor color and color)
-------- -------- -------- x-------		Translator map enable ? (Seems to be some 4/5bit colour format. Max observed value=16)
-------- -------- -------- -x------		Texture page
-------- -------- -------- ---xxxxx		Upper 5 bits of texture U coordinate

0x05 : 
xxxxxxxx xxxxxxxx xxxxxxxx --------		Texture NP ?
-------- -------- -------- x-------		Low bit of texture U coordinate
-------- -------- -------- ---xxxxx		Low 5 bits of texture V coordinate

0x06: 
x------- -------- -------- --------		Alpha testing / contour
-xxxxxxx -------- -------- --------		Translator map offset?
-------- xxxxxx-- -------- --------		Polygon transparency 0-32
-------- ------x- -------- --------		Translucency pattern select (stipple alpha) - not sure if used?
-------- -------x -------- --------		1 = disable lighting
-------- -------- xxxxx--- --------		Polygon light modifier(Amount that a luminous polygon will burn through fog. Valid range is 0.0 to 1.0. 0.0 is completely fogged; 1.0 has no fog.)
-------- -------- -----x-- --------		Texture enable
-------- -------- ------xx x-------		Texture format
-------- -------- -------- -xx-----		Shininess
-------- -------- -------- ---x----		High priority
-------- -------- -------- ----x---		Layered polygon
-------- -------- -------- -----xxx		Translucency mode
*/

class PolyHeader
{
public:
	PolyHeader();
	PolyHeader(UINT32* h);

	void operator =(const UINT32* h);

	UINT32*	StartOfData();
	bool	NextPoly();
	int		NumPolysTotal();		// could be quads or triangles
	int		NumTrianglesTotal();

	//header 0
	bool	SpecularEnabled();
	float	SpecularValue();
	bool	Clockwise();
	int		PolyNumber();
	bool	Discard();				// both bits
	bool	Discard1();				// bit 1 only
	bool	Discard2();				// bit 2 only
	int		NumVerts();
	int		NumSharedVerts();
	bool	SharedVertex(int vertex);

	//header 1
	void	FaceNormal(float n[3]);
	float	UVScale();
	bool	DoubleSided();
	bool	LastPoly();
	bool	PolyColor();			// if false uses LUT from ram
	bool	FixedShading();
	bool	SmoothShading();
	bool	NoLosReturn();			// no line of sight return	(still not exactly how sure this attribute is used yet)

	//header 2
	bool	TexUMirror();
	bool	TexVMirror();
	bool	MicroTexture();
	int		MicroTextureID();
	int		MicroTextureMinLOD();	// basically how many times it repeats compared to the base texture (i assume)

	// header 3
	int		TexWidth();
	int		TexHeight();
	bool	TexSmoothU();
	bool	TexSmoothV();

	//header 4
	void	Color(UINT8& r, UINT8& g, UINT8& b);
	int		ColorIndex();
	int		SensorColorIndex();
	bool	TranslatorMap();
	int		Page();

	// header 5
	int		X();
	int		Y();

	//header 6
	bool	Layered();
	bool	HighPriority();
	float	Shininess();
	int		TexFormat();
	bool	TexEnabled();
	bool	LightEnabled();
	bool	AlphaTest();
	UINT8	Transparency();			// 0-255
	bool	PolyAlpha();
	bool	TextureAlpha();
	bool	Luminous();
	float	LightModifier();
	int		TranslatorMapOffset();
	bool	TranslucencyPatternSelect();

	// misc
	UINT64	Hash();		// make a unique hash for sorting by state


	//=============
	UINT32* header;
	//=============
};

} // New3D

#endif
