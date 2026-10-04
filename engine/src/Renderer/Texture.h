#pragma once

#include "Renderer/GpuResource.h"
#include "Renderer/Image.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>

namespace Viva {

class VulkanContext;

// A texture: an image in GPU memory with a full chain of mipmaps, plus the sampler that says how
// shaders read it (filtering, repeating). Like a Unity Texture2D together with its import
// settings.
class Texture : public GpuResource {
public:
    // A texture from pixels in memory: width x height pixels of 4 bytes (red, green, blue,
    // alpha), row after row from the top-left corner. Renderer::CreateTexture checks that the
    // sizes add up before calling this, and image files are decoded before they get here.
    static std::unique_ptr<Texture> Create(const VulkanContext& context, uint32_t width, uint32_t height,
                                           std::span<const uint8_t> pixels);

    explicit Texture(VkDevice device); // creates nothing: use Create()
    ~Texture() override;

    VkImageView GetView() const { return m_Image->GetView(); }
    VkSampler GetSampler() const { return m_Sampler; }

    // See Renderer::CreateTexture.
    const std::string& GetAssetName() const { return m_AssetName; }
    void SetAssetName(std::string name) { m_AssetName = std::move(name); }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    std::unique_ptr<Image> m_Image;
    VkSampler m_Sampler = VK_NULL_HANDLE;
    std::string m_AssetName;
};

} // namespace Viva
