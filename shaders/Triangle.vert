#version 450

// The vertex shader runs once per vertex and says where that vertex lands on screen.
//
// For now the triangle's three corners are written right here in the shader; M5 moves them
// into a vertex buffer. Vulkan's clip space has x pointing right and y pointing DOWN, with the
// window's center at (0, 0) and its edges at -1 and 1, so (0, -0.5) is the top corner.
const vec2 kPositions[3] = vec2[](vec2(0.0, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5));
const vec3 kColors[3] = vec3[](vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0));

// An output to the fragment shader. The GPU blends it across the triangle, so a pixel between
// a red and a green corner gets a mix of both.
layout(location = 0) out vec3 outColor;

void main()
{
    // gl_VertexIndex counts 0, 1, 2 for a draw of three vertices.
    gl_Position = vec4(kPositions[gl_VertexIndex], 0.0, 1.0);
    outColor = kColors[gl_VertexIndex];
}
