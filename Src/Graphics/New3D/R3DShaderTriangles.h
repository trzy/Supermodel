#ifndef _R3DSHADERTRIANGLES_H_
#define _R3DSHADERTRIANGLES_H_

static const char *vertexShaderR3D = R"glsl(

#version 410 core

// uniforms
uniform float	modelScale;
uniform float	nodeAlpha;
uniform float	cota;
uniform mat4	modelMat;
uniform mat4	projMat;
uniform bool	translatorMap;

// attributes
in vec4		inVertex;
in vec3		inNormal;
in vec2		inTexCoord;
in vec3		inFaceNormal;		// used to emulate r3d culling 
in float	inFixedShade;
in vec4		inColour;
in float	inTextureNP;

// outputs to fragment shader
out vec3	fsViewVertex;
out vec3	fsViewNormal;		// per vertex normal vector
out vec2	fsTexCoord;
out vec4	fsColor;
out float	fsFixedShade;
out float	fsDiscardPoly;		// can't have varying bool (glsl spec)
out float	fsLODBase;

vec4 GetColour(vec4 colour)
{
	vec4 c = colour;

	if(translatorMap) {
		c.rgb *= 16.0;
	}

	c.a *= nodeAlpha;

	return c;
}

float CalcBackFace(in vec3 viewVertex)
{
	vec3 vt = viewVertex; // - vec3(0.0);
	vec3 vn = mat3(modelMat) * inFaceNormal;

	// dot product of face normal with view direction
	return dot(vt, vn);
}

void main(void)
{
	fsViewVertex	= (modelMat * inVertex).xyz;
	fsViewNormal	= (mat3(modelMat) / modelScale) * inNormal;
	fsDiscardPoly	= CalcBackFace(fsViewVertex);
	fsColor			= GetColour(inColour);
	fsTexCoord		= inTexCoord;
	fsFixedShade	= inFixedShade;
	fsLODBase		= fsDiscardPoly * -cota * inTextureNP;
	gl_Position		= (projMat * modelMat) * inVertex;
}
)glsl";

static const char *fragmentShaderR3D = R"glsl(

#version 410 core

uniform usampler2D textureBank[2];			// entire texture sheet

// texturing
uniform bool	textureEnabled;
uniform bool	microTexture;
uniform float	microTextureMinLOD;
uniform int		microTextureID;
uniform ivec4	baseTexInfo;		// x/y are x,y positions in the texture sheet. z/w are with and height
uniform int		baseTexType;
uniform bool	textureInverted;
uniform bool	textureAlpha;
uniform bool	alphaTest;
uniform bool	discardAlpha;
uniform ivec2	textureWrapMode;
uniform int		texturePage;

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
uniform bool	smoothShading;
uniform int		hardwareStep;
uniform int		colourLayer;
uniform bool	polyAlpha;

// matrices (shared with vertex shader)
uniform mat4	projMat;

//interpolated inputs from vertex shader
in vec3		fsViewVertex;
in vec3		fsViewNormal;		// per vertex normal vector
in vec2		fsTexCoord;
in vec4		fsColor;
in float	fsFixedShade;
in float	fsDiscardPoly;
in float	fsLODBase;

//outputs
layout(location = 0) out vec4 out0;		// opaque
layout(location = 1) out vec4 out1;		// trans layer 1
layout(location = 2) out vec4 out2;		// trans layer 2

// forward declarations (see common file)

float CalcFog();
void Step15Luminous(inout vec4 colour);
vec4 GetTextureValue();
void WriteOutputs(vec4 colour, int layer);
float Sqr(float a);
float SqrLength(vec2 a);

void main()
{
	vec4 tex1Data;
	vec4 colData;
	vec4 finalData;
	vec4 fogData;

	if(fsDiscardPoly > 0.0) {
		discard;		//emulate back face culling here
	}

	gl_FragDepth = projMat[3][2] * gl_FragCoord.w;

	fogData = vec4(fogColour.rgb * fogAmbient, CalcFog());
	tex1Data = vec4(1.0, 1.0, 1.0, 1.0);

	if(textureEnabled) {
		tex1Data = GetTextureValue();
	}

	colData = fsColor;
	Step15Luminous(colData);			// no-op for step 2.0+
	finalData = tex1Data * colData;

	if (finalData.a < (1.0/32.0)) {		// basically chuck out any totally transparent pixels value = 1/16 the smallest transparency level h/w supports
		discard;
	}

	float ellipse;
	ellipse = SqrLength((gl_FragCoord.xy - spotEllipse.xy) / spotEllipse.zw); // decay rate = square of distance from center
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
		range = 1.0 / Sqr(d * inv_r - 1.0);
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
		// We suspect that translucent polygons are always clamped (e.g. lasers in Daytona 2)
		if(sunClamp || finalData.a<1.0) {
			sunFactor = max(sunFactor,0.0);
		}

		// Total light intensity: sum of all components 
		lightIntensity = vec3(sunFactor*lighting[1].x + lighting[1].y);   // diffuse + ambient

		lightIntensity += spotColor*lobeEffect;

		// Upper clamp is optional, step 1.5+ games will drive brightness beyond 100%
		if(intensityClamp) {
			lightIntensity = min(lightIntensity,1.0);
		}

		finalData.rgb *= lightIntensity;

		// for now assume fixed shading doesn't work with specular
		if (specularEnabled) {

			float specularFactor;

			if (smoothShading)
			{
				// Always clamp floor to zero
				float NdotL = max(0.0, sunFactor);

				const float expIndex[4]  = float[4](8.0, 16.0, 32.0, 64.0);
				const float multIndex[4] = float[4](1.6, 1.6, 2.4, 3.2);
				float exponent = expIndex[int(shininess)];

				specularFactor = pow(NdotL, exponent);
				specularFactor *= multIndex[int(shininess)];
			}
			else
			{
				// flat shaded polys use Phong reflection model (R dot V) without any exponent or multiplier
				// V = (0.0, 0.0, 1.0) is used by Model 3 as a fast approximation, so R dot V = R.z
				vec3 R = reflect(-sunVector, fsViewNormal);
				specularFactor = max(0.0, R.z);
			}

			specularFactor *= specularValue;
			specularFactor *= lighting[1].x;

			if (colData.a < 1.0) {
				/// Specular hi-light affects translucent polygons alpha channel ///
				finalData.a = max(finalData.a, specularFactor);
			}

			finalData.rgb += specularFactor;
		}
	}

	// Final clamp: we need it for proper shading in dimmed light and dark ambients
	finalData.rgb = min(finalData.rgb, vec3(1.0));

	// Spotlight on fog
	vec3 lSpotFogColor = spotFogColor * fogAttenuation * fogColour.rgb * lobeFogEffect;

	// Fog & spotlight applied
	finalData.rgb = mix(finalData.rgb, fogData.rgb + lSpotFogColor, fogData.a);

	// Write outputs to colour buffers
	WriteOutputs(finalData,colourLayer);
}
)glsl";

#endif
