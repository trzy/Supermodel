#ifndef _R3DSHADERQUADS_H_
#define _R3DSHADERQUADS_H_

static const char *vertexShaderR3DQuads = R"glsl(

#version 410 core

// uniforms
uniform float	modelScale;
uniform mat4	modelMat;
uniform mat4	projMat;

// attributes
in vec4		inVertex;
in vec3		inNormal;
in vec2		inTexCoord;
in vec3		inFaceNormal;		// used to emulate r3d culling 
in float	inFixedShade;
in vec4		inColour;

// outputs to geometry shader

out VS_OUT
{
	vec3	viewVertex;
	vec3	viewNormal;		// per vertex normal vector
	vec2	texCoord;
	float	fixedShade;
	vec4	color;
	float	discardPoly;	// can't have varying bool (glsl spec)
} vs_out;

float CalcBackFace(in vec3 viewVertex)
{
	vec3 vt = viewVertex - vec3(0.0);
	vec3 vn = (mat3(modelMat) * inFaceNormal);

	// dot product of face normal with view direction
	return dot(vt, vn);
}

void main(void)
{
	vs_out.viewVertex	= vec3(modelMat * inVertex);
	vs_out.viewNormal	= (mat3(modelMat) * inNormal) / modelScale;
	vs_out.discardPoly	= CalcBackFace(vs_out.viewVertex);
	vs_out.color    	= inColour;
	vs_out.texCoord		= inTexCoord;
	vs_out.fixedShade	= inFixedShade;
	gl_Position			= projMat * modelMat * inVertex;
}
)glsl";

static const char *geometryShaderR3DQuads = R"glsl(

#version 410 core

layout (lines_adjacency) in;
layout (triangle_strip, max_vertices = 4) out;

in VS_OUT
{
	vec3	viewVertex;
	vec3	viewNormal;		// per vertex normal vector
	vec2	texCoord;
	float	fixedShade;
	vec4	color;
	float	discardPoly;	// can't have varying bool (glsl spec)
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
	flat float	fixedShade[4];
	flat vec4	color;
} gs_out;

float area(vec2 a, vec2 b)
{
	return a.x*b.y - a.y*b.x;
}

void main(void)
{
	if(gs_in[0].discardPoly>=0) {
		return;					//emulate back face culling here (all vertices in poly have same value)
	}

	int i, j, j_next;
	vec2 v[4];

	for (i=0; i<4; i++) {
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

	for (i=0; i<4; i++) {
		// Mapping of polygon vertex order to triangle strip vertex order.
		//
		// Quad (lines adjacency)    Triangle strip
		// vertex order:             vertex order:
		// 
		//        1----2                 1----3
		//        |    |      ===>       | \  |
		//        |    |                 |  \ |
		//        0----3                 0----2
		//
		int reorder[4] = int[]( 1, 0, 2, 3 );
		int ii = reorder[i];

		for (j=0; j<4; j++) {
			gs_out.v[j] = v[j] - v[ii];
		}
		for (j=0; j<4; j++) {
			j_next = (j+1) % 4;
			gs_out.area[j] = area(gs_out.v[j], gs_out.v[j_next]);
		}

		gl_Position = gl_in[ii].gl_Position;

		EmitVertex();
	}
}

)glsl";

static const char *fragmentShaderR3DQuads = R"glsl(

#version 410 core

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

// test
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
	flat float	fixedShade[4];
	flat vec4	color;
} fs_in;

//our calculated vertex attributes from the above
vec3	fsViewVertex;
vec3	fsViewNormal;
vec2	fsTexCoord;
float	fsFixedShade;
vec4	fsColor;

//outputs
out vec4 outColor;

void QuadraticInterpolation()
{
	uint i, i_next, i_prev;

	vec2 s[4];
	float A[4];

	for (i=0; i<4; i++) {
		s[i] = fs_in.v[i];
		A[i] = fs_in.area[i];
	}

	float D[4];
	float r[4];

	for (i=0; i<4; i++) {
		i_next = (i+1)%4;
		D[i] = dot(s[i], s[i_next]);
		r[i] = length(s[i]);
		if (fs_in.oneOverW[i] < 0) {  // is w[i] negative?
			r[i] = -r[i];
		}
	}

	float t[4];

	for (i=0; i<4; i++) {
		i_next = (i+1)%4;
		if(A[i]==0.0)	t[i] = 0;									// check for zero area + div by zero
		else			t[i] = (r[i]*r[i_next] - D[i]) / A[i];
	}

	float uSum = 0;
	float u[4];

	for (i=0; i<4; i++) {
		i_prev = (i-1)%4;
		u[i] = (t[i_prev] + t[i]) / r[i];
		uSum += u[i];
	}

	float lambda[4];

	for (i=0; i<4; i++) {
		lambda[i] = u[i] / uSum;
	}

	/* Discard fragments when all the weights are neither all negative nor all positive. */

	int lambdaSignCount = 0;

	for (i=0; i<4; i++) {
		if (fs_in.oneOverW[i] < 0) {
			if (lambda[i] > 0) {
				lambdaSignCount--;
			} else {
				lambdaSignCount++;
			}
		}
		else {
			if (lambda[i] < 0) {
				lambdaSignCount--;
			} else {
				lambdaSignCount++;
			}
		}
	}
	if (abs(lambdaSignCount) != 4) {
		discard;		// need to revisit this
	}

	float interp_oneOverW = 0;

	fsViewVertex	= vec3(0.0);
	fsViewNormal	= vec3(0.0);
	fsTexCoord		= vec2(0.0);
	fsFixedShade	= 0.0;
	fsColor			= fs_in.color;
	
	for (i=0; i<4; i++) {
		fsViewVertex	+= lambda[i] * fs_in.viewVertex[i];
		fsViewNormal	+= lambda[i] * fs_in.viewNormal[i];
		fsTexCoord		+= lambda[i] * fs_in.texCoord[i];
		fsFixedShade	+= lambda[i] * fs_in.fixedShade[i];
		interp_oneOverW	+= lambda[i] * fs_in.oneOverW[i];
	}

	fsViewVertex	/= interp_oneOverW;
	fsViewNormal	/= interp_oneOverW;
	fsTexCoord		/= interp_oneOverW;
	fsFixedShade	/= interp_oneOverW;

	vec4 vertex;
	float depth;

	// dirty hack for co-planar polys that really need 100% identical values to depth test correctly
	// the reason we waste cycles and calcute depth value here is because we have run out of vertex attribs
	if(fs_in.oneOverW[0]==fs_in.oneOverW[1] && 
	   fs_in.oneOverW[1]==fs_in.oneOverW[2] && 
	   fs_in.oneOverW[2]==fs_in.oneOverW[3]) {

		fsViewVertex.z	= fs_in.viewVertex[0].z / fs_in.oneOverW[0];
		vertex			= projMat * vec4(fsViewVertex,1.0);
		depth			= ((vertex.z / vertex.w) + 1.0) / 2.0;
	}
	else {
		vertex			= projMat * vec4(fsViewVertex,1.0);
		depth			= ((vertex.z * interp_oneOverW) + 1.0) / 2.0;
	}

	gl_FragDepth = depth;
}

vec4 GetTextureValue()
{
	vec4 tex1Data = texture2D( tex1, fsTexCoord.st);

	if(textureInverted) {
		tex1Data.rgb = vec3(1.0) - vec3(tex1Data.rgb);
	}

	if (microTexture) {
		vec2 scale    = (baseTexSize / 128.0) * microTextureScale;
		vec4 tex2Data = texture2D( tex2, fsTexCoord.st * scale);
		tex1Data = (tex1Data+tex2Data)/2.0;
	}

	if (alphaTest) {
		if (tex1Data.a < (8.0/16.0)) {
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

	QuadraticInterpolation();	// calculate our vertex attributes

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

	// Write output
	outColor = finalData;	
}
)glsl";

#endif