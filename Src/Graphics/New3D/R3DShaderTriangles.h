#ifndef _R3DSHADERTRIANGLES_H_
#define _R3DSHADERTRIANGLES_H_

static const char *vertexShaderR3D = R"glsl(

#version 120

// uniforms
uniform float	modelScale;
uniform mat4	modelMat;
uniform mat4	projMat;
uniform bool	translatorMap;

// attributes
attribute vec4	inVertex;
attribute vec3	inNormal;
attribute vec2	inTexCoord;
attribute vec4	inColour;
attribute vec3	inFaceNormal;		// used to emulate r3d culling 
attribute float	inFixedShade;

// outputs to fragment shader
varying vec3	fsViewVertex;
varying vec3	fsViewNormal;		// per vertex normal vector
varying vec2	fsTexCoord;
varying vec4	fsColor;
varying float	fsDiscard;			// can't have varying bool (glsl spec)
varying float	fsFixedShade;

vec4 GetColour(vec4 colour)
{
	vec4 c = colour;

	if(translatorMap) {
		c.rgb *= 16.0;
	}

	return c;
}

float CalcBackFace(in vec3 viewVertex)
{
	vec3 vt = viewVertex - vec3(0.0);
	vec3 vn = (mat3(modelMat) * inFaceNormal);

	// dot product of face normal with view direction
	return dot(vt, vn);
}

void main(void)
{
	fsViewVertex	= vec3(modelMat * inVertex);
	fsViewNormal	= (mat3(modelMat) * inNormal) / modelScale;
	fsDiscard		= CalcBackFace(fsViewVertex);
	fsColor    		= GetColour(inColour);
	fsTexCoord		= inTexCoord;
	fsFixedShade	= inFixedShade;
	gl_Position		= projMat * modelMat * inVertex;
}
)glsl";

static const char *fragmentShaderR3D = R"glsl(

#version 120
#extension GL_ARB_shader_texture_lod : require

uniform sampler2D tex1;			// base tex
uniform sampler2D tex2;			// micro tex (optional)

// texturing
uniform bool	textureEnabled;
uniform bool	microTexture;
uniform float	microTextureScale;
uniform vec2	baseTexSize;
uniform bool	textureInverted;
uniform bool	textureAlpha;
uniform bool	alphaTest;
uniform bool	discardAlpha;
uniform ivec2	textureWrapMode;

// general
uniform vec3	fogColour;
uniform vec4	spotEllipse;		// spotlight ellipse position: .x=X position (screen coordinates), .y=Y position, .z=half-width, .w=half-height)
uniform vec2	spotRange;			// spotlight Z range: .x=start (viewspace coordinates), .y=limit
uniform vec3	spotColor;			// spotlight RGB color
uniform vec3	spotFogColor;		// spotlight RGB color on fog
uniform vec3	lighting[2];		// lighting state (lighting[0] = sun direction, lighting[1].x,y = diffuse, ambient intensities from 0-1.0)
uniform bool	lightEnabled;		// lighting enabled (1.0) or luminous (0.0), drawn at full intensity
uniform bool	sunClamp;			// not used by daytona and la machine guns
uniform bool	intensityClamp;		// some games such as daytona and 
uniform bool	specularEnabled;	// specular enabled
uniform float	specularValue;		// specular coefficient
uniform float	shininess;			// specular shininess
uniform float	fogIntensity;
uniform float	fogDensity;
uniform float	fogStart;
uniform float	fogAttenuation;
uniform float	fogAmbient;
uniform bool	fixedShading;
uniform int		hardwareStep;

//interpolated inputs from vertex shader
varying vec3	fsViewVertex;
varying vec3	fsViewNormal;		// per vertex normal vector
varying vec4	fsColor;
varying vec2	fsTexCoord;
varying float	fsDiscard;
varying float	fsFixedShade;

float mip_map_level(in vec2 texture_coordinate) // in texel units
{
    vec2  dx_vtc        = dFdx(texture_coordinate);
    vec2  dy_vtc        = dFdy(texture_coordinate);
    float delta_max_sqr = max(dot(dx_vtc, dx_vtc), dot(dy_vtc, dy_vtc));
    float mml = 0.5 * log2(delta_max_sqr);
    return max( 0, mml );
}

float LinearTexLocations(int wrapMode, float size, float u, out float u0, out float u1)
{
	float texelSize		= 1.0 / size;
	float halfTexelSize	= 0.5 / size;

	if(wrapMode==0) {							// repeat
		u	= (u * size) - 0.5;
		u0	= (floor(u) + 0.5) / size;			// + 0.5 offset added to push us into the centre of a pixel, without we'll get rounding errors
		u0	= fract(u0);
		u1	= u0 + texelSize;
		u1	= fract(u1);

		return fract(u);						// return weight
	}
	else if(wrapMode==1) {						// repeat + clamp
		u	= fract(u);							// must force into 0-1 to start
		u	= (u * size) - 0.5;
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

		u	= (u * size) - 0.5;
		u0	= (floor(u) + 0.5) / size;			// + 0.5 offset added to push us into the centre of a pixel, without we'll get rounding errors
		u1	= u0 + texelSize;

		if(u0 <  0.0)	u0 = 0.0;
		if(u1 >= 1.0)	u1 = 1.0 - halfTexelSize;
		
		return fract(u);						// return weight
	}
}

vec4 texBiLinear(sampler2D texSampler, float level, ivec2 wrapMode, vec2 texSize, vec2 texCoord)
{
	float tx[2], ty[2];
	float a = LinearTexLocations(wrapMode.s, texSize.x, texCoord.x, tx[0], tx[1]);
	float b = LinearTexLocations(wrapMode.t, texSize.y, texCoord.y, ty[0], ty[1]);
	
	vec4 p0q0 = texture2DLod(texSampler, vec2(tx[0],ty[0]), level);
    vec4 p1q0 = texture2DLod(texSampler, vec2(tx[1],ty[0]), level);
    vec4 p0q1 = texture2DLod(texSampler, vec2(tx[0],ty[1]), level);
    vec4 p1q1 = texture2DLod(texSampler, vec2(tx[1],ty[1]), level);

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

vec4 textureR3D(sampler2D texSampler, ivec2 wrapMode, vec2 texSize, vec2 texCoord)
{
	float numLevels = floor(log2(min(texSize.x, texSize.y)));				// r3d only generates down to 1:1 for square textures, otherwise its the min dimension
	float fLevel	= min(mip_map_level(texCoord * texSize), numLevels);

	if(alphaTest) fLevel *= 0.5;
	else fLevel *= 0.8;

	float iLevel = floor(fLevel);						// value as an 'int'

	vec2 texSize0 = texSize / pow(2, iLevel);
	vec2 texSize1 = texSize / pow(2, iLevel+1.0);

	vec4 texLevel0 = texBiLinear(texSampler, iLevel, wrapMode, texSize0, texCoord);
	vec4 texLevel1 = texBiLinear(texSampler, iLevel+1.0, wrapMode, texSize1, texCoord);

	return mix(texLevel0, texLevel1, fract(fLevel));	// linear blend between our mipmap levels
}

vec4 GetTextureValue()
{
	vec4 tex1Data = textureR3D(tex1, textureWrapMode, baseTexSize, fsTexCoord);

	if(textureInverted) {
		tex1Data.rgb = vec3(1.0) - vec3(tex1Data.rgb);
	}

	if (microTexture) {
		vec2 scale			= (baseTexSize / 128.0) * microTextureScale;
		vec4 tex2Data		= textureR3D( tex2, ivec2(0), vec2(128.0), fsTexCoord * scale);

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
				colour.rgb *= vec3(1.5);
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

void main()
{
	vec4 tex1Data;
	vec4 colData;
	vec4 finalData;
	vec4 fogData;

	if(fsDiscard > 0) {
		discard;		//emulate back face culling here
	}

	fogData = vec4(fogColour.rgb * fogAmbient, CalcFog());
	tex1Data = vec4(1.0, 1.0, 1.0, 1.0);

	if(textureEnabled) {
		tex1Data = GetTextureValue();
	}

	colData = fsColor;
	Step15Luminous(colData);			// no-op for step 2.0+	
	finalData = tex1Data * colData;

	if (finalData.a < (1.0/16.0)) {		// basically chuck out any totally transparent pixels value = 1/16 the smallest transparency level h/w supports
		discard;
	}

	float ellipse;
	ellipse = length((gl_FragCoord.xy - spotEllipse.xy) / spotEllipse.zw);
	ellipse = pow(ellipse, 2.0);  // decay rate = square of distance from center
	ellipse = 1.0 - ellipse;      // invert
	ellipse = max(0.0, ellipse);  // clamp

	// Compute spotlight and apply lighting
	float enable, absExtent, d, inv_r, range;

	// start of spotlight
	enable = step(spotRange.x, -fsViewVertex.z);

	if (spotRange.y == 0.0) {
		range = 0.0;
	}
	else {
		absExtent = abs(spotRange.y);

		d = spotRange.x + absExtent + fsViewVertex.z;
		d = min(d, 0.0);

		// slope of decay function
		inv_r = 1.0 / (1.0 + absExtent);

		// inverse-linear falloff
		// Reference: https://imdoingitwrong.wordpress.com/2011/01/31/light-attenuation/
		// y = 1 / (d/r + 1)^2
		range = 1.0 / pow(d * inv_r - 1.0, 2.0);
		range *= enable;
	}

	float lobeEffect = range * ellipse;
	float lobeFogEffect = enable * ellipse;

	if (lightEnabled) {
		vec3   lightIntensity;
		vec3   sunVector;     // sun lighting vector (as reflecting away from vertex)
		float  sunFactor;     // sun light projection along vertex normal (0.0 to 1.0)

		// Sun angle
		sunVector = lighting[0];

		// Compute diffuse factor for sunlight
		if(fixedShading) {
			sunFactor = fsFixedShade;
		}
		else {
			sunFactor = dot(sunVector, fsViewNormal);
		}

		// Clamp ceil, fix for upscaled models without "modelScale" defined
		sunFactor = clamp(sunFactor,-1.0,1.0);

		// Optional clamping, value is allowed to be negative
		if(sunClamp) {
			sunFactor = max(sunFactor,0.0);
		}

		// Total light intensity: sum of all components 
		lightIntensity = vec3(sunFactor*lighting[1].x + lighting[1].y);   // diffuse + ambient

		lightIntensity.rgb += spotColor*lobeEffect;

		// Upper clamp is optional, step 1.5+ games will drive brightness beyond 100%
		if(intensityClamp) {
			lightIntensity = min(lightIntensity,1.0);
		}

		finalData.rgb *= lightIntensity;

		// for now assume fixed shading doesn't work with specular
		if (specularEnabled) {

			float exponent, NdotL, specularFactor;
			vec4 biasIndex, expIndex, multIndex;

			// Always clamp floor to zero, we don't want deep black areas
			NdotL = max(0.0,sunFactor);

			expIndex = vec4(8.0, 16.0, 32.0, 64.0);
			multIndex = vec4(2.0, 2.0, 3.0, 4.0);
			biasIndex = vec4(0.95, 0.95, 1.05, 1.0);
			exponent = expIndex[int(shininess)] / biasIndex[int(shininess)];

			specularFactor = pow(NdotL, exponent);
			specularFactor *= multIndex[int(shininess)];
			specularFactor *= biasIndex[int(shininess)];
			
			specularFactor *= specularValue;
			specularFactor *= lighting[1].x;

			if (colData.a < 1.0) {
				/// Specular hi-light affects translucent polygons alpha channel ///
				finalData.a = max(finalData.a, specularFactor);
			}

			finalData.rgb += vec3(specularFactor);
		}
	}

	// Final clamp: we need it for proper shading in dimmed light and dark ambients
	finalData.rgb = min(finalData.rgb, vec3(1.0));

	// Spotlight on fog
	vec3 lSpotFogColor = spotFogColor * fogAttenuation * fogColour.rgb * lobeFogEffect;

	 // Fog & spotlight applied
	finalData.rgb = mix(finalData.rgb, fogData.rgb + lSpotFogColor, fogData.a);

	gl_FragColor = finalData;
}
)glsl";

#endif