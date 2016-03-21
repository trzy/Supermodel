#include "TextureSheet.h"

namespace New3D {

TextureSheet::TextureSheet()
{
	m_temp.resize(512 * 512 * 4);	// temporay buffer for textures
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
		return 0;
	}

	if (width > 512 || height > 512) {
		return 0;
	}

	index = ToIndex(x, y);

	if (m_texMap[format&TEXTURE_DEBUG_MASK].count(index) == 0) {

		//no textures at this position or format so add it to the map

		std::shared_ptr<Texture> t(new Texture());
		m_texMap[format&TEXTURE_DEBUG_MASK].insert(std::pair<int, std::shared_ptr<Texture>>(index, t));
		t->UploadTexture(src, m_temp.data(), format, mirrorU, mirrorV, x, y, width, height);
		return t;
	}
	else {
		//scan for duplicates
		//only texture width/height and wrap modes can change here. Since key is based on x/y pos, and each map is a separate format

		auto range = m_texMap[format&TEXTURE_DEBUG_MASK].equal_range(index);

		for (auto it = range.first; it != range.second; ++it) {

			int x2, y2, width2, height2, format2;

			it->second->GetDetails(x2, y2, width2, height2, format2);

			if (width == width2 && height == height2) {
				return it->second;
			}
		}

		std::shared_ptr<Texture> t(new Texture());
		m_texMap[format&TEXTURE_DEBUG_MASK].insert(std::pair<int, std::shared_ptr<Texture>>(index, t));
		t->UploadTexture(src, m_temp.data(), format, mirrorU, mirrorV, x, y, width, height);

		return t;
	}
}

void TextureSheet::Release()
{
	for (int i = 0; i < 8; i++) {
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

		for (int j = 0; j<8; j++) {

			if (m_texMap[j].count(index) > 0) {

				m_texMap[j].erase(index);
			}
		}
	}
}

} // New3D
