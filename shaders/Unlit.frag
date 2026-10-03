#version 450

// Per material: the texture to show, with its sampler. Descriptor set 1, binding 0, which each
// Material owns (engine/src/Renderer/Material.h).
layout(set = 1, binding = 0) uniform sampler2D albedo;

// From the vertex shader, blended across the triangle.
layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 inUV;

// The pixel's color, written to the image being drawn into (attachment 0).
layout(location = 0) out vec4 outColor;

void main()
{
    // texture() reads the texture at this pixel's UV: the sampler blends the nearest texels and
    // mip levels. The vertex color tints the result; white leaves it unchanged.
    outColor = texture(albedo, inUV) * vec4(inColor, 1.0);
}
