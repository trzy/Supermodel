/**
 ** Supermodel
 ** A Sega Model Series Arcade Emulator.
 **
 ** This file is part of Supermodel.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **
 ** Supermodel is distributed in the hope that it will be useful, but WITHOUT
 ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 ** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 ** more details.
 **
 ** You should have received a copy of the GNU General Public License along
 ** with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
 **/

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
