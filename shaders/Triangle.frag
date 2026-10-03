#version 450

// The fragment shader runs once per pixel the triangle covers, and decides its color.

// The vertex shader's outColor, already blended for this pixel.
layout(location = 0) in vec3 inColor;

// What we write into the image being rendered (color attachment 0).
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(inColor, 1.0);
}
