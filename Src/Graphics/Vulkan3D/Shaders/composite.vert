#version 450

layout(location = 0) out vec2 vUv;

void main()
{
	vec2 pos[4] = vec2[4](
		vec2(-1.0, -1.0),
		vec2(-1.0,  1.0),
		vec2( 1.0, -1.0),
		vec2( 1.0,  1.0));
	// Vulkan framebuffer y=0 is the top. Memory row 0 of a tile layer is the top.
	vec2 uv[4] = vec2[4](
		vec2(0.0, 0.0),
		vec2(0.0, 1.0),
		vec2(1.0, 0.0),
		vec2(1.0, 1.0));
	gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
	vUv = uv[gl_VertexIndex];
}
