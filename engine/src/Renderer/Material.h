#pragma once

#include "Renderer/DescriptorAllocator.h"
#include "Renderer/GpuResource.h"
#include "Viva/Renderer.h"

#include <vulkan/vulkan.h>

#include <memory>

namespace Viva {

class Shader;
class Texture;

// What a mesh is drawn with: a shader, a texture, a color, and the texture's tiling and offset.
// Like a Unity Material.
//
// Its texture reaches the fragment shader through descriptor set 1 ("layout(set = 1,
// binding = 0) uniform sampler2D albedo"), a set this material owns. Drawing with the material
// means binding that set. The color, tiling and offset travel with each draw as push constants.
class Material : public GpuResource {
public:
    // `layout` is set 1's layout, which every shader's pipeline layout includes.
    // `texture` is the one the shader samples: settings.Texture, or a white one if that's null.
    static std::unique_ptr<Material> Create(VkDevice device, DescriptorAllocator& descriptors,
                                            VkDescriptorSetLayout layout, std::shared_ptr<Shader> shader,
                                            std::shared_ptr<Texture> texture, const MaterialSettings& settings);

    explicit Material(DescriptorAllocator& descriptors); // creates nothing: use Create()
    ~Material() override;

    const Shader& GetShader() const { return *m_Shader; }
    VkDescriptorSet GetDescriptorSet() const { return m_Descriptor.Set; }
    // As the game created it (the texture may be null).
    const MaterialSettings& GetSettings() const { return m_Settings; }

private:
    DescriptorAllocator& m_Descriptors;
    DescriptorAllocation m_Descriptor;
    // Shared: many materials can use the same shader and texture. Holding them keeps them alive
    // as long as this material.
    std::shared_ptr<Shader> m_Shader;
    std::shared_ptr<Texture> m_Texture;
    MaterialSettings m_Settings;
};

} // namespace Viva
