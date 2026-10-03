#pragma once

#include "Renderer/Buffer.h"
#include "Renderer/GpuResource.h"
#include "Viva/MeshData.h"

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace Viva {

class VulkanContext;

// How a pipeline reads Vertex (Viva/MeshData.h) out of a vertex buffer (both go into
// PipelineSettings).
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

// A mesh in GPU memory, ready to draw: a vertex buffer and an index buffer.
class Mesh : public GpuResource {
public:
    // Uploads `data` to GPU memory. The upload has finished when this returns.
    static std::unique_ptr<Mesh> Create(const VulkanContext& context, const MeshData& data);

    Mesh() = default; // creates nothing: use Create()

    // Binds the mesh's vertex and index buffers. Draws of the same mesh in a row share one Bind.
    void Bind(VkCommandBuffer cmd) const;
    // Draws all its triangles. The mesh must be bound, and so must a pipeline that reads Vertex.
    void Draw(VkCommandBuffer cmd) const;

    // Three per triangle.
    uint32_t GetIndexCount() const { return m_IndexCount; }

private:
    std::unique_ptr<Buffer> m_VertexBuffer;
    std::unique_ptr<Buffer> m_IndexBuffer;
    uint32_t m_IndexCount = 0;
};

} // namespace Viva
