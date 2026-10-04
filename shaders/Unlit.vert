#version 450

// Draws textured meshes without lighting, seen through the camera (like Unity's Unlit/Texture
// shader, tinted by the vertex colors).

// Per frame, shared by every draw: the camera's matrices, in a uniform buffer that descriptor
// set 0, binding 0 points at (engine/src/Renderer/FrameUniforms.h has the matching C++ struct).
layout(set = 0, binding = 0) uniform Camera {
    mat4 View;
    mat4 Projection;
} camera;

// Per draw, pushed right before each draw call: the object's model matrix, and its material's
// color and texture tiling (xy) and offset (zw) (ObjectPushConstants in
// engine/src/Renderer/Renderer.cpp).
layout(push_constant) uniform Object {
    mat4 Model;
    vec4 Color;
    vec4 TilingOffset;
} object;

// Per vertex, from the vertex buffer: these inputs match kVertexAttributes in
// engine/src/Renderer/Mesh.h (location 0 = Position, 1 = Color, 2 = UV).
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;

// Passed on to the fragment shader, blended across each triangle on the way.
layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outUV;

void main()
{
    // Read right to left: the model matrix places the vertex in the world, the view matrix moves
    // the world in front of the camera, and the projection adds perspective and maps the result
    // into clip space. The parentheses make each step a matrix times a vector; without them GLSL
    // would multiply the matrices together first, for every vertex, which is three times the work.
    gl_Position = camera.Projection * (camera.View * (object.Model * vec4(inPosition, 1.0)));
    // The material's color tints the vertex color; the fragment shader multiplies in the texture.
    outColor = inColor * object.Color.rgb;
    // Tiling 2 shows the texture twice across the mesh: the UVs run 0..2 instead of 0..1, and the
    // sampler repeats the texture past 1 (M7).
    outUV = inUV * object.TilingOffset.xy + object.TilingOffset.zw;
}
