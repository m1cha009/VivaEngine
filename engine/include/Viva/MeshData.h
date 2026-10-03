#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <vector>

namespace Viva {

// One vertex, laid out exactly as a vertex buffer stores it and the vertex shader reads it
// (shaders/Unlit.vert: "layout(location = 0) in vec3 inPosition", location 1 the color,
// location 2 the texture coordinates).
struct Vertex {
    glm::vec3 Position;
    // Multiplied with the texture's color: white shows the texture as it is.
    glm::vec3 Color;
    // Texture coordinates ("UV"): which point of the texture this vertex shows. (0, 0) is the
    // texture's top-left corner and (1, 1) its bottom-right; values past 1 repeat it.
    glm::vec2 UV;
};

// A mesh on the CPU side: its vertices, and its triangles as indices into Vertices, three per
// triangle. These are the arrays you'd assign to Unity's Mesh.vertices and Mesh.triangles.
// A triangle's front is the side from which its corners appear counter-clockwise.
struct MeshData {
    std::vector<Vertex> Vertices;
    std::vector<uint32_t> Indices;
};

} // namespace Viva
