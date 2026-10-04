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

// The smallest box, lined up with the axes, that holds every vertex of a mesh: its lowest and its
// highest x, y and z (Unity's Mesh.bounds, which stores the center and the half-size instead). In
// the mesh's own space; a GameObject's world matrix turns it into a (possibly tilted) box in the
// world. The editor tests mouse clicks against it, and frames objects by it.
struct Bounds {
    glm::vec3 Min { 0.0f };
    glm::vec3 Max { 0.0f };

    glm::vec3 Center() const { return (Min + Max) * 0.5f; }
    glm::vec3 Size() const { return Max - Min; }
    // The box's 8 corners, numbered 0 to 7: each bit of the number picks Min or Max along one
    // axis (1: x, 2: y, 4: z). So two corners share an edge when their numbers differ in one bit.
    glm::vec3 Corner(int index) const
    {
        return { index & 1 ? Max.x : Min.x, index & 2 ? Max.y : Min.y, index & 4 ? Max.z : Min.z };
    }
};

// The bounds of `data`'s vertices (all zero for a mesh without any).
Bounds ComputeBounds(const MeshData& data);

} // namespace Viva
