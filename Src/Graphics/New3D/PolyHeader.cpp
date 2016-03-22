#include "Supermodel.h"
#include "PolyHeader.h"

namespace New3D {

PolyHeader::PolyHeader()
{
	header = NULL;
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

bool PolyHeader::Specular()
{
	return (header[0] & 0x800000000) > 0;
}

int PolyHeader::PolyNumber() 
{
	return (header[0] & 0x000FFFC00) >> 10;				// not all programs pass this, instead they are set to 0
}

bool PolyHeader::Disabled()
{
	if ((header[0] & 0x100) && (header[0] & 0x200)) {	// assuming these two bits mean z and colour writes are disabled
		return true;
	}

	return false;
}

int	PolyHeader::NumVerts() 
{
	return (header[0] & 0x40) ? 4 : 3;
}

int PolyHeader::NumSharedVerts()
{
	int num = 0;

	for (int i = 0; i < 4; i++) {
		if (SharedVertex(i)) {
			num++;
		}
	}

	return num;
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

//
// header 3

int	PolyHeader::TexWidth()
{
	return 32 << ((header[3] >> 3) & 7);
}
int	PolyHeader::TexHeight()
{
	return 32 << ((header[3] >> 0) & 7);
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

int	PolyHeader::Page()
{
	return (header[4] & 0x40) >> 6;
}

//
// header 5
//

int PolyHeader::X(int textureXOffset)
{
	//====
	int x;
	//====

	x = (32 * (((header[4] & 0x1F) << 1) | ((header[5] >> 7) & 1))) + textureXOffset;
	x &= 2047;
	return x;
}

int PolyHeader::Y(int textureYOffset)
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

	y = (32 * (header[5] & 0x1F) + page) + textureYOffset;	// if we hit 2nd page add 1024 to y coordinate
	y &= 2047;

	return y;
}

//
// header 6
//

int	PolyHeader::TexFormat()
{
	return (header[6] >> 7) & 7;
}

bool PolyHeader::TexEnabled()
{
	return (header[6] & 0x04000000) > 0;
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
	return (UINT8)(((header[6] >> 18) & 0x1F)  * 255.f / 0x1F);	
}

bool PolyHeader::FixedShading()
{
	return (header[6] & 0x2000000) > 0;
}

UINT8 PolyHeader::ShadeValue()
{
	return (UINT8)(((header[6] >> 26) & 0x1F) * (255.f / 0x1F));
}

bool PolyHeader::PolyAlpha()
{
	return (header[6] & 0x00800000) == 0;
}

bool PolyHeader::TextureAlpha()
{
	return (header[6] & 0x1);
}

bool PolyHeader::StencilPoly()
{
	return (header[6] & 1000000) > 0;
}

bool PolyHeader::Luminous()
{
	return (header[6] & 0x00010000) > 0;
}

float PolyHeader::LightModifier()
{
	return (float)((header[6] >> 11) & 0x1F) * (1.0f / 31.0f);
}

//
// misc
//

UINT64 PolyHeader::Hash(int textureXOffset, int textureYOffset)
{
	UINT64 hash = 0;

	hash |= (header[2] & 3);						// bits 0-1 uv mirror bits
	hash |= (UINT64)((header[3] >> 0) & 7) << 2;	// bits 2-4 tex height
	hash |= (UINT64)((header[3] >> 3) & 7) << 5;	// bits 5-7 tex width
	hash |= (UINT64)X(textureXOffset) << 8;			// bits 8-17 x offset
	hash |= (UINT64)Y(textureYOffset) << 18;		// bits 18-27 y offset
	hash |= (UINT64)TexFormat() << 28;				// bits 28-30 tex format
	hash |= (UINT64)TexEnabled() << 31;				// bits 31 textures enabled
	hash |= (UINT64)LightEnabled() << 32;			// bits 32 light enabled
	hash |= (UINT64)DoubleSided() << 33;			// bits 33 double sided
	hash |= (UINT64)AlphaTest() << 34;				// bits 34 contour processing
	hash |= (UINT64)PolyAlpha() << 35;				// bits 35 poly alpha processing
	hash |= (UINT64)TextureAlpha() << 36;			// bits 35 poly alpha processing

	//to do add the rest of the states

	return hash;
}

} // New3D
