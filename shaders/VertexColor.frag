#version 450

// The color the vertex shader wrote, blended between the triangle's three vertices.
layout(location = 0) in vec3 inColor;

// The pixel's color, written to the image being drawn into (attachment 0).
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(inColor, 1.0);
}
