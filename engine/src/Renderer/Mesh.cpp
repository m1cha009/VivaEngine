#include "Renderer/Mesh.h"

#include "Viva/Log.h"

#include <span>

namespace Viva {

std::unique_ptr<Mesh> Mesh::Create(const VulkanContext& context, const MeshData& data)
{
    // std::as_bytes views the vertices (or indices) as the raw bytes the GPU will receive.
    auto mesh = std::make_unique<Mesh>();
    mesh->m_VertexBuffer = Buffer::CreateWithData(context, std::as_bytes(std::span(data.Vertices)),
                                                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    mesh->m_IndexBuffer = Buffer::CreateWithData(context, std::as_bytes(std::span(data.Indices)),
                                                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    mesh->m_IndexCount = static_cast<uint32_t>(data.Indices.size());
    mesh->m_Bounds = ComputeBounds(data);
    Log::Trace("Mesh uploaded: {} vertices, {} triangles ({} + {} bytes)", data.Vertices.size(),
               data.Indices.size() / 3, mesh->m_VertexBuffer->GetSize(), mesh->m_IndexBuffer->GetSize());
    return mesh;
}

void Mesh::Bind(VkCommandBuffer cmd) const
{
    // Binding 0 (see kVertexBindings) reads from our vertex buffer, starting at its first byte.
    const VkBuffer vertexBuffer = m_VertexBuffer->GetHandle();
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &offset);
    vkCmdBindIndexBuffer(cmd, m_IndexBuffer->GetHandle(), 0, VK_INDEX_TYPE_UINT32);
}

void Mesh::Draw(VkCommandBuffer cmd) const
{
    // Draw by index: the GPU walks the index buffer and fetches the vertex each index names.
    // Arguments: index count, 1 instance, starting at index 0, adding 0 to every index, instance 0.
    vkCmdDrawIndexed(cmd, m_IndexCount, 1, 0, 0, 0);
}

} // namespace Viva
