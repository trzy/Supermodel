#include "TextureBank.h"

New3D::TextureBank::TextureBank()
{
	glGenTextures(1, &m_texID);
	glBindTexture(GL_TEXTURE_2D, m_texID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);

	int width = 2048;
	int height = 1024;
	int level = 0;

	while (width>=1 && height>=1) {

		glTexImage2D(GL_TEXTURE_2D, level, GL_R16UI, width, height, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, nullptr);	// allocate storage

		width	= (width > 1) ? width / 2 : 1;
		height	= (height > 1) ? height / 2 : 1;

		level++;

		if (width == 1 && height == 1) {
			glTexImage2D(GL_TEXTURE_2D, level, GL_R16UI, width, height, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, nullptr);	// allocate storage
			break;
		}
	}

	m_numLevels = level;
}

New3D::TextureBank::~TextureBank()
{
	if (m_texID) {
		glDeleteTextures(1, &m_texID);
		m_texID = 0;
	}
}

void New3D::TextureBank::AttachMemory(const UINT16* textureRam)
{
	m_textureRam = textureRam;
}

void New3D::TextureBank::Bind()
{
	glBindTexture(GL_TEXTURE_2D, m_texID);
}

void New3D::TextureBank::UploadTextures(int level, int x, int y, int width, int height)
{
	glBindTexture(GL_TEXTURE_2D, m_texID);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 2);

	static constexpr int mipXBase[] = { 0, 1024, 1536, 1792, 1920, 1984, 2016, 2032, 2040, 2044, 2046, 2047 };
	static constexpr int mipYBase[] = { 0, 512, 768, 896, 960, 992, 1008, 1016, 1020, 1022, 1023 };

	int subX = x - mipXBase[level];
	int subY = y - mipYBase[level];

	for (int i = 0; i < height; i++) {
		glTexSubImage2D(GL_TEXTURE_2D, level, subX, subY + i, width, 1, GL_RED_INTEGER, GL_UNSIGNED_SHORT, m_textureRam + ((y + i) * 2048) + x);
	}
}

int New3D::TextureBank::GetNumberOfLevels() const
{
	return m_numLevels;
}
