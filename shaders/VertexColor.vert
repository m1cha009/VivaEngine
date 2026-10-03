#version 450

// Draws meshes colored by their vertices. Each vertex comes from the vertex buffer: these inputs
// match kVertexAttributes in engine/src/Renderer/Mesh.h (location 0 = Position, 1 = Color).
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

// Passed on to the fragment shader, blended across each triangle on the way.
layout(location = 0) out vec3 outColor;

void main()
{
    // The positions are already in clip space (x and y from -1 to 1, y pointing down). M6 adds
    // the camera and projection matrices that take a mesh from world space to here.
    gl_Position = vec4(inPosition, 1.0);
    outColor = inColor;
}
