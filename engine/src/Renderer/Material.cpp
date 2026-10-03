#include "Renderer/Material.h"

#include "Renderer/Texture.h"
#include "Renderer/VulkanHelpers.h"

#include <utility>

namespace Viva {

std::unique_ptr<Material> Material::Create(VkDevice device, DescriptorAllocator& descriptors,
                                           VkDescriptorSetLayout layout, std::shared_ptr<Shader> shader,
                                           std::shared_ptr<Texture> texture, const glm::vec4& color)
{
    auto material = std::make_unique<Material>(descriptors);
    material->m_Descriptor = descriptors.Allocate(layout);
    // Texture leaves every mip level in SHADER_READ_ONLY_OPTIMAL, the layout the shader samples.
    WriteImageDescriptor(device, material->m_Descriptor.Set, 0, texture->GetView(), texture->GetSampler(),
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    material->m_Shader = std::move(shader);
    material->m_Texture = std::move(texture);
    material->m_Color = color;
    return material;
}

Material::Material(DescriptorAllocator& descriptors)
    : m_Descriptors(descriptors)
{
}

Material::~Material()
{
    if (m_Descriptor.Set)
        m_Descriptors.Free(m_Descriptor);
}

} // namespace Viva
