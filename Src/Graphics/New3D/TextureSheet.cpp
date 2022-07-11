#include "TextureSheet.h"

namespace New3D {

TextureSheet::TextureSheet()
{
	m_temp.resize(1024 * 1024 * 4);	// temporary buffer for textures
}

int TextureSheet::ToIndex(int x, int y)
{
	return (y * 2048) + x;
}

std::shared_ptr<Texture> TextureSheet::BindTexture(const UINT16* src, int format, int x, int y, int width, int height)
{
	//========
	int	index;
	//========

	x &= 2047;
	y &= 2047;

	if (width > 1024 || height > 1024) {	// sanity checking
		return nullptr;
	}

	index = ToIndex(x, y);

	auto range = m_texMap.equal_range(index);

	if (range.first == range.second) {	

		// nothing found so create a new texture 

		std::shared_ptr<Texture> t(new Texture());
		m_texMap.insert(std::pair<int, std::shared_ptr<Texture>>(index, t));
		t->UploadTexture(src, m_temp.data(), format, x, y, width, height);
		return t;
	}
	else {

		// iterate to try and find a match

		for (auto it = range.first; it != range.second; ++it) {

			int x2, y2, width2, height2, format2;

			it->second->GetDetails(x2, y2, width2, height2, format2);

			if (width == width2 && height == height2 && format == format2) {
				return it->second;
			}
		}

		// nothing found so create a new entry

		std::shared_ptr<Texture> t(new Texture());
		m_texMap.insert(std::pair<int, std::shared_ptr<Texture>>(index, t));
		t->UploadTexture(src, m_temp.data(), format, x, y, width, height);
		return t;
	}
}

void TextureSheet::Release()
{
	m_texMap.clear();
}

void TextureSheet::Invalidate(int x, int y, int width, int height)
{
	//============
	int newX;
	int newY;
	int newWidth;
	int newHeight;
	int count;
	int sWidth;		// sample width
	int sHeight;	// sample height
	//============

	if (width <= 512) {
		newX = (x + width) - 512;
		newWidth = 512;
	}
	else {
		newX = x;
		newWidth = width;
	}
	
	if (height <= 512) {
		newY = (y + height) - 512;
		newHeight = 512;
	}
	else {
		newY = y;
		newHeight = height;
	}

	CropTile(x, y, newX, newY, newWidth, newHeight);

	sWidth	= newWidth / 32;
	sHeight = newHeight / 32;
	count	= sWidth * sHeight;

	for (int i = 0; i < count; i++) {

		int posX	= newX + ((i%sWidth) * 32);
		int posY	= newY + ((i / sWidth) * 32);
		int index	= ToIndex(posX, posY);

		if (posX >= x && posY >= y) {				// invalidate this area of memory
			m_texMap.erase(index);
		}
		else {										// check for overlapping data tiles and invalidate as necessary

			auto range = m_texMap.equal_range(index);

			for (auto it = range.first; it != range.second; ++it) {

				if (it->second->CheckMapPos(x, x + width, y, y + height)) {
					m_texMap.erase(index);
					break;
				}
			}
		}
	}
}

void TextureSheet::CropTile(int oldX, int oldY, int &newX, int &newY, int &newWidth, int &newHeight)
{
	if (newX < 0) {
		newWidth += newX;
		newX = 0;
	}

	if (newY < 0) {
		newHeight += newY;
		newY = 0;
	}

	if (oldY >= 1024 && newY < 1024) {		// gone into next memory page, limitation of our flat model
		newHeight -= 1024 - newY;
		newY = 1024;
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
	int xCoords[8] = { 0, 0, 128, 128, 0, 0, 128, 128 };
	int yCoords[8] = { 0, 128, 0, 128, 256, 384, 256, 384 };

	// i'm assuming .. the micro texture map is always on the other memory bank to the base texture
	// this logic works for all our current games
	// the microtextures are always on the top left of each texture sheet

	basePage = (basePage + 1) & 1;	// wrap around base page

	x = xCoords[id];
	y = yCoords[id] + (basePage * 1024);
}

} // New3D
