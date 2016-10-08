#include "TextureSheet.h"

namespace New3D {

TextureSheet::TextureSheet()
{
	m_temp.resize(1024 * 1024 * 4);	// temporay buffer for textures
}

int TextureSheet::ToIndex(int x, int y)
{
	return (y * 2048) + x;
}

std::shared_ptr<Texture> TextureSheet::BindTexture(const UINT16* src, int format, bool mirrorU, bool mirrorV, int x, int y, int width, int height)
{
	//========
	int	index;
	//========

	x &= 2047;
	y &= 2047;

	if ((x + width) > 2048 || (y + height) > 2048) {
		return nullptr;
	}

	if (width > 1024 || height > 1024) {	// sanity checking
		return nullptr;
	}

	index = ToIndex(x, y);

	auto range = m_texMap[format].equal_range(index);

	if (range.first == range.second) {	

		// nothing found so create a new texture 

		std::shared_ptr<Texture> t(new Texture());
		m_texMap[format].insert(std::pair<int, std::shared_ptr<Texture>>(index, t));
		t->UploadTexture(src, m_temp.data(), format, mirrorU, mirrorV, x, y, width, height);
		return t;
	}
	else {

		// iterate to try and find a match

		for (auto it = range.first; it != range.second; ++it) {

			int x2, y2, width2, height2, format2;

			it->second->GetDetails(x2, y2, width2, height2, format2);

			if (width == width2 && height == height2) {
				return it->second;
			}
		}

		// nothing found so create a new entry

		std::shared_ptr<Texture> t(new Texture());
		m_texMap[format].insert(std::pair<int, std::shared_ptr<Texture>>(index, t));
		t->UploadTexture(src, m_temp.data(), format, mirrorU, mirrorV, x, y, width, height);
		return t;
	}
}

void TextureSheet::Release()
{
	for (int i = 0; i < 12; i++) {
		m_texMap[i].clear();
	}
}

void TextureSheet::Invalidate(int x, int y, int width, int height)
{
	//==========
	int count;
	int sWidth;		// sample width
	int sHeight;	// sample height
	//==========

	// since the smallest sized texture is 32x32 pixels?
	// we can invalidate 32x32 tiles over the width/height of the area

	sWidth	= width / 32;
	sHeight	= height / 32;
	count	= sWidth * sHeight;

	for (int i = 0; i < count; i++) {

		int index = ToIndex(x + ((i%sWidth) * 32), y + ((i / sWidth) * 32));

		for (int j = 0; j<12; j++) {

			if (m_texMap[j].count(index) > 0) {

				m_texMap[j].erase(index);
			}
		}
	}
}

int	TextureSheet::GetTexFormat(int originalFormat, bool contour)
{
	if (!contour) {
		return originalFormat;	// the same
	}

	switch (originalFormat)
	{
	case 1:
	case 2:
	case 3:
	case 4:
		return originalFormat + 7;		// these formats are identical to 1-4, except they lose the 4 bit alpha part when contour is enabled
	default:
		return originalFormat;
	}
}

void TextureSheet::GetMicrotexPos(int basePage, int id, int& x, int& y)
{
	int xCoords[8] = { 0, 128, 0, 128, 0, 128, 0, 128 };
	int yCoords[8] = { 0, 0, 128, 128, 0, 0, 128, 128 };

	// i'm assuming .. the micro texture map is always on the other memory bank to the base texture
	// this logic works for all our current games
	// the microtextures are always on the top left of each texture sheet

	basePage = (basePage + 1) & 1;	// wrap around base page

	x = xCoords[id];
	y = xCoords[id] + (basePage * 1024);
}

} // New3D
