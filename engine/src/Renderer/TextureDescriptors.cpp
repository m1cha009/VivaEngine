#include "Renderer/TextureDescriptors.h"

#include "Renderer/Texture.h"
#include "Renderer/VulkanHelpers.h"

namespace Viva {

std::unique_ptr<TextureDescriptors> TextureDescriptors::Create(VkDevice device)
{
    auto descriptors = std::make_unique<TextureDescriptors>(device);

    // Binding 0: one combined image sampler, read by the fragment shader.
    const VkDescriptorSetLayoutBinding bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };
    descriptors->m_Layout = CreateDescriptorSetLayout(device, bindings);

    const VkDescriptorPoolSize poolSizes[] = {
        { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = kMaxTextures },
    };
    descriptors->m_Pool = CreateDescriptorPool(device, kMaxTextures, poolSizes);
    return descriptors;
}

TextureDescriptors::TextureDescriptors(VkDevice device)
    : m_Device(device)
{
}

TextureDescriptors::~TextureDescriptors()
{
    // Destroying the pool frees every set allocated from it.
    vkDestroyDescriptorPool(m_Device, m_Pool, nullptr);
    vkDestroyDescriptorSetLayout(m_Device, m_Layout, nullptr);
}

VkDescriptorSet TextureDescriptors::Allocate(const Texture& texture)
{
    const VkDescriptorSet set = AllocateDescriptorSet(m_Device, m_Pool, m_Layout);
    // Texture leaves every mip level in SHADER_READ_ONLY_OPTIMAL, the layout the shader samples.
    WriteImageDescriptor(m_Device, set, 0, texture.GetView(), texture.GetSampler(),
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return set;
}

} // namespace Viva
