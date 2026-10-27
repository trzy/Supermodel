#version 450

// Ambient / scroll fog background pass.
//
// This is a port of New3D's R3DScrollFog fragment shader. New3D draws two
// full-viewport quads before rendering the 3D layers:
//
//   DrawAmbientFog()  rgba = { 0, 0, 0, 1 - fogAmbient }, attenuation = 0, ambient = 1
//   DrawScrollFog()   rgba = { fogColour, scrollFog },    attenuation = fogAttenuation,
//                                                         ambient = fogAmbient
//
// The quad writes the opaque layer. Whatever is not covered by 3D geometry
// therefore ends up as this fog colour, which is what makes the horizon and
// distant background look correct instead of plain black.

layout(location = 0) in vec2 vScreen;
layout(location = 0) out vec4 outColour;

layout(push_constant) uniform PushConstants
{
	// xyz = fog colour, w = scroll fog amount (rgba[3] in New3D)
	vec4 fogColour;
	// x = fog ambient, y = fog attenuation
	vec4 fogAmbientAtt;
	// Spotlight ellipse position: xy = centre, zw = size
	vec4 spotEllipse;
	vec3 spotFogColor;
} pc;

void main()
{
	// Scroll fog base colour
	vec3 lFogColor = pc.fogColour.rgb * pc.fogAmbientAtt.x;

	// Spotlight on fog (area). Note the ellipse was scaled into the viewport by
	// the scene builder, so gl_FragCoord is the right space to test it in.
	vec2 position = pc.spotEllipse.xy;
	vec2 size = pc.spotEllipse.zw;
	float ellipse = length((gl_FragCoord.xy - position) / size);
	ellipse = ellipse * ellipse;			// decay rate = square of distance from centre
	ellipse = 1.0 - ellipse;				// invert
	ellipse = max(0.0, ellipse);			// clamp

	// Spotlight on fog (colour)
	vec3 lSpotFogColor = mix(vec3(0.0), pc.spotFogColor * ellipse * pc.fogColour.rgb, pc.fogAmbientAtt.y);

	// Final colour. The alpha carries the fog amount so the 2D layer underneath
	// is dimmed by the ambient fog exactly like New3D does.
	outColour = vec4(lFogColor + lSpotFogColor, pc.fogColour.a);
}
