/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011-2012 Bart Trzynadlowski, Nik Henson 
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
 
/*
 * Shaders2D.h
 * 
 * Header file containing the 2D vertex and fragment shaders.
 */

#ifndef INCLUDED_SHADERS2D_H
#define INCLUDED_SHADERS2D_H

// Vertex shader
static const char s_vertexShaderSource[] = R"glsl(

	#version 410 core

	// outputs
	out vec2 fsTexCoord;

	void main(void)
	{
		const vec4 vertices[] = vec4[](vec4(-1.0, -1.0, 0.0, 1.0),
										vec4(-1.0,  1.0, 0.0, 1.0),
										vec4( 1.0, -1.0, 0.0, 1.0),
										vec4( 1.0,  1.0, 0.0, 1.0));

		fsTexCoord = ((vertices[gl_VertexID % 4].xy + 1.0) / 2.0);
		fsTexCoord.y = 1.0 - fsTexCoord.y;							// flip upside down
		gl_Position = vertices[gl_VertexID % 4];	
	}

	)glsl";


// Fragment shader
static const char s_fragmentShaderSource[] = R"glsl(

	#version 410 core

	// inputs
	uniform sampler2D tex1;			// texture
	in vec2 fsTexCoord;

	// outputs
	out vec4 fragColor;

	void main()
	{
		fragColor = texture(tex1, fsTexCoord);
	}

	)glsl";


// Vertex shader
static const char s_vertexShaderTileGen[] = R"glsl(

	#version 410 core

	uniform float lineStart;		// defined as a % of the viewport height in the range 0-1. So 0 is top line, 0.5 is line 192 etc
	uniform float lineEnd;

	void main(void)
	{
		const float v1 = -1.0;
		const float v2 =  1.0;

		vec4 vertices[] = vec4[]( vec4(-1.0, v1, 0.0, 1.0),
								  vec4(-1.0, v2, 0.0, 1.0),
								  vec4( 1.0, v1, 0.0, 1.0),
								  vec4( 1.0, v2, 0.0, 1.0));

		float top		= ((v2 - v1) * lineStart) + v1;
		float bottom	= ((v2 - v1) * lineEnd  ) + v1;

		vertices[0].y = top;
		vertices[2].y = top;
		vertices[1].y = bottom;
		vertices[3].y = bottom;

		gl_Position = vertices[gl_VertexID % 4];	
	}

	)glsl";

// Fragment shader
static const char s_fragmentShaderTileGen[] = R"glsl(

	#version 410 core

	//layout(origin_upper_left) in vec4 gl_FragCoord;

	// inputs
	uniform usampler2D vram;			// texture 512x512
	uniform usampler2D palette;			// texture 128x256	- actual dimensions dont matter too much but we have to stay in the limits of max tex width/height, so can't have 1 giant 1d array
	uniform uint regs[32];
	uniform int layerNumber;

	// outputs
	out vec4 fragColor;

	ivec2 GetVRamCoords(int offset)
	{
		return ivec2(offset % 512, offset / 512);
	}

	ivec2 GetPaletteCoords(int offset)
	{
		return ivec2(offset % 128, offset / 128);
	}

	uint GetLineMask(int layerNum, int yCoord)
	{
		uint shift			= (layerNum<2) ? 16u : 0u;									// need to check this, we could be endian swapped so could be wrong
		uint maskPolarity	= ((layerNum & 1) > 0) ? 0xFFFFu : 0x0000u;
		int index			= (0xF7000 / 4) + yCoord;

		ivec2 coords		= GetVRamCoords(index);
		uint mask			= ((texelFetch(vram,coords,0).r >> shift) & 0xFFFFu) ^ maskPolarity;

		return mask;
	}

	bool GetPixelMask(int layerNum, int xCoord, int yCoord)
	{
		uint lineMask = GetLineMask(layerNum, yCoord);
		uint maskTest = 1 << (15-(xCoord/32));

		return (lineMask & maskTest) != 0;
	}

	int GetLineScrollValue(int layerNum, int yCoord)
	{
		int index = ((0xF6000 + (layerNum * 0x400)) / 4) + (yCoord / 2);
		int shift = (1 - (yCoord % 2)) * 16;

		ivec2 coords = GetVRamCoords(index);
		return int((texelFetch(vram,coords,0).r >> shift) & 0xFFFFu);
	}

	int GetTileNumber(int xCoord, int yCoord, int xScroll, int yScroll)
	{
		int xIndex = ((xCoord + xScroll) / 8) & 0x3F;
		int yIndex = ((yCoord + yScroll) / 8) & 0x3F;
		
		return (yIndex*64) + xIndex;
	}

	int GetTileData(int layerNum, int tileNumber)
	{
		int addressBase = (0xF8000 + (layerNum * 0x2000)) / 4;
		int offset = tileNumber / 2;							// two tiles per 32bit word
		int shift = (1 - (tileNumber % 2)) * 16;				// triple check this

		ivec2 coords = GetVRamCoords(addressBase+offset);
		uint data = (texelFetch(vram,coords,0).r >> shift) & 0xFFFFu;

		return int(data);
	}

	int GetVFine(int yCoord, int yScroll)
	{
		return (yCoord + yScroll) & 7;
	}

	int GetHFine(int xCoord, int xScroll)
	{
		return (xCoord + xScroll) & 7;
	}

	// register data
	bool LineScrollMode		(int layerNum)	{ return (regs[0x60/4 + layerNum] & 0x8000u) != 0; }
	int  GetHorizontalScroll	(int layerNum)	{ return int(regs[0x60/4 + layerNum] & 0x3FFu); }
	int  GetVerticalScroll		(int layerNum)	{ return int((regs[0x60/4 + layerNum] >> 16) & 0x1FFu); }
	int  LayerPriority		()		{ return int((regs[0x20/4] >> 8) & 0xFu); }
	bool LayerIs4Bit		(int layerNum)	{ return (regs[0x20/4] & uint(1 << (12 + layerNum))) != 0; }
	bool LayerEnabled		(int layerNum)	{ return (regs[0x60/4 + layerNum] & 0x80000000u) != 0; }
	bool LayerSelected		(int layerNum)	{ return (LayerPriority() & (1 << layerNum)) == 0; }

	float Int8ToFloat(uint c)
	{
		if((c & 0x80u) > 0u) {		// this is a bit harder in GLSL. Top bit means negative number, we extend to make 32bit
			return float(int(c | 0xFFFFFF00u)) / 128.0;
		}
		else {
			return float(c) / 127.0;
		}
	}

	vec4 AddColourOffset(int layerNum, vec4 colour)  
	{ 
		uint offsetReg = regs[(0x40/4) + layerNum/2];

		vec4 c;
		c.b = Int8ToFloat((offsetReg >>16) & 0xFFu);
		c.g = Int8ToFloat((offsetReg >> 8) & 0xFFu);
		c.r = Int8ToFloat((offsetReg >> 0) & 0xFFu);
		c.a = 0.0;

		colour += c;
		return clamp(colour,0.0,1.0);		// clamp is probably not needed since will get clamped on render target
	}

	vec4 Int16ColourToVec4(uint colour)
	{
		uint alpha = (colour>>15);		// top bit is alpha. 1 means clear, 0 opaque
		alpha = ~alpha;					// invert
		alpha = alpha & 0x1u;			// mask bit
		const uint mask = 0x1F;
		
		vec4 c;
		c.r = float((colour >> 0 ) & mask) / 31.0;
		c.g = float((colour >> 5 ) & mask) / 31.0;
		c.b = float((colour >> 10) & mask) / 31.0;
		c.a = float(alpha) / 1.0;

		c.rgb *= c.a;		// multiply by alpha value, this will push transparent to black, no branch needed
		
		return c;
	}

	vec4 GetColour(int layerNum, int paletteOffset)
	{
		ivec2 coords = GetPaletteCoords(paletteOffset);
		uint colour = texelFetch(palette,coords,0).r;

		vec4 col = Int16ColourToVec4(colour);			// each colour is only 16bits, but occupies 32bits

		return AddColourOffset(layerNum,col);			// apply colour offsets from registers	
	}

	vec4 Draw4Bit(int layerNum, int tileData, int hFine, int vFine)
	{		
		// Tile pattern offset: each tile occupies 32 bytes when using 4-bit pixels (offset of tile pattern within VRAM)
		int patternOffset = ((tileData & 0x3FFF) << 1) | ((tileData >> 15) & 1);
		patternOffset *= 32;
		patternOffset /= 4;

		// Upper color bits; the lower 4 bits come from the tile pattern
		int paletteIndex = tileData & 0x7FF0;

		ivec2 coords = GetVRamCoords(patternOffset+vFine);
		uint pattern = texelFetch(vram,coords,0).r;
		pattern = (pattern >> ((7-hFine)*4)) & 0xFu;			// get the pattern for our horizontal value

		return GetColour(layerNum, paletteIndex | int(pattern));
	}

	vec4 Draw8Bit(int layerNum, int tileData, int hFine, int vFine)
	{
		// Tile pattern offset: each tile occupies 64 bytes when using 8-bit pixels
		int patternOffset = tileData & 0x3FFF;
		patternOffset *= 64;
		patternOffset /= 4;

		// Upper color bits
		int paletteIndex = tileData & 0x7F00;

		// each read is 4 pixels
		int offset = hFine / 4;

		ivec2 coords = GetVRamCoords(patternOffset+(vFine*2)+offset);		// 8-bit pixels, each line is two words
		uint pattern = texelFetch(vram,coords,0).r;

		pattern = (pattern >> ((3-(hFine%4))*8)) & 0xFFu;					// shift out the bits we want for this pixel

		return GetColour(layerNum, paletteIndex | int(pattern));
	}
	
	void main()
	{
		ivec2 pos = ivec2(gl_FragCoord.xy);

		int scrollX;
		if(LineScrollMode(layerNumber)) {
			scrollX = GetLineScrollValue(layerNumber, pos.y);
		}
		else {
			scrollX = GetHorizontalScroll(layerNumber);
		}

		int scrollY		= GetVerticalScroll(layerNumber);
		int tileNumber	= GetTileNumber(pos.x,pos.y,scrollX,scrollY);
		int hFine		= GetHFine(pos.x,scrollX);
		int vFine		= GetVFine(pos.y,scrollY);
		bool pixelMask	= GetPixelMask(layerNumber,pos.x,pos.y);

		if(pixelMask==true) {

			int tileData = GetTileData(layerNumber,tileNumber);

			if(LayerIs4Bit(layerNumber)) {
				fragColor = Draw4Bit(layerNumber,tileData,hFine,vFine);
			}
			else {
				fragColor = Draw8Bit(layerNumber,tileData,hFine,vFine);
			}
		}
		else {
			fragColor = vec4(0.0);
		}
	}

	)glsl";


#endif	// INCLUDED_SHADERS2D_H
