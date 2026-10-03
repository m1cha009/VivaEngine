#pragma once

#include "Renderer/Buffer.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace Viva {

class VulkanContext;

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

// How a pipeline reads Vertex out of a vertex buffer (both go into PipelineSettings).
// The binding: vertex buffer 0 holds one Vertex after another, sizeof(Vertex) bytes apart.
inline constexpr VkVertexInputBindingDescription kVertexBindings[] = {
    { .binding = 0, .stride = sizeof(Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX },
};
// The attributes: for each shader input (its location), which binding it comes from, its type
// (three or two 32-bit floats) and where it sits inside a Vertex.
inline constexpr VkVertexInputAttributeDescription kVertexAttributes[] = {
    { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, Position) },
    { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, Color) },
    { .location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, UV) },
};

// A mesh on the CPU side: its vertices, and its triangles as indices into Vertices, three per
// triangle. These are the arrays you'd assign to Unity's Mesh.vertices and Mesh.triangles.
struct MeshData {
    std::vector<Vertex> Vertices;
    std::vector<uint32_t> Indices;
};

// A mesh in GPU memory, ready to draw: a vertex buffer and an index buffer.
class Mesh {
public:
    // Uploads `data` to GPU memory. The upload has finished when this returns.
    static std::unique_ptr<Mesh> Create(const VulkanContext& context, const MeshData& data);

    Mesh() = default; // creates nothing: use Create()

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    // Binds the mesh's buffers and draws its triangles. The bound pipeline must read Vertex
    // (kVertexBindings and kVertexAttributes).
    void Draw(VkCommandBuffer cmd) const;

private:
    std::unique_ptr<Buffer> m_VertexBuffer;
    std::unique_ptr<Buffer> m_IndexBuffer;
    uint32_t m_IndexCount = 0;
};

} // namespace Viva
