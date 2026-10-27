#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
layout(location = 3) in uint inColour;
layout(location = 4) in uint inFlags;
layout(location = 5) in float inFixedShade;
layout(location = 6) in float inTextureNP;
layout(location = 7) in vec3 inFaceNormal;

layout(location = 0) out vec3 vView;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec2 vUv;
layout(location = 3) out vec4 vColour;
layout(location = 4) out float vFixedShade;
layout(location = 5) out float vFaceDot;
layout(location = 6) out float vLodBase;

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

void main()
{
	vec4 view = pc.modelView * vec4(inPos, 1.0);
	vView = view.xyz;
	float scale = pc.extra.y != 0.0 ? pc.extra.y : 1.0;
	vNormal = mat3(pc.modelView) * inNormal / scale;
	vec3 face = mat3(pc.modelView) * inFaceNormal;
	vFaceDot = dot(view.xyz, face);
	vUv = inUv;
	vFixedShade = inFixedShade;
	vLodBase = vFaceDot * -pc.extra.x * inTextureNP;

	vec4 colour = unpackUnorm4x8(inColour);
	colour.a *= pc.fogParams.w;
	vColour = colour;

	gl_Position = pc.projection * view;
}
