#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>

namespace Viva {

class Texture;

// Descriptor set 1: the texture a draw samples, as "layout(set = 1, binding = 0) uniform
// sampler2D" in the fragment shader. A "combined image sampler" descriptor hands over both the
// image view and the sampler.
//
// Each texture gets its own set, allocated once from this class's pool. Drawing with a texture
// means binding its set; switching textures between draws means binding another set, while set 0
// (the camera) stays bound.
class TextureDescriptors {
public:
    // The most sets the pool has room for. (M8's materials will need something more flexible.)
    static constexpr uint32_t kMaxTextures = 16;

    static std::unique_ptr<TextureDescriptors> Create(VkDevice device);

    explicit TextureDescriptors(VkDevice device); // creates nothing: use Create()
    ~TextureDescriptors();

    TextureDescriptors(const TextureDescriptors&) = delete;
    TextureDescriptors& operator=(const TextureDescriptors&) = delete;

    VkDescriptorSetLayout GetLayout() const { return m_Layout; }

    // A set that points at `texture`. It lives as long as this object; the texture must too.
    VkDescriptorSet Allocate(const Texture& texture);

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_Layout = VK_NULL_HANDLE;
    VkDescriptorPool m_Pool = VK_NULL_HANDLE;
};

} // namespace Viva
