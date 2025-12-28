#ifndef _R3DSHADERQUADS_H_
#define _R3DSHADERQUADS_H_

static const char* vertexShaderR3DQuads = R"glsl(

#version 450 core

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

// outputs to geometry shader

out VS_OUT
{
	vec3	viewVertex;
	vec3	viewNormal;		// per vertex normal vector
	vec2	texCoord;
	vec4	color;
	float	fixedShade;
	float	discardPoly;	// can't have varying bool (glsl spec)
	float	LODBase;
} vs_out;

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
	vs_out.viewVertex	= (modelMat * inVertex).xyz;
	vs_out.viewNormal	= (mat3(modelMat) / modelScale) * inNormal;
	vs_out.discardPoly	= CalcBackFace(vs_out.viewVertex);
	vs_out.color		= GetColour(inColour);
	vs_out.texCoord		= inTexCoord;
	vs_out.fixedShade	= inFixedShade;
	vs_out.LODBase		= vs_out.discardPoly * -cota * inTextureNP;
	gl_Position			= (projMat * modelMat) * inVertex;
}
)glsl";

static const char* geometryShaderR3DQuads = R"glsl(

#version 450 core

layout (lines_adjacency) in;
layout (triangle_strip, max_vertices = 4) out;

in VS_OUT
{
	vec3	viewVertex;
	vec3	viewNormal;		// per vertex normal vector
	vec2	texCoord;
	vec4	color;
	float	fixedShade;
	float	discardPoly;	// can't have varying bool (glsl spec)
	float	LODBase;
} gs_in[4];

out GS_OUT
{
	noperspective vec2 v[4];
	noperspective float area[4];
	flat float oneOverW[4];

	//our regular attributes
	flat vec3	viewVertex[4];
	flat vec3	viewNormal[4];		// per vertex normal vector
	flat vec2	texCoord[4];
	flat vec4	color;
	flat float	fixedShade[4];
	flat float	LODBase;
	vec3 barycentricCoords;
} gs_out;

//a*b - c*d, computed in a stable fashion (Kahan)
float DifferenceOfProducts(float a, float b, float c, float d)
{
    precise float cd = c * d;
    precise float err = fma(-c, d, cd);
    precise float dop = fma(a, b, -cd);
    return dop + err;
}

void main(void)
{
	if(gs_in[0].discardPoly > 0.0) {
		return;					//emulate back face culling here (all vertices in poly have same value)
	}

	vec2 v[4];

	for (int i=0; i<4; i++) {
		float oneOverW		= 1.0 / gl_in[i].gl_Position.w;
		gs_out.oneOverW[i]	= oneOverW;
		v[i]				= gl_in[i].gl_Position.xy * oneOverW;

		// our regular vertex attribs
		gs_out.viewVertex[i]	= gs_in[i].viewVertex * oneOverW;
		gs_out.viewNormal[i]	= gs_in[i].viewNormal * oneOverW;
		gs_out.texCoord[i]		= gs_in[i].texCoord   * oneOverW;
		gs_out.fixedShade[i]	= gs_in[i].fixedShade * oneOverW;
	}

	// flat attributes
	gs_out.color = gs_in[0].color;
	gs_out.LODBase = gs_in[0].LODBase;

	// precompute crossproducts for all vertex combinations to be looked up in loop below for area computation
	precise float cross[4][4];
	for (int i=0; i<4; i++)
	{
		cross[i][i] = 0.0;
		for (int j=i+1; j<4; j++)
			cross[i][j] = DifferenceOfProducts(gl_in[i].gl_Position.x, gl_in[j].gl_Position.y, gl_in[j].gl_Position.x, gl_in[i].gl_Position.y) / (gl_in[i].gl_Position.w * gl_in[j].gl_Position.w);
	}
	for (int i=1; i<4; i++)
		for (int j=0; j<i; j++)
			cross[i][j] = -cross[j][i];

	for (int i=0; i<4; i++) {
		// Mapping of polygon vertex order to triangle strip vertex order.
		//
		// Quad (lines adjacency)    Triangle strip
		// vertex order:             vertex order:
		// 
		//        1----2                 0----2
		//        |    |      ===>       |  / |
		//        |    |                 | /  |
		//        0----3                 1----3
		//
		const int reorder[4] = int[4]( 1, 0, 2, 3 );
		int ii = reorder[i];

		for (int j=0; j<4; j++) {
			gs_out.v[j] = v[j] - v[ii];
			int j_next = (j+1) % 4;
			// compute area via shoelace algorithm BUT divided by w afterwards to improve precision!
			// in addition also use Kahans algorithm to further improve precision of the 2D crossproducts
			gs_out.area[j] = cross[j][j_next] + cross[j_next][ii] + cross[ii][j];
		}

		const vec3 bary[4] = vec3[](
			vec3(1.0, 0.0, 0.0),
			vec3(0.0, 1.0, 0.0),
			vec3(0.0, 0.0, 1.0),
			vec3(0.0,-1.0, 0.0)		// make the last coordinate negative so we can tell the difference between the triangles
		);

		gs_out.barycentricCoords = bary[ii];

		gl_Position = gl_in[ii].gl_Position;

		EmitVertex();
	}
}

)glsl";

static const char* fragmentShaderR3DQuads = R"glsl(

#version 450 core

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

//interpolated inputs from geometry shader

in GS_OUT
{
	noperspective vec2 v[4];
	noperspective float area[4];
	flat float oneOverW[4];

	//our regular attributes
	flat vec3	viewVertex[4];
	flat vec3	viewNormal[4];		// per vertex normal vector
	flat vec2	texCoord[4];
	flat vec4	color;
	flat float	fixedShade[4];
	flat float	LODBase;
	vec3 barycentricCoords;
} fs_in;

//our calculated vertex attributes from the above
vec3	fsViewVertex;
vec3	fsViewNormal;
vec2	fsTexCoord;
vec4	fsColor;
float	fsFixedShade;
float	fsLODBase;

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
void ComputeSpotlight(vec4 spotEllipse, vec2 spotRange, float viewZ, out float lobeEffect, out float lobeFogEffect);

void QuadraticInterpolation()
{
	// init interpolated vertex attributes
	fsViewVertex	= vec3(0.0);
	fsViewNormal	= vec3(0.0);
	fsTexCoord		= vec2(0.0);
	fsFixedShade	= 0.0;

	float interp_oneOverW	= 0.0;
	float uSum				= 0.0;

	const float hpFMin		= 0.00006103515625;		// half precision FLT_MIN
	bool extreme_edge		= (fs_in.barycentricCoords.x <= hpFMin || fs_in.barycentricCoords.z <= hpFMin); 

	// As the coordinates get closer to the poly edge the interpolated areas can approach zero which can blow up the maths leading to bad interpolation.
	// In this case we fall back to 3 point triangle interpolation (really 2 because 1 coord will be zero).
	// This code will only get triggered in rarer corner cases

	float u[4];
	if(extreme_edge)
	{
		for (int i=0; i<4; i++) {
			int b = (i == 3) ? 1  // 2nd triangle
			                 : i; // 1st triangle
			float baryCoord	= abs(fs_in.barycentricCoords[b]);
			u[i] = baryCoord / fs_in.oneOverW[i];
			if((i == 1 && !(fs_in.barycentricCoords.y>0.0)) // ignore u[1] if 2nd triangle
			 ||(i == 3 &&  (fs_in.barycentricCoords.y>0.0)) // ignore u[3] if 1st triangle
			 ||(baryCoord <= hpFMin))
			{
				u[i] = 0.0;
			}
		}
	}
	else
	{
		for (int i=0; i<4; i++)
			u[i] = length(fs_in.v[i]) * sign(fs_in.oneOverW[i]); // is w[i] negative?

		precise float t[4];
		for (int i=0; i<4; i++) {
			int i_next = (i+1)%4;
			if(fs_in.area[i]==0.0) t[i] = 0.0; // check for zero area to avoid div by zero
			else                   t[i] = fma(u[i],u[i_next], -dot(fs_in.v[i],fs_in.v[i_next])) / fs_in.area[i];
		}

		int lambdaSignCount = 0; // to discard fragments if all the weights are neither all negative nor all positive (=outside the convex/concave/crossed quad).

		for (uint i=0; i<4; i++) {
			uint i_prev = (i-1)%4;
			u[i] = (t[i_prev] + t[i]) / u[i];
			lambdaSignCount += (t[i_prev] < -t[i]) ? -1 : 1;
		}

		if (lambdaSignCount == 0) { // one can either check for == 0 or abs(...) != 4, both should(!) be equivalent (but in practice its not due to precision issues, but these cases are extremely rare)
			if(!gl_HelperInvocation) {
				discard;
			}
		}

		for (int i=0; i<4; i++) {
			interp_oneOverW	= fma(fs_in.oneOverW[i], u[i], interp_oneOverW);
			uSum           += u[i];
		}
	}

	// shared computations
	for (int i=0; i<4; i++) {
		fsViewVertex	= fma(fs_in.viewVertex[i], vec3(u[i]), fsViewVertex);
		fsViewNormal	= fma(fs_in.viewNormal[i], vec3(u[i]), fsViewNormal);
		fsTexCoord		= fma(fs_in.texCoord[i], vec2(u[i]), fsTexCoord);
		fsFixedShade	= fma(fs_in.fixedShade[i], u[i], fsFixedShade);
	}

	// finalize the 2 cases
	if(extreme_edge)
	{
		float w = (projMat * vec4(fsViewVertex,1.0)).w;

		interp_oneOverW = 1.0 / w;
		uSum = 1.0;
	}
	else
	{
		float inv = 1.0/interp_oneOverW;
		fsViewVertex *= inv;
		fsViewNormal *= inv;
		fsTexCoord   *= inv;
		fsFixedShade *= inv;
	}

	vec4 vertex;
	float depth;

	// dirty hack for co-planar polys that really need 100% identical values to depth test correctly
	// the reason we waste cycles and calculate depth value here is because we have run out of vertex attribs
	if(fs_in.oneOverW[0]==fs_in.oneOverW[1] && 
	   fs_in.oneOverW[1]==fs_in.oneOverW[2] && 
	   fs_in.oneOverW[2]==fs_in.oneOverW[3]) {

		fsViewVertex.z	= fs_in.viewVertex[0].z / fs_in.oneOverW[0];
		vertex			= projMat * vec4(fsViewVertex,1.0);
		depth			= vertex.z / vertex.w;
	}
	else {
		vertex.z		= projMat[2][2] * fsViewVertex.z + projMat[3][2];		// standard projMat * vertex - but just using Z components
		depth			= vertex.z * (interp_oneOverW/uSum);
	}

	fsColor			= fs_in.color;
	fsLODBase		= fs_in.LODBase;

	gl_FragDepth = depth;
}

void main()
{
	vec4 tex1Data;
	vec4 colData;
	vec4 finalData;
	vec4 fogData;

	QuadraticInterpolation();	// calculate our vertex attributes

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

	float lobeEffect;
	float lobeFogEffect;
	ComputeSpotlight(spotEllipse, spotRange, fsViewVertex.z, lobeEffect, lobeFogEffect);

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
		if(sunClamp || finalData.a<0.999) {
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
