#pragma once

// I altered this code a bit to make sure it always compiles with gl 4.1. Version 4.5 allows you to specify arrays differently.
// Ripped out most of the common code, people have been pushing changes to the shaders but we are ending up with diverging implementations
// between triangle / quad version which is less than ideal.

static const char* fragmentShaderR3DCommon = R"glsl(

#define LayerColour 0x0
#define LayerTrans0 0x1
#define LayerTrans1 0x2

vec4 ExtractColour(int type, uint value)
{
	vec4 c = vec4(0.0);

	if(type==0) {			// T1RGB5	
		c.r		= float((value >> 10) & 0x1Fu);
		c.g		= float((value >> 5 ) & 0x1Fu);
		c.b		= float((value      ) & 0x1Fu);
		c.rgb  *= (1.0/31.0);
		c.a		= 1.0 - float((value >> 15) & 0x1u);
	}
	else if(type==1) {		// Interleaved A4L4 (low byte)
		c.rgb	= vec3(float(value&0xFu));
		c.a		= float((value >> 4) & 0xFu);
		c      *= (1.0/15.0);
	}
	else if(type==2) {
		c.a		= float(value&0xFu);
		c.rgb   = vec3(float((value >> 4) & 0xFu));
		c	   *= (1.0/15.0);
	}
	else if(type==3) {
		c.rgb	= vec3(float((value>>8)&0xFu));
		c.a		= float((value >> 12) & 0xFu);
		c	   *= (1.0/15.0);
	}
	else if(type==4) {
		c.a		= float((value>>8)&0xFu);
		c.rgb   = vec3(float((value >> 12) & 0xFu));
		c	   *= (1.0/15.0);
	}
	else if(type==5) {
		c = vec4(float(value&0xFFu) / 255.0);
		if(c.a==1.0)	{ c.a = 0.0; }
		else			{ c.a = 1.0; }
	}
	else if(type==6) {
		c = vec4(float((value>>8)&0xFFu) / 255.0);
		if(c.a==1.0)	{ c.a = 0.0; }
		else			{ c.a = 1.0; }
	}
	else if(type==7) {	// RGBA4
		c.r = float((value>>12)&0xFu);
		c.g = float((value>> 8)&0xFu);
		c.b = float((value>> 4)&0xFu);
		c.a = float((value>> 0)&0xFu);
		c *= (1.0/15.0);
	}
	else if(type==8) {	// low byte, low nibble
		c = vec4(float(value&0xFu) / 15.0);
		if(c.a==1.0)	{ c.a = 0.0; }
		else			{ c.a = 1.0; }
	}
	else if(type==9) {	// low byte, high nibble
		c = vec4(float((value>>4)&0xFu) / 15.0);
		if(c.a==1.0)	{ c.a = 0.0; }
		else			{ c.a = 1.0; }
	}
	else if(type==10) {	// high byte, low nibble
		c = vec4(float((value>>8)&0xFu) / 15.0);
		if(c.a==1.0)	{ c.a = 0.0; }
		else			{ c.a = 1.0; }
	}
	else if(type==11) {	// high byte, high nibble
		c = vec4(float((value>>12)&0xFu) / 15.0);
		if(c.a==1.0)	{ c.a = 0.0; }
		else			{ c.a = 1.0; }
	}

	return c;
}

int GetPage(int yCoord)
{
	return yCoord / 1024;
}

int GetNextPage(int yCoord)
{
	return (GetPage(yCoord) + 1) & 1;
}

int GetNextPageOffset(int yCoord)
{
	return GetNextPage(yCoord) * 1024;
}

// wrapping tex coords would be super easy but we combined tex sheets so have to handle wrap around between sheets
// hardware testing would be useful because i don't know exactly what happens if you try to read outside the texture sheet
// wrap around is a good guess
ivec2 WrapTexCoords(ivec2 pos, ivec2 coordinate)
{
	ivec2 newCoord;

	newCoord.x = coordinate.x & 2047;
	newCoord.y = coordinate.y;

	int page = GetPage(pos.y);

	newCoord.y -= (page * 1024);	// remove page
	newCoord.y &= 1023;				// wrap around in the same sheet
	newCoord.y += (page * 1024);	// add page back

	return newCoord;
}

ivec2 GetTextureSize(int level, ivec2 size)
{
	int mipDivisor = 1 << level;

	return size / mipDivisor;
}

ivec2 GetTexturePosition(int level, ivec2 pos)
{
	const int mipXBase[] = int[](0, 1024, 1536, 1792, 1920, 1984, 2016, 2032, 2040, 2044, 2046, 2047);
	const int mipYBase[] = int[](0, 512, 768, 896, 960, 992, 1008, 1016, 1020, 1022, 1023);

	int mipDivisor = 1 << level;

	int page = pos.y / 1024;
	pos.y -= (page * 1024);		// remove page from tex y

	ivec2 retPos;
	retPos.x = mipXBase[level] + (pos.x / mipDivisor);
	retPos.y = mipYBase[level] + (pos.y / mipDivisor);

	retPos.y += (page * 1024);	// add page back to tex y

	return retPos;
}

ivec2 GetMicroTexturePos(int id)
{
	const int xCoords[8] = int[](0, 0, 128, 128, 0, 0, 128, 128);
	const int yCoords[8] = int[](0, 128, 0, 128, 256, 384, 256, 384);

	return ivec2(xCoords[id],yCoords[id]);
}

float mip_map_level(in vec2 texture_coordinate) // in texel units
{
    vec2  dx_vtc        = dFdx(texture_coordinate);
    vec2  dy_vtc        = dFdy(texture_coordinate);
    float delta_max_sqr = max(dot(dx_vtc, dx_vtc), dot(dy_vtc, dy_vtc));
    float mml = 0.5 * log2(delta_max_sqr);
    return max( 0.0, mml );
}

float LinearTexLocations(int wrapMode, float size, float u, out float u0, out float u1)
{
	float texelSize		= 1.0 / size;
	float halfTexelSize	= 0.5 / size;

	if(wrapMode==0) {							// repeat
		u	= u * size - 0.5;
		u0	= (floor(u) + 0.5) / size;			// + 0.5 offset added to push us into the centre of a pixel, without we'll get rounding errors
		u0	= fract(u0);
		u1	= u0 + texelSize;
		u1	= fract(u1);

		return fract(u);						// return weight
	}
	else if(wrapMode==1) {						// repeat + clamp
		u	= fract(u);							// must force into 0-1 to start
		u	= u * size - 0.5;
		u0	= (floor(u) + 0.5) / size;			// + 0.5 offset added to push us into the centre of a pixel, without we'll get rounding errors
		u1	= u0 + texelSize;

		if(u0 <  0.0)	u0 = 0.0;
		if(u1 >= 1.0)	u1 = 1.0 - halfTexelSize;
		
		return fract(u);						// return weight
	}
	else {										// mirror + mirror clamp - both are the same since the edge pixels are repeated anyway

		float odd = floor(mod(u, 2.0));			// odd values are mirrored

		if(odd > 0.0) {
			u = 1.0 - fract(u);
		}
		else {
			u = fract(u);
		}

		u	= u * size - 0.5;
		u0	= (floor(u) + 0.5) / size;			// + 0.5 offset added to push us into the centre of a pixel, without we'll get rounding errors
		u1	= u0 + texelSize;

		if(u0 <  0.0)	u0 = 0.0;
		if(u1 >= 1.0)	u1 = 1.0 - halfTexelSize;
		
		return fract(u);						// return weight
	}
}

vec4 texBiLinear(usampler2D texSampler, ivec2 wrapMode, vec2 texSize, ivec2 texPos, vec2 texCoord)
{
	float tx[2], ty[2];
	float a = LinearTexLocations(wrapMode.s, texSize.x, texCoord.x, tx[0], tx[1]);
	float b = LinearTexLocations(wrapMode.t, texSize.y, texCoord.y, ty[0], ty[1]);

	vec4 p0q0 = ExtractColour(baseTexType,texelFetch(texSampler, WrapTexCoords(texPos,ivec2(vec2(tx[0],ty[0]) * texSize + texPos)), 0).r);
    vec4 p1q0 = ExtractColour(baseTexType,texelFetch(texSampler, WrapTexCoords(texPos,ivec2(vec2(tx[1],ty[0]) * texSize + texPos)), 0).r);
    vec4 p0q1 = ExtractColour(baseTexType,texelFetch(texSampler, WrapTexCoords(texPos,ivec2(vec2(tx[0],ty[1]) * texSize + texPos)), 0).r);
    vec4 p1q1 = ExtractColour(baseTexType,texelFetch(texSampler, WrapTexCoords(texPos,ivec2(vec2(tx[1],ty[1]) * texSize + texPos)), 0).r);

	if(alphaTest) {
		if(p0q0.a > p1q0.a)		{ p1q0.rgb = p0q0.rgb; }
		if(p0q0.a > p0q1.a)		{ p0q1.rgb = p0q0.rgb; }

		if(p1q0.a > p0q0.a)		{ p0q0.rgb = p1q0.rgb; }
		if(p1q0.a > p1q1.a)		{ p1q1.rgb = p1q0.rgb; }

		if(p0q1.a > p0q0.a)		{ p0q0.rgb = p0q1.rgb; }
		if(p0q1.a > p1q1.a)		{ p1q1.rgb = p0q1.rgb; }

		if(p1q1.a > p0q1.a)		{ p0q1.rgb = p1q1.rgb; }
		if(p1q1.a > p1q0.a)		{ p1q0.rgb = p1q1.rgb; }
	}

	// Interpolation in X direction.
    vec4 pInterp_q0 = mix( p0q0, p1q0, a ); // Interpolates top row in X direction.
    vec4 pInterp_q1 = mix( p0q1, p1q1, a ); // Interpolates bottom row in X direction.

    return mix( pInterp_q0, pInterp_q1, b ); // Interpolate in Y direction.
}

vec4 textureR3D(usampler2D texSampler, ivec2 wrapMode, ivec2 texSize, ivec2 texPos, vec2 texCoord)
{
	float numLevels	= floor(log2(min(float(texSize.x), float(texSize.y))));				// r3d only generates down to 1:1 for square textures, otherwise its the min dimension
	float fLevel	= min(mip_map_level(texCoord * vec2(texSize)), numLevels);

	if(alphaTest) fLevel *= 0.5;
	else fLevel *= 0.8;

	int iLevel = int(fLevel);

	ivec2 texPos0 = GetTexturePosition(iLevel,texPos);
	ivec2 texPos1 = GetTexturePosition(iLevel+1,texPos);

	ivec2 texSize0 = GetTextureSize(iLevel, texSize);
	ivec2 texSize1 = GetTextureSize(iLevel+1, texSize); 

	vec4 texLevel0 = texBiLinear(texSampler, wrapMode, vec2(texSize0), texPos0, texCoord);
	vec4 texLevel1 = texBiLinear(texSampler, wrapMode, vec2(texSize1), texPos1, texCoord);

	return mix(texLevel0, texLevel1, fract(fLevel));	// linear blend between our mipmap levels
}

vec4 GetTextureValue()
{
	vec4 tex1Data = textureR3D(tex1, textureWrapMode, ivec2(baseTexInfo.zw), ivec2(baseTexInfo.xy), fsTexCoord);

	if(textureInverted) {
		tex1Data.rgb = vec3(1.0) - vec3(tex1Data.rgb);
	}

	if (microTexture) {
		vec2 scale			= (vec2(baseTexInfo.zw) / 128.0) * microTextureScale;
		ivec2 pos			= GetMicroTexturePos(microTextureID);

		// add page offset to microtexture position
		pos.y				+= GetNextPageOffset(baseTexInfo.y);
	
		vec4 tex2Data		= textureR3D(tex1, ivec2(0), ivec2(128), pos, fsTexCoord * scale);

		float lod			= mip_map_level(fsTexCoord * scale * vec2(128.0));

		float blendFactor	= max(lod - 1.5, 0.0);			// bias -1.5
		blendFactor			= min(blendFactor, 1.0);		// clamp to max value 1
		blendFactor			= (blendFactor + 1.0) / 2.0;	// 0.5 - 1 range

		tex1Data			= mix(tex2Data, tex1Data, blendFactor);
	}

	if (alphaTest) {
		if (tex1Data.a < (32.0/255.0)) {
			discard;
		}
	}

	if(textureAlpha) {
		if(discardAlpha) {					// opaque 1st pass
			if (tex1Data.a < 1.0) {
				discard;
			}
		}
		else {								// transparent 2nd pass
			if ((tex1Data.a * fsColor.a) >= 1.0) {
				discard;
			}
		}
	}

	if (textureAlpha == false) {
		tex1Data.a = 1.0;
	}

	return tex1Data;
}

void Step15Luminous(inout vec4 colour)
{
	// luminous polys seem to behave very differently on step 1.5 hardware
	// when fixed shading is enabled the colour is modulated by the vp ambient + fixed shade value
	// when disabled it appears to be multiplied by 1.5, presumably to allow a higher range
	if(hardwareStep==0x15) {
		if(!lightEnabled && textureEnabled) {
			if(fixedShading) {
				colour.rgb *= 1.0 + fsFixedShade + lighting[1].y;
			}
			else {
				colour.rgb *= 1.5;
			}
		}
	}
}

float CalcFog()
{
	float z		= -fsViewVertex.z;
	float fog	= fogIntensity * clamp(fogStart + z * fogDensity, 0.0, 1.0);

	return fog;
}

void WriteOutputs(vec4 colour, int layer)
{
	vec4 blank = vec4(0.0);

	if(layer==LayerColour) {
		out0 = colour;
		out1 = blank;
		out2 = blank;
	}
	else if(layer==LayerTrans0) {
		out0 = blank;
		out1 = colour;
		out2 = blank;
	}
	else if(layer==LayerTrans1) {
		out0 = blank;
		out1 = blank;
		out2 = colour;
	}
}

)glsl";
