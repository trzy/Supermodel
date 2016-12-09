#ifndef _TEXTURE_H_
#define _TEXTURE_H_

#include "Types.h"
#include "Pkgs/glew.h"	//arg

namespace New3D {
  
#define TEXTURE_DEBUG		0x8
#define TEXTURE_DEBUG_MASK	0x7

class Texture
{
public:

	Texture();
	~Texture();

	UINT32	UploadTexture	(const UINT16* src, UINT8* scratch, int format, bool mirrorU, bool mirrorV, int x, int y, int width, int height);
	void	DeleteTexture	();
	void	BindTexture		();
	void	GetCoordinates	(UINT16 uIn, UINT16 vIn, float uvScale, float& uOut, float& vOut);
	void	GetDetails		(int& x, int&y, int& width, int& height, int& format);
	void	SetWrapMode		(bool mirrorU, bool mirrorV);

	static void GetCoordinates(bool mirror, int width, int height, UINT16 uIn, UINT16 vIn, float uvScale, float& uOut, float& vOut);

private:

	void Reset();

	int m_x;
	int m_y;
	int m_width;
	int m_height;
	int m_format;
	bool m_mirrorU;
	bool m_mirrorV;
	GLuint m_textureID;
};

} // New3D

#endif
