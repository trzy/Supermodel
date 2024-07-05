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

// hardware testing would be useful because i don't know exactly what happens if you try to read outside the texture sheet
// wrap around is a good guess
ivec2 WrapTexCoords(ivec2 pos, ivec2 coordinate, int level)
{
	ivec2 newCoord;
	newCoord.x = coordinate.x & (2047 >> level);
	newCoord.y = coordinate.y & (1023 >> level);

	return newCoord;
}

ivec2 GetTextureSize(int level, ivec2 size)
{
	int mipDivisor = 1 << level;

	return size / mipDivisor;
}

ivec2 GetTexturePosition(int level, ivec2 pos)
{
	int mipDivisor = 1 << level;

	ivec2 retPos;
	retPos.x = pos.x / mipDivisor;
	retPos.y = pos.y / mipDivisor;

	return retPos;
}

ivec2 GetMicroTexturePos(int id)
{
	const int xCoords[8] = int[](0, 0, 128, 128, 0, 0, 128, 128);
	const int yCoords[8] = int[](0, 128, 0, 128, 256, 384, 256, 384);

	return ivec2(xCoords[id],yCoords[id]);
}

float mip_map_level(in vec3 coordinate)
{
	// Real3D uses vertex coordinates rather than texel coordinates to calculate mipmap levels
	vec3 dx_vtc = dFdx(coordinate);
	vec3 dy_vtc = dFdy(coordinate);
	float delta_max_sqr = max(dot(dx_vtc, dx_vtc), dot(dy_vtc, dy_vtc));
	return log2(delta_max_sqr / (fsTextureNP * fsTextureNP)) * 0.5;			// result not clamped
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

vec4 texBiLinear(usampler2D texSampler, ivec2 wrapMode, vec2 texSize, ivec2 texPos, vec2 texCoord, int level)
{
	float tx[2], ty[2];
	float a = LinearTexLocations(wrapMode.s, texSize.x, texCoord.x, tx[0], tx[1]);
	float b = LinearTexLocations(wrapMode.t, texSize.y, texCoord.y, ty[0], ty[1]);

	vec4 p0q0 = ExtractColour(baseTexType,texelFetch(texSampler, WrapTexCoords(texPos,ivec2(vec2(tx[0],ty[0]) * texSize + texPos),level), level).r);
    vec4 p1q0 = ExtractColour(baseTexType,texelFetch(texSampler, WrapTexCoords(texPos,ivec2(vec2(tx[1],ty[0]) * texSize + texPos),level), level).r);
    vec4 p0q1 = ExtractColour(baseTexType,texelFetch(texSampler, WrapTexCoords(texPos,ivec2(vec2(tx[0],ty[1]) * texSize + texPos),level), level).r);
    vec4 p1q1 = ExtractColour(baseTexType,texelFetch(texSampler, WrapTexCoords(texPos,ivec2(vec2(tx[1],ty[1]) * texSize + texPos),level), level).r);

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

vec4 GetTextureValue()
{
	float lod = mip_map_level(fsViewVertex);
	float numLevels = floor(log2(min(float(baseTexInfo.z), float(baseTexInfo.w))));		// r3d only generates down to 1:1 for square textures, otherwise its the min dimension
	float fLevel = clamp(lod, 0.0, numLevels);

	int iLevel = int(fLevel);

	ivec2 tex1Pos = GetTexturePosition(iLevel, ivec2(baseTexInfo.xy));
	ivec2 tex1Size = GetTextureSize(iLevel, ivec2(baseTexInfo.zw));
	vec4 tex1Data = texBiLinear(textureBank[texturePage], textureWrapMode, vec2(tex1Size), tex1Pos, fsTexCoord, iLevel);

	// init second texel with blank data to avoid any potentially undefined behavior
	vec4 tex2Data = vec4(0.0);

	float blendFactor = 0.0;

	// if LOD < 0, no need to blend with next mipmap level; slight performance boost
	if (lod > 0.0)
	{
		ivec2 tex2Pos = GetTexturePosition(iLevel+1, ivec2(baseTexInfo.xy));
		ivec2 tex2Size = GetTextureSize(iLevel+1, ivec2(baseTexInfo.zw));
		tex2Data = texBiLinear(textureBank[texturePage], textureWrapMode, vec2(tex2Size), tex2Pos, fsTexCoord, iLevel+1);

		blendFactor = fract(fLevel);
	}
	else if (microTexture && lod < -microTextureMinLOD)
	{
		vec4 scaleIndex = vec4(2.0, 4.0, 16.0, 256.0);		// unsure if minLOD=4 has 256x scale? No games appear to use it
		vec2 scale = (vec2(baseTexInfo.zw) / 128.0) * scaleIndex[int(microTextureMinLOD)];

		// microtextures are always 128x128 and only use LOD 0 mipmap
		ivec2 tex2Pos = GetMicroTexturePos(microTextureID);
		tex2Data = texBiLinear(textureBank[(texturePage+1)&1], ivec2(0), ivec2(128), tex2Pos, fsTexCoord * scale, 0);

		blendFactor = -(lod + microTextureMinLOD) * 0.25;
		blendFactor = clamp(blendFactor, 0.0, 0.5);
	}

	tex1Data = mix(tex1Data, tex2Data, blendFactor);

	if(textureInverted) {
		tex1Data.rgb = vec3(1.0) - vec3(tex1Data.rgb);
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

float Sqr(float a)
{
	return a*a;
}

float SqrLength(vec2 a)
{
	return a.x*a.x + a.y*a.y;
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
