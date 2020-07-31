#ifndef _TEXTURE_H_
#define _TEXTURE_H_

#include "Types.h"
#include <GL/glew.h>

namespace New3D {
  
class Texture
{
public:

	Texture();
	~Texture();

	UINT32	UploadTexture	(const UINT16* src, UINT8* scratch, int format, int x, int y, int width, int height);
	void	DeleteTexture	();
	void	BindTexture		();
	void	GetCoordinates	(UINT16 uIn, UINT16 vIn, float uvScale, float& uOut, float& vOut);
	void	GetDetails		(int& x, int&y, int& width, int& height, int& format);
	bool	Compare			(int x, int y, int width, int height, int format);
	bool	CheckMapPos		(int ax1, int ax2, int ay1, int ay2);				//check to see if textures overlap

	static void GetCoordinates(int width, int height, UINT16 uIn, UINT16 vIn, float uvScale, float& uOut, float& vOut);

private:

	void CreateTextureObject(int format, int x, int y, int width, int height);
	void UploadTextureMip(int level, const UINT16* src, UINT8* scratch, int format, int x, int y, int width, int height);
	void Reset();

	int m_x;
	int m_y;
	int m_width;
	int m_height;
	int m_format;
	GLuint m_textureID;
};

} // New3D

#endif
