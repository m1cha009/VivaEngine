#version 450

// Draws meshes colored by their vertices, seen through the camera.

// Per frame, shared by every draw: the camera's matrices, in a uniform buffer that descriptor
// set 0, binding 0 points at (engine/src/Renderer/FrameUniforms.h has the matching C++ struct).
layout(set = 0, binding = 0) uniform Camera {
    mat4 View;
    mat4 Projection;
} camera;

// Per draw: the object's model matrix, pushed right before each draw call.
layout(push_constant) uniform Object {
    mat4 Model;
} object;

// Per vertex, from the vertex buffer: these inputs match kVertexAttributes in
// engine/src/Renderer/Mesh.h (location 0 = Position, 1 = Color).
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

// Passed on to the fragment shader, blended across each triangle on the way.
layout(location = 0) out vec3 outColor;

void main()
{
    // Read right to left: the model matrix places the vertex in the world, the view matrix moves
    // the world in front of the camera, and the projection adds perspective and maps the result
    // into clip space. The parentheses make each step a matrix times a vector; without them GLSL
    // would multiply the matrices together first, for every vertex, which is three times the work.
    gl_Position = camera.Projection * (camera.View * (object.Model * vec4(inPosition, 1.0)));
    outColor = inColor;
}
