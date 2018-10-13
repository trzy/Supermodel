#include "Texture.h"
#include <stdio.h>
#include <math.h>
#include <algorithm>

namespace New3D {

Texture::Texture()
{
	Reset();
}

Texture::~Texture()
{
	DeleteTexture();		// make sure to have valid context before destroying
}

void Texture::DeleteTexture()
{
	if (m_textureID) {
		glDeleteTextures(1, &m_textureID);
		Reset();
	}
}

void Texture::Reset()
{
	m_x = 0;
	m_y = 0;
	m_width = 0;
	m_height = 0;
	m_format = 0;
	m_textureID = 0;
}

void Texture::BindTexture()
{
	glBindTexture(GL_TEXTURE_2D, m_textureID);
}

void Texture::GetCoordinates(UINT16 uIn, UINT16 vIn, float uvScale, float& uOut, float& vOut)
{
	uOut = (uIn*uvScale) / m_width;
	vOut = (vIn*uvScale) / m_height;
}

void Texture::GetCoordinates(int width, int height, UINT16 uIn, UINT16 vIn, float uvScale, float& uOut, float& vOut)
{
	uOut = (uIn*uvScale) / width;
	vOut = (vIn*uvScale) / height;
}

void Texture::UploadTextureMip(int level, const UINT16* src, UINT8* scratch, int format, int x, int y, int width, int height)
{
	int		xi, yi, i;
	int		subWidth, subHeight;
	GLubyte	texel;
	GLubyte	c, a;
	
	i = 0;

	subWidth = width;
	subHeight = height;

	if (subWidth + x > 2048) {
		subWidth = 2048 - x;
	}

	if (subHeight + y > 2048) {
		subHeight = 2048 - y;
	}

	switch (format)
	{
	default:	// Debug texture
		for (yi = y; yi < (y + subHeight); yi++)
		{
			for (xi = x; xi < (x + subWidth); xi++)
			{
				scratch[i++] = 255;	// R
				scratch[i++] = 0;	// G
				scratch[i++] = 0;	// B
				scratch[i++] = 255;	// A
			}
		}
		break;

	case 0:	// T1RGB5 <- correct
		for (yi = y; yi < (y + subHeight); yi++)
		{
			for (xi = x; xi < (x + subWidth); xi++)
			{
				scratch[i++] = ((src[yi * 2048 + xi] >> 10) & 0x1F) * 255 / 0x1F;	// R
				scratch[i++] = ((src[yi * 2048 + xi] >> 5) & 0x1F) * 255 / 0x1F;	// G
				scratch[i++] = ((src[yi * 2048 + xi] >> 0) & 0x1F) * 255 / 0x1F;	// B
				scratch[i++] = ((src[yi * 2048 + xi] & 0x8000) ? 0 : 255);			// T
			}
		}
		break;

	case 1:	// Interleaved A4L4 (low byte)
		for (yi = y; yi < (y + subHeight); yi++)
		{
			for (xi = x; xi < (x + subWidth); xi++)
			{
				// Interpret as A4L4
				texel = src[yi * 2048 + xi] & 0xFF;
				c = (texel & 0xF) * 17;
				a = (texel >> 4) * 17;
				scratch[i++] = c;
				scratch[i++] = c;
				scratch[i++] = c;
				scratch[i++] = a;
			}
		}
		break;

	case 2:	// luminance alpha texture <- this one is correct
		for (yi = y; yi < (y + subHeight); yi++)
		{
			for (xi = x; xi < (x + subWidth); xi++)
			{
				texel = src[yi * 2048 + xi] & 0xFF;
				c = ((texel >> 4) & 0xF) * 17;
				a = (texel & 0xF) * 17;
				scratch[i++] = c;
				scratch[i++] = c;
				scratch[i++] = c;
				scratch[i++] = a;
			}
		}
		break;

	case 3:	// 8-bit, A4L4 (high byte)
		for (yi = y; yi < (y + subHeight); yi++)
		{
			for (xi = x; xi < (x + subWidth); xi++)
			{
				texel = src[yi * 2048 + xi] >> 8;
				c = (texel & 0xF) * 17;
				a = (texel >> 4) * 17;
				scratch[i++] = c;
				scratch[i++] = c;
				scratch[i++] = c;
				scratch[i++] = a;
			}
		}
		break;

	case 4: // 8-bit, L4A4 (high byte)

		for (yi = y; yi < (y + subHeight); yi++)
		{
			for (xi = x; xi < (x + subWidth); xi++)
			{
				texel = src[yi * 2048 + xi] >> 8;
				c = ((texel >> 4) & 0xF) * 17;
				a = (texel & 0xF) * 17;
				scratch[i++] = c;
				scratch[i++] = c;
				scratch[i++] = c;
				scratch[i++] = a;
			}
		}
		break;

	case 5:	// 8-bit grayscale
		for (yi = y; yi < (y + subHeight); yi++)
		{
			for (xi = x; xi < (x + subWidth); xi++)
			{
				texel = src[yi * 2048 + xi] & 0xFF;
				scratch[i++] = texel;
				scratch[i++] = texel;
				scratch[i++] = texel;
				scratch[i++] = (texel == 255 ? 0 : 255);
			}
		}
		break;

	case 6:	// 8-bit grayscale <-- this one is correct
		for (yi = y; yi < (y + subHeight); yi++)
		{
			for (xi = x; xi < (x + subWidth); xi++)
			{
				texel = src[yi * 2048 + xi] >> 8;
				scratch[i++] = texel;
				scratch[i++] = texel;
				scratch[i++] = texel;
				scratch[i++] = (texel == 255 ? 0 : 255);
			}
		}
		break;

	case 7:	// RGBA4
		for (yi = y; yi < (y + subHeight); yi++)
		{
			for (xi = x; xi < (x + subWidth); xi++)
			{
				scratch[i++] = ((src[yi * 2048 + xi] >> 12) & 0xF) * 17;// R
				scratch[i++] = ((src[yi * 2048 + xi] >> 8) & 0xF) * 17;	// G
				scratch[i++] = ((src[yi * 2048 + xi] >> 4) & 0xF) * 17;	// B
				scratch[i++] = ((src[yi * 2048 + xi] >> 0) & 0xF) * 17;	// A
			}
		}
		break;

		//
		// 4 bit texture types - all luminance textures (no alpha), only seem to be enabled when contour is enabled ( white = contour value )
		//

	case 8: // low byte, low nibble
		for (yi = y; yi < (y + subHeight); yi++)
		{
			for (xi = x; xi < (x + subWidth); xi++)
			{
				texel = src[yi * 2048 + xi] & 0xFF;
				c = (texel & 0xF) * 17;
				scratch[i++] = c;
				scratch[i++] = c;
				scratch[i++] = c;
				scratch[i++] = (c == 255 ? 0 : 255);
			}
		}
		break;

	case 9:	// low byte, high nibble
		for (yi = y; yi < (y + subHeight); yi++)
		{
			for (xi = x; xi < (x + subWidth); xi++)
			{
				texel = src[yi * 2048 + xi] & 0xFF;
				c = ((texel >> 4) & 0xF) * 17;
				scratch[i++] = c;
				scratch[i++] = c;
				scratch[i++] = c;
				scratch[i++] = (c == 255 ? 0 : 255);
			}
		}
		break;

	case 10:	// high byte, low nibble
		for (yi = y; yi < (y + subHeight); yi++)
		{
			for (xi = x; xi < (x + subWidth); xi++)
			{
				texel = src[yi * 2048 + xi] >> 8;
				c = (texel & 0xF) * 17;
				scratch[i++] = c;
				scratch[i++] = c;
				scratch[i++] = c;
				scratch[i++] = (c == 255 ? 0 : 255);
			}
		}
		break;

	case 11: // high byte, high nibble

		for (yi = y; yi < (y + subHeight); yi++)
		{
			for (xi = x; xi < (x + subWidth); xi++)
			{
				texel = src[yi * 2048 + xi] >> 8;
				c = ((texel >> 4) & 0xF) * 17;
				scratch[i++] = c;
				scratch[i++] = c;
				scratch[i++] = c;
				scratch[i++] = (c == 255 ? 0 : 255);
			}
		}
		break;
	}

	glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexSubImage2D(GL_TEXTURE_2D, level, 0, 0, subWidth, subHeight, GL_RGBA, GL_UNSIGNED_BYTE, scratch);
}

UINT32 Texture::UploadTexture(const UINT16* src, UINT8* scratch, int format, int x, int y, int width, int height)
{
	const int mipXBase[] = { 0, 1024, 1536, 1792, 1920, 1984, 2016, 2032, 2040, 2044, 2046, 2047 };
	const int mipYBase[] = { 0, 512, 768, 896, 960, 992, 1008, 1016, 1020, 1022, 1023 };
	const int mipDivisor[] = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024 };

	if (!src || !scratch) {
		return 0;		// sanity checking
	}

	DeleteTexture();	// free any existing texture
	CreateTextureObject(format, x, y, width, height);

	int page = y / 1024;

	y -= (page * 1024);	// remove page from tex y

	for (int i = 0; width > 0 && height > 0; i++) {

		int xPos = mipXBase[i] + (x / mipDivisor[i]);
		int yPos = mipYBase[i] + (y / mipDivisor[i]);

		UploadTextureMip(i, src, scratch, format, xPos, yPos + (page * 1024), width, height);

		width /= 2;
		height /= 2;
	}

	return m_textureID;
}

void Texture::GetDetails(int& x, int&y, int& width, int& height, int& format)
{
	x = m_x;
	y = m_y;
	width = m_width;
	height = m_height;
	format = m_format;
}

bool Texture::Compare(int x, int y, int width, int height, int format)
{
	if (m_x == x && m_y == y && m_width == width && m_height == height && m_format == format) {
		return true;
	}

	return false;
}

bool Texture::CheckMapPos(int ax1, int ax2, int ay1, int ay2)
{
	int bx1 = m_x;
	int bx2 = m_x + m_width;
	int by1 = m_y;
	int by2 = m_y + m_height;

	if (ax1<bx2 && ax2>bx1 &&
		ay1<by2 && ay2>by1) {
		return true;			// rectangles overlap
	}

	return false;
}

void Texture::CreateTextureObject(int format, int x, int y, int width, int height)
{
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);	// rgba is always 4 byte aligned
	glGenTextures(1, &m_textureID);
	glBindTexture(GL_TEXTURE_2D, m_textureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);

	int minD = std::min(width, height);
	int count = 0;

	while (minD > 1) {
		minD /= 2;
		count++;
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, count);

	m_x = x;
	m_y = y;
	m_width = width;
	m_height = height;
	m_format = format;
}

} // New3D
