#include "Supermodel.h"
#include "PolyHeader.h"

namespace New3D {

PolyHeader::PolyHeader()
{
	header = nullptr;
}

PolyHeader::PolyHeader(UINT32* h) 
{
	header = h;
}

void PolyHeader::operator = (const UINT32* h) 
{
	header = (UINT32*)h;
}

UINT32* PolyHeader::StartOfData()
{
	return header + 7;	// 7 is size of header in bytes, data immediately follows
}

bool PolyHeader::NextPoly()
{
	if (LastPoly()) {
		return false;
	}

	header += 7 + (NumVerts() - NumSharedVerts()) * 4;

	return true;
}

int	PolyHeader::NumPolysTotal()
{
	UINT32* start = header;	// save start address
	int count = 1;

	while (NextPoly()) {
		count++;
	}

	header = start;			// restore start address

	return count;
}

int	PolyHeader::NumTrianglesTotal()
{
	if (header[6] == 0) {
		return 0;			// no poly data
	}

	UINT32* start = header;	// save start address

	int count = (NumVerts() == 4) ? 2 : 1;

	while (NextPoly()) {
		count += (NumVerts() == 4) ? 2 : 1;
	}

	header = start;			// restore start address

	return count;
}

//
//  header 0
//

bool PolyHeader::SpecularEnabled()
{
	return (header[0] & 0x80) > 0;
}

float PolyHeader::SpecularValue()
{
	return (header[0] >> 26) / 63.f;					// 63 matches decompiled lib value
}

bool PolyHeader::Clockwise()
{
	return (header[0] & 0x2000000) > 0;
}

int PolyHeader::PolyNumber() 
{
	return (header[0] & 0x000FFFC00) >> 10;				// not all programs pass this, instead they are set to 0
}

bool PolyHeader::Discard()
{
	if ((header[0] & 0x100) && (header[0] & 0x200)) {
		return true;
	}

	return false;
}

bool PolyHeader::Discard1()
{
	return (header[0] & 0x200) > 0;
}

bool PolyHeader::Discard2()
{
	return (header[0] & 0x100) > 0;
}

int	PolyHeader::NumVerts() 
{
	return (header[0] & 0x40) ? 4 : 3;
}

int PolyHeader::NumSharedVerts()
{
	int sharedVerts[] = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4 };

	return sharedVerts[header[0] & 0xf];
}

bool PolyHeader::SharedVertex(int vertex)
{
	UINT32 mask = 1 << vertex;

	return (header[0] & mask) > 0;
}

//
//  header 1
//

void PolyHeader::FaceNormal(float n[3]) 
{
	n[0] = (float)(((INT32)header[1]) >> 8) * (1.0f / 4194304.0f);
	n[1] = (float)(((INT32)header[2]) >> 8) * (1.0f / 4194304.0f);
	n[2] = (float)(((INT32)header[3]) >> 8) * (1.0f / 4194304.0f);
}

float PolyHeader::UVScale()
{
	return (header[1] & 0x40) ? 1.0f : (1.0f / 8.0f);
}

bool PolyHeader::DoubleSided()
{
	return (header[1] & 0x10) ? true : false;
}

bool PolyHeader::LastPoly() 
{
	if ((header[1] & 4) > 0 || header[6] == 0) {
		return true;
	}

	return false;
}

bool PolyHeader::PolyColor()
{
	return (header[1] & 2) > 0;
}

bool PolyHeader::FixedShading()
{
	return (header[1] & 0x20) > 0;
}

bool PolyHeader::SmoothShading()
{
	return (header[1] & 0x8) > 0;
}

bool PolyHeader::NoLosReturn()
{
	return (header[1] & 0x1) > 0;
}

//
// header 2
//

bool PolyHeader::TexUMirror()
{
	return (header[2] & 2) > 0;
}

bool PolyHeader::TexVMirror()
{
	return (header[2] & 1) > 0;
}

bool PolyHeader::MicroTexture()
{
	return (header[2] & 0x10) > 0;
}

int	PolyHeader::MicroTextureID()
{
	return (header[2] >> 5) & 7;
}

int	PolyHeader::MicroTextureMinLOD()
{
	return (header[2] >> 2) & 3;
}

//
// header 3

int	PolyHeader::TexWidth()
{
	UINT32 w = (header[3] >> 3) & 7;

	if (w >= 6) {
		w = 0;
	}

	return 32 << w;
}

int	PolyHeader::TexHeight()
{
	UINT32 h = (header[3] >> 0) & 7;

	if (h >= 6) {
		h = 0;	
	}

	return 32 << h;
}

bool PolyHeader::TexSmoothU()
{
	return (header[3] & 0x80) > 0;
}

bool PolyHeader::TexSmoothV()
{
	return (header[3] & 0x40) > 0;
}

//
// header 4
//

void PolyHeader::Color(UINT8& r, UINT8& g, UINT8& b)
{
	r = (header[4] >> 24);
	g = (header[4] >> 16) & 0xFF;
	b = (header[4] >> 8) & 0xFF;
}

int PolyHeader::ColorIndex()
{
	return (header[4] >> 8) & 0xFFF;
}

int PolyHeader::SensorColorIndex()
{
	return (header[4] >> 20) & 0xFFF;
}

bool PolyHeader::TranslatorMap()
{
	return (header[4] & 0x80) > 0;
}

int	PolyHeader::Page()
{
	return (header[4] & 0x40) >> 6;
}

//
// header 5
//

int PolyHeader::X()
{
	//====
	int x;
	//====

	x = (32 * (((header[4] & 0x1F) << 1) | ((header[5] >> 7) & 1)));
	x &= 2047;
	return x;
}

int PolyHeader::Y()
{
	//=======
	int y;
	int page;
	//=======

	if (Page()) {
		page = 1024;
	}
	else {
		page = 0;
	}

	y = (32 * (header[5] & 0x1F) + page);	// if we hit 2nd page add 1024 to y coordinate
	y &= 2047;

	return y;
}

//
// header 6
//

bool PolyHeader::Layered()
{
	return (header[6] & 0x8) > 0;
}

float PolyHeader::Shininess()
{
	return (float)((header[6] >> 5) & 3);	// input sdk values are float 0-1 output are int 0-3
}

int	PolyHeader::TexFormat()
{
	return (header[6] >> 7) & 7;
}

bool PolyHeader::TexEnabled()
{
	return (header[6] & 0x400) > 0;
}

bool PolyHeader::LightEnabled()
{
	return !(header[6] & 0x00010000);
}

bool PolyHeader::AlphaTest()
{
	return (header[6] & 0x80000000) > 0;
}

UINT8 PolyHeader::Transparency()
{
	if (header[6] & 0x800000) {		// check top bit to see if its 1. Star wars is writing 1 for opaque, but the rest of the bits are garbage and are ignored
		return 255;					// without this check we get overflow. In the SDK, values are explicitly clamped to 0-32.
	}

	return (((header[6] >> 18) & 0x3F) * 255) / 32;	
}

bool PolyHeader::PolyAlpha()
{
	return (header[6] & 0x00800000) == 0;
}

bool PolyHeader::TextureAlpha()
{
	return (header[6] & 0x7) > 0;
}

bool PolyHeader::Luminous()
{
	return (header[6] & 0x00010000) > 0;
}

float PolyHeader::LightModifier()
{
	return ((header[6] >> 11) & 0x1F) * (1.0f / 16.0f);
}

bool PolyHeader::HighPriority()
{
	return (header[6] & 0x10) > 0;
}

int	PolyHeader::TranslatorMapOffset()
{
	return (header[6] >> 24) & 0x7f;
}

bool PolyHeader::TranslucencyPatternSelect()
{
	return (header[6] & 0x20000) > 0;
}

//
// hashing
//

UINT64 PolyHeader::Hash()
{
	UINT64 hash = 0;

	hash |= (UINT64)(header[3] & 0xFF);											// bits 0-7 tex width / height / uv smooth
	hash |= (UINT64)(((header[4] & 0x1F) << 1) | ((header[5] >> 7) & 1)) << 8;	// bits 8-13 x offset
	hash |= (UINT64)(header[5] & 0x1F) << 14;									// bits 14-18 y offset
	hash |= (UINT64)((header[4] & 0xC0) >> 6) << 19;							// bits 19-20 page / translatormap
	hash |= (UINT64)DoubleSided() << 21;										// bits 21 double sided
	hash |= (UINT64)AlphaTest() << 22;											// bits 22 contour processing
	hash |= (UINT64)PolyAlpha() << 23;											// bits 23 poly alpha processing
	hash |= (UINT64)(header[2] & 0xFF) << 24;									// bits 24-31 microtexture / uv mirror
	hash |= (UINT64)SpecularEnabled() << 32;									// bits 32 enable specular reflection
	hash |= (UINT64)SmoothShading() << 33;										// bits 33 smooth shading
	hash |= (UINT64)FixedShading() << 34;										// bits 34 fixed shading
	hash |= (UINT64)(header[0] >> 26) << 35;									// bits 35-40 specular coefficient (opacity)
	hash |= (UINT64)(header[6] & 0x3FFFF) << 41;								// bits 41-58 Translucency pattern select / disable lighting / Polygon light modifier / Texture enable / Texture format / Shininess / High priority / Layered polygon / Translucency mode

	return hash;
}

} // New3D
