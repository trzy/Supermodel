#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 vView;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vUv;
layout(location = 3) in vec4 vColour;
layout(location = 4) in float vFixedShade;
layout(location = 5) in float vFaceDot;
layout(location = 6) in float vLodBase;

layout(location = 0) out vec4 outColour;

struct R3DMaterial
{
	uint flags;
	int texX;
	int texY;
	int texWidth;
	int texHeight;
	int texFormat;
	int texturePage;
	int wrapModeX;
	int wrapModeY;
	int microTextureID;
	float microMinLOD;
	float shininess;
	float specularValue;
	float fogIntensity;
	float pad0;
	float pad1;
};

layout(set = 0, binding = 0) uniform sampler uSamplers[16];
layout(set = 0, binding = 1) uniform ViewportUBO
{
	vec4 spotEllipse;
	vec4 spotRange;
	vec4 spotColor;
	vec4 spotFogColor;
} ubo;
layout(std430, set = 0, binding = 2) readonly buffer Materials
{
	R3DMaterial m[];
} materials;
// The two Real3D texture pages, exactly as they exist in hardware: 2048x1024
// 16-bit texels with the mip pyramid of each sheet packed into the same image.
// This is the only texture source the fragment shader samples, just like New3D.
layout(set = 0, binding = 3) uniform usampler2D uPages[2];

layout(push_constant) uniform PushConstants
{
	mat4 modelView;
	mat4 projection;
	vec4 lightDir;
	vec4 lightParams;
	vec4 fogColour;
	vec4 fogParams;
	vec4 extra;
} pc;

const uint kTextured     = 1u << 0;
const uint kAlphaTest    = 1u << 1;
const uint kTextureAlpha = 1u << 2;
const uint kLighting     = 1u << 3;
const uint kFixedShade   = 1u << 4;
const uint kSpecular     = 1u << 5;
const uint kTranslator   = 1u << 6;
const uint kInverted     = 1u << 7;
const uint kSmooth       = 1u << 8;
const uint kMicro        = 1u << 10;

float Sqr(float a) { return a * a; }

// ---------------------------------------------------------------------------
// Texture decode / fetch
//
// The code below is a direct port of New3D's R3DShaderCommon.h so that the
// Vulkan backend reproduces the Real3D hardware exactly: palette formats are
// unpacked in the shader, the mip pyramid is addressed by shifting the base
// position (r3d only packs mips down to 2:2), wrapping masks against the mip
// size, and bilinear filtering is computed manually with wrap-aware taps.
// ---------------------------------------------------------------------------

vec4 ExtractColour(int type, uint value)
{
	vec4 c = vec4(0.0);

	if (type == 0) {			// T1RGB5
		c.r = float((value >> 10) & 0x1Fu);
		c.g = float((value >> 5) & 0x1Fu);
		c.b = float((value) & 0x1Fu);
		c.rgb *= (1.0 / 31.0);
		c.a = 1.0 - float((value >> 15) & 0x1u);
	}
	else if (type == 1) {		// interleaved A4L4 (low byte)
		c.rgb = vec3(float(value & 0xFu));
		c.a = float((value >> 4) & 0xFu);
		c *= (1.0 / 15.0);
	}
	else if (type == 2) {
		c.a = float(value & 0xFu);
		c.rgb = vec3(float((value >> 4) & 0xFu));
		c *= (1.0 / 15.0);
	}
	else if (type == 3) {
		c.rgb = vec3(float((value >> 8) & 0xFu));
		c.a = float((value >> 12) & 0xFu);
		c *= (1.0 / 15.0);
	}
	else if (type == 4) {
		c.a = float((value >> 8) & 0xFu);
		c.rgb = vec3(float((value >> 12) & 0xFu));
		c *= (1.0 / 15.0);
	}
	else if (type == 5) {
		c = vec4(float(value & 0xFFu) / 255.0);
		c.a = (c.a == 1.0) ? 0.0 : 1.0;
	}
	else if (type == 6) {
		c = vec4(float((value >> 8) & 0xFFu) / 255.0);
		c.a = (c.a == 1.0) ? 0.0 : 1.0;
	}
	else if (type == 7) {		// RGBA4
		c.r = float((value >> 12) & 0xFu);
		c.g = float((value >> 8) & 0xFu);
		c.b = float((value >> 4) & 0xFu);
		c.a = float((value >> 0) & 0xFu);
		c *= (1.0 / 15.0);
	}
	else if (type == 8) {		// low byte, low nibble
		c = vec4(float(value & 0xFu) / 15.0);
		c.a = (c.a == 1.0) ? 0.0 : 1.0;
	}
	else if (type == 9) {		// low byte, high nibble
		c = vec4(float((value >> 4) & 0xFu) / 15.0);
		c.a = (c.a == 1.0) ? 0.0 : 1.0;
	}
	else if (type == 10) {		// high byte, low nibble
		c = vec4(float((value >> 8) & 0xFu) / 15.0);
		c.a = (c.a == 1.0) ? 0.0 : 1.0;
	}
	else if (type == 11) {		// high byte, high nibble
		c = vec4(float((value >> 12) & 0xFu) / 15.0);
		c.a = (c.a == 1.0) ? 0.0 : 1.0;
	}

	return c;
}

// Real3D stores each mip level of a texture inside the packed texture page at a
// fixed base offset (the same tables New3D uses in TextureBank::UploadTextures).
// All levels therefore read from the single physical page; only the base offset
// changes. New3D gets this for free because it uploads each level into the
// matching GL mip level, but our Vulkan page is a plain RAM copy with one level,
// so we add the offset ourselves and always fetch level 0.
const int kMipX[12] = int[](0, 1024, 1536, 1792, 1920, 1984, 2016, 2032, 2040, 2044, 2046, 2047);
const int kMipY[12] = int[](0, 512, 768, 896, 960, 992, 1008, 1016, 1020, 1022, 1023, 1023);

uint FetchTexel(int page, ivec2 coordinate, int level)
{
	ivec2 addr = coordinate + ivec2(kMipX[level], kMipY[level]);

	return texelFetch(uPages[nonuniformEXT(page)], addr, 0).r;
}

// Wrap around is a good guess for reads outside the texture sheet.
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

	return ivec2(xCoords[id], yCoords[id]);
}

float LinearTexLocations(int wrapMode, float size, float u, out float u0, out float u1)
{
	float texelSize		= 1.0 / size;
	float halfTexelSize	= 0.5 / size;

	if (wrapMode == 0) {						// repeat
		u	= u * size - 0.5;
		u0	= (floor(u) + 0.5) / size;			// + 0.5 offset added to push us into the centre of a pixel, without we'll get rounding errors
		u0	= fract(u0);
		u1	= u0 + texelSize;
		u1	= fract(u1);

		return fract(u);						// return weight
	}
	else if (wrapMode == 1) {					// repeat + clamp
		u	= fract(u);							// must force into 0-1 to start
		u	= u * size - 0.5;
		u0	= (floor(u) + 0.5) / size;
		u1	= u0 + texelSize;

		if (u0 < 0.0)	u0 = 0.0;
		if (u1 >= 1.0)	u1 = 1.0 - halfTexelSize;

		return fract(u);
	}
	else {										// mirror + mirror clamp - both are the same since the edge pixels are repeated anyway
		float odd = floor(mod(u, 2.0));			// odd values are mirrored

		if (odd > 0.0) {
			u = 1.0 - fract(u);
		}
		else {
			u = fract(u);
		}

		u	= u * size - 0.5;
		u0	= (floor(u) + 0.5) / size;
		u1	= u0 + texelSize;

		if (u0 < 0.0)	u0 = 0.0;
		if (u1 >= 1.0)	u1 = 1.0 - halfTexelSize;

		return fract(u);
	}
}

vec4 texBiLinear(int page, ivec2 wrapMode, vec2 texSize, ivec2 texPos,
	vec2 texCoord, int level, int baseTexType, bool alphaTest)
{
	float tx[2], ty[2];
	float a = LinearTexLocations(wrapMode.s, texSize.x, texCoord.x, tx[0], tx[1]);
	float b = LinearTexLocations(wrapMode.t, texSize.y, texCoord.y, ty[0], ty[1]);

	vec4 p0q0 = ExtractColour(baseTexType, FetchTexel(page, WrapTexCoords(texPos, ivec2(vec2(tx[0], ty[0]) * texSize + texPos), level), level));
	vec4 p1q0 = ExtractColour(baseTexType, FetchTexel(page, WrapTexCoords(texPos, ivec2(vec2(tx[1], ty[0]) * texSize + texPos), level), level));
	vec4 p0q1 = ExtractColour(baseTexType, FetchTexel(page, WrapTexCoords(texPos, ivec2(vec2(tx[0], ty[1]) * texSize + texPos), level), level));
	vec4 p1q1 = ExtractColour(baseTexType, FetchTexel(page, WrapTexCoords(texPos, ivec2(vec2(tx[1], ty[1]) * texSize + texPos), level), level));

	if (alphaTest) {
		if (p0q0.a > p1q0.a)	{ p1q0.rgb = p0q0.rgb; }
		if (p0q0.a > p0q1.a)	{ p0q1.rgb = p0q0.rgb; }

		if (p1q0.a > p0q0.a)	{ p0q0.rgb = p1q0.rgb; }
		if (p1q0.a > p1q1.a)	{ p1q1.rgb = p1q0.rgb; }

		if (p0q1.a > p0q0.a)	{ p0q0.rgb = p0q1.rgb; }
		if (p0q1.a > p1q1.a)	{ p1q1.rgb = p0q1.rgb; }

		if (p1q1.a > p0q1.a)	{ p0q1.rgb = p1q1.rgb; }
		if (p1q1.a > p1q0.a)	{ p1q0.rgb = p1q1.rgb; }
	}

	vec4 pInterp_q0 = mix(p0q0, p1q0, a);	// interpolate top row in X direction
	vec4 pInterp_q1 = mix(p0q1, p1q1, a);	// interpolate bottom row in X direction

	return mix(pInterp_q0, pInterp_q1, b);	// interpolate in Y direction
}

vec4 GetTextureValue(R3DMaterial mat, bool discardAlpha)
{
	float lod = -log2(gl_FragCoord.w * gl_FragCoord.w * vLodBase);
	float numLevels = floor(log2(min(float(mat.texWidth), float(mat.texHeight)))) - 1.0;	// r3d only generates down to 2:2 for square textures, otherwise its the min dimension
	float fLevel = clamp(lod, 0.0, numLevels);

	int iLevel = int(fLevel);

	ivec2 wrapMode	= ivec2(mat.wrapModeX, mat.wrapModeY);
	ivec2 basePos	= ivec2(mat.texX, mat.texY);
	ivec2 baseSize	= ivec2(mat.texWidth, mat.texHeight);
	bool  alphaTest	= (mat.flags & kAlphaTest) != 0u;

	int page = mat.texturePage & 1;
	ivec2 tex1Pos = GetTexturePosition(iLevel, basePos);
	ivec2 tex1Size = GetTextureSize(iLevel, baseSize);
	vec4 tex1Data = texBiLinear(page, wrapMode, vec2(tex1Size), tex1Pos, vUv, iLevel, mat.texFormat, alphaTest);

	// init second texel with blank data to avoid any potentially undefined behavior
	vec4 tex2Data = vec4(0.0);

	float blendFactor = 0.0;

	// if LOD < 0, no need to blend with next mipmap level
	float ffL = fract(fLevel);
	if (ffL > 0.0)
	{
		ivec2 tex2Pos = GetTexturePosition(iLevel + 1, basePos);
		ivec2 tex2Size = GetTextureSize(iLevel + 1, baseSize);
		tex2Data = texBiLinear(page, wrapMode, vec2(tex2Size), tex2Pos, vUv, iLevel + 1, mat.texFormat, alphaTest);

		blendFactor = ffL;
	}
	else if ((mat.flags & kMicro) != 0u && lod < 0.0)
	{
		vec4 scaleIndex = vec4(2.0, 4.0, 16.0, 256.0);		// unsure if minLOD=4 has 256x scale? No games appear to use it
		vec2 scale = (vec2(baseSize) / 128.0) * scaleIndex[int(clamp(mat.microMinLOD, 0.0, 3.0))];

		// microtextures are always 128x128 and only use LOD 0 mipmap
		ivec2 tex2Pos = GetMicroTexturePos(mat.microTextureID & 7);
		tex2Data = texBiLinear((page + 1) & 1, ivec2(0), vec2(128.0), tex2Pos, vUv * scale, 0, mat.texFormat, alphaTest);

		blendFactor = -lod * exp2(-mat.microMinLOD) * 0.5;
		blendFactor = min(blendFactor, 0.5);
	}

	tex1Data = mix(tex1Data, tex2Data, blendFactor);

	if ((mat.flags & kInverted) != 0u) {
		tex1Data.rgb = vec3(1.0) - tex1Data.rgb;
	}

	if (alphaTest) {
		if (tex1Data.a < (32.0 / 255.0)) {
			discard;
		}
	}

	if ((mat.flags & kTextureAlpha) != 0u) {
		if (discardAlpha) {					// opaque 1st pass
			if (tex1Data.a < 1.0) {
				discard;
			}
		}
		else {								// transparent 2nd pass
			if ((tex1Data.a * vColour.a) >= 1.0) {
				discard;
			}
		}
	}

	if ((mat.flags & kTextureAlpha) == 0u && !alphaTest) {
		tex1Data.a = 1.0;
	}

	return tex1Data;
}

void main()
{
	if (vFaceDot > 0.0)
		discard;

	uint index = floatBitsToUint(pc.extra.w);
	R3DMaterial mat = materials.m[index];

	vec4 tex = vec4(1.0);
	if ((mat.flags & kTextured) != 0u)
		tex = GetTextureValue(mat, pc.extra.z > 0.5);

	vec4 baseColour = vColour;
	if ((mat.flags & kTranslator) != 0u)
		baseColour.rgb *= 16.0;
	vec4 colour = tex * baseColour;

	int hardwareStep = int(pc.lightParams.w + 0.5);
	if (hardwareStep == 0x15 && (mat.flags & kLighting) == 0u)
	{
		if ((mat.flags & kFixedShade) != 0u)
			colour.rgb *= 1.0 + vFixedShade + pc.lightParams.x;
		else
			colour.rgb *= 1.5;
	}

	if (colour.a < (1.0 / 32.0))
		discard;

	float ellipse = 1.0 - dot((gl_FragCoord.xy - ubo.spotEllipse.xy) / max(ubo.spotEllipse.zw, vec2(1.0)),
		(gl_FragCoord.xy - ubo.spotEllipse.xy) / max(ubo.spotEllipse.zw, vec2(1.0)));
	ellipse = max(ellipse, 0.0);
	float enable = step(ubo.spotRange.x, -vView.z);
	float range = 0.0;
	if (ubo.spotRange.y != 0.0)
	{
		float absExtent = abs(ubo.spotRange.y);
		float d = min(ubo.spotRange.x + absExtent + vView.z, 0.0);
		float invR = 1.0 / (1.0 + absExtent);
		range = 1.0 / Sqr(d * invR - 1.0);
		range *= enable;
	}
	float lobe = range * ellipse;
	float lobeFog = enable * ellipse;

	if ((mat.flags & kLighting) != 0u)
	{
		float sunFactor = (mat.flags & kFixedShade) != 0u ? vFixedShade : dot(pc.lightDir.xyz, normalize(vNormal));
		sunFactor = clamp(sunFactor, -1.0, 1.0);
		if (pc.lightParams.y > 0.5 || colour.a < 0.999)
			sunFactor = max(sunFactor, 0.0);

		vec3 light = vec3(sunFactor * pc.lightDir.w + pc.lightParams.x);
		light += ubo.spotColor.rgb * lobe;
		if (pc.lightParams.z > 0.5)
			light = min(light, vec3(1.0));
		colour.rgb *= light;

		if ((mat.flags & kSpecular) != 0u)
		{
			float spec;
			if ((mat.flags & kSmooth) != 0u)
			{
				float expIndex[4] = float[4](8.0, 16.0, 32.0, 64.0);
				float multIndex[4] = float[4](1.6, 1.6, 2.4, 3.2);
				int shin = clamp(int(mat.shininess + 0.5), 0, 3);
				spec = pow(max(sunFactor, 0.0), expIndex[shin]) * multIndex[shin];
			}
			else
			{
				vec3 R = reflect(-pc.lightDir.xyz, normalize(vNormal));
				spec = max(R.z, 0.0);
			}
			spec *= mat.specularValue * pc.lightDir.w;
			if (vColour.a < 1.0)
				colour.a = max(colour.a, spec);
			colour.rgb += spec;
		}
	}

	colour.rgb = min(colour.rgb, vec3(1.0));

	float fog = mat.fogIntensity * clamp(pc.fogParams.x + (-vView.z) * pc.fogColour.a, 0.0, 1.0);
	vec3 fogRgb = pc.fogColour.rgb * pc.fogParams.z;
	fogRgb += ubo.spotFogColor.rgb * pc.fogParams.y * pc.fogColour.rgb * lobeFog;
	colour.rgb = mix(colour.rgb, fogRgb, fog);

	// Real3D writes a 1/w depth so nearer fragments compare greater.
	gl_FragDepth = pc.projection[3][2] * gl_FragCoord.w;
	outColour = colour;
}
