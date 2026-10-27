#version 450

// Full-screen quad for the ambient / scroll fog background pass. The fog is
// evaluated per fragment (spotlight on fog needs gl_FragCoord), so this shader
// only has to forward the screen position.
layout(location = 0) out vec2 vScreen;

void main()
{
	vec2 pos[4] = vec2[4](
		vec2(-1.0, -1.0),
		vec2(-1.0,  1.0),
		vec2( 1.0, -1.0),
		vec2( 1.0,  1.0));
	gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
	vScreen = pos[gl_VertexIndex];
}
