#include "Texture.h"
#include <stdio.h>
#include <math.h>

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
		printf("-----> deleting %i %i %i %i %i\n", m_format, m_x, m_y, m_width, m_height);
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
	m_mirrorU = false;
	m_mirrorV = false;
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

void Texture::SetWrapMode(bool mirrorU, bool mirrorV)
{
	if (mirrorU != m_mirrorU) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, mirrorU ? GL_MIRRORED_REPEAT : GL_REPEAT);
		m_mirrorU = mirrorU;
	}

	if (mirrorV != m_mirrorV) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, mirrorV ? GL_MIRRORED_REPEAT : GL_REPEAT);
		m_mirrorV = mirrorV;
	}
}

UINT32 Texture::UploadTexture(const UINT16* src, UINT8* scratch, int format, bool mirrorU, bool mirrorV, int x, int y, int width, int height)
{
	int		xi, yi, i;
	GLubyte	texel;
	GLubyte	c, a;

	if (!src || !scratch) {
		return 0;		// sanity checking
	}

	DeleteTexture();	// free any existing texture

	i = 0;

	switch (format)
	{
	default:	// Debug texture
		for (yi = y; yi < (y + height); yi++)
		{
			for (xi = x; xi < (x + width); xi++)
			{
				scratch[i++] = 255;	// R
				scratch[i++] = 0;	// G
				scratch[i++] = 0;	// B
				scratch[i++] = 255;	// A
			}
		}
		break;

	case 0:	// T1RGB5 <- correct
		for (yi = y; yi < (y + height); yi++)
		{
			for (xi = x; xi < (x + width); xi++)
			{
				scratch[i++] = (GLubyte)(((src[yi * 2048 + xi] >> 10) & 0x1F) * 255.f / 0x1F);	// R
				scratch[i++] = (GLubyte)(((src[yi * 2048 + xi] >> 5) & 0x1F) * 255.f / 0x1F);	// G
				scratch[i++] = (GLubyte)(((src[yi * 2048 + xi] >> 0) & 0x1F) * 255.f / 0x1F);	// B
				scratch[i++] = ((src[yi * 2048 + xi] & 0x8000) ? 0 : 255);						// T
			}
		}
		break;

	case 1:	// Interleaved A4L4 (low byte)
		for (yi = y; yi < (y + height); yi++)
		{
			for (xi = x; xi < (x + width); xi++)
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
		for (yi = y; yi < (y + height); yi++)
		{
			for (xi = x; xi < (x + width); xi++)
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
		for (yi = y; yi < (y + height); yi++)
		{
			for (xi = x; xi < (x + width); xi++)
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

		for (yi = y; yi < (y + height); yi++)
		{
			for (xi = x; xi < (x + width); xi++)
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
		for (yi = y; yi < (y + height); yi++)
		{
			for (xi = x; xi < (x + width); xi++)
			{
				texel = src[yi * 2048 + xi] & 0xFF;
				scratch[i++] = texel;
				scratch[i++] = texel;
				scratch[i++] = texel;
				scratch[i++] = (texel==255 ? 0 : 255);
			}
		}
		break;

	case 6:	// 8-bit grayscale <-- this one is correct
		for (yi = y; yi < (y + height); yi++)
		{
			for (xi = x; xi < (x + width); xi++)
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
		for (yi = y; yi < (y + height); yi++)
		{
			for (xi = x; xi < (x + width); xi++)
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
		for (yi = y; yi < (y + height); yi++)
		{
			for (xi = x; xi < (x + width); xi++)
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
		for (yi = y; yi < (y + height); yi++)
		{
			for (xi = x; xi < (x + width); xi++)
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
		for (yi = y; yi < (y + height); yi++)
		{
			for (xi = x; xi < (x + width); xi++)
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

		for (yi = y; yi < (y + height); yi++)
		{
			for (xi = x; xi < (x + width); xi++)
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


	GLfloat maxAnistrophy;

	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnistrophy);

	if (maxAnistrophy > 8) {
		maxAnistrophy = 8.0f;	//anymore than 8 can get expensive for little gain
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);	// rgba is always 4 byte aligned
	glGenTextures(1, &m_textureID);
	glBindTexture(GL_TEXTURE_2D, m_textureID);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, mirrorU ? GL_MIRRORED_REPEAT : GL_REPEAT);	//todo this in shaders?
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, mirrorV ? GL_MIRRORED_REPEAT : GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAnistrophy);
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scratch);

	// assuming successful we can copy details

	m_x = x;
	m_y = y;
	m_width = width;
	m_height = height;
	m_format = format;
	m_mirrorU = mirrorU;
	m_mirrorV = mirrorV;

	printf("create format %i x: %i y: %i width: %i height: %i\n", format, x, y, width, height);

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

} // New3D
