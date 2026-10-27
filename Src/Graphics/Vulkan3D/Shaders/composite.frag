#version 450

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColour;

layout(set = 0, binding = 0) uniform sampler uSampler;
// 0 = bottom tile layer, 1 = 3D scene, 2 = top tile layer. The scene view is
// swapped in at runtime (see SetSceneColor) because it changes when the targets are recreated.
layout(set = 0, binding = 1) uniform texture2D uLayers[3];

// UpscaleMode applies to the tile layers; the output-resolution scene uses bilinear sampling.
layout(push_constant) uniform PushConstants
{
	uint layer;
	int upscaleMode;
} pc;

// Match Graphics/Shaders2D.h in the original OpenGL renderer.
vec4 biquintic(texture2D image, vec2 uv)
{
    vec2 ts = vec2(textureSize(sampler2D(image, uSampler), 0));
    uv = uv * ts + 0.5;
    vec2 i = floor(uv);
    vec2 f = uv - i;
    f = f*f*f*(f*(f*6.0-15.0)+10.0) - 0.5;
    return texture(sampler2D(image, uSampler), (i + f) / ts);
}

vec4 cubic(float v)
{
    vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;
    vec4 s = n * n * n;
    float x = s.x;
    float y = s.y - 4.0 * s.x;
    float z = s.z - 4.0 * s.y + 6.0 * s.x;
    return vec4(x, y, z, 6.0 - x - y - z) / 6.0;
}

vec4 bicubic(texture2D image, vec2 uv)
{
    vec2 invTexSize = 1.0 / vec2(textureSize(sampler2D(image, uSampler), 0));
    uv = uv * vec2(textureSize(sampler2D(image, uSampler), 0)) - 0.5;
    vec2 uvi = floor(uv);
    vec2 fxy = uv - uvi;
    vec4 xcubic = cubic(fxy.x);
    vec4 ycubic = cubic(fxy.y);
    vec4 c = uvi.xxyy + vec2(-0.5, 1.5).xyxy;
    vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);
    vec4 offset = (c + vec4(xcubic.yw, ycubic.yw) / s) * invTexSize.xxyy;
    vec4 sample0 = texture(sampler2D(image, uSampler), offset.xz);
    vec4 sample1 = texture(sampler2D(image, uSampler), offset.yz);
    vec4 sample2 = texture(sampler2D(image, uSampler), offset.xw);
    vec4 sample3 = texture(sampler2D(image, uSampler), offset.yw);
    float sx = s.x / (s.x + s.y);
    float sy = s.z / (s.z + s.w);
    return mix(mix(sample3, sample2, sx), mix(sample1, sample0, sx), sy);
}

void main()
{
    if (pc.upscaleMode == 0)
    {
        ivec2 size = textureSize(sampler2D(uLayers[pc.layer], uSampler), 0);
        ivec2 pos = clamp(ivec2(floor(vUv * vec2(size))), ivec2(0), size - 1);
        outColour = texelFetch(sampler2D(uLayers[pc.layer], uSampler), pos, 0);
    }
    else if (pc.upscaleMode == 1)
        outColour = vec4(biquintic(uLayers[pc.layer], vUv).rgb,
            texture(sampler2D(uLayers[pc.layer], uSampler), vUv).a);
    else if (pc.upscaleMode == 3)
        outColour = bicubic(uLayers[pc.layer], vUv);
    else
        outColour = texture(sampler2D(uLayers[pc.layer], uSampler), vUv);
}
