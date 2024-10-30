#pragma once

#ifndef _TEXTUREBANK_H_
#define _TEXTUREBANK_H_

#include "Types.h"
#include <GL/glew.h>

// texture banks are a fixed size
// 2048x1024 pixels, each pixel is 16bits in size

namespace New3D {

	class TextureBank
	{
	public:
		TextureBank();
		~TextureBank();

		void AttachMemory(const UINT16* textureRam);
		void Bind();
		void UploadTextures(int level, int x, int y, int width, int height);
		int GetNumberOfLevels() const;

	private:
		const UINT16* m_textureRam = nullptr;
		GLuint m_texID = 0;
		int m_numLevels = 0;
	};

}

#endif
