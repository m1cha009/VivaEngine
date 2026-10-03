#pragma once

#include "Renderer/DescriptorAllocator.h"
#include "Renderer/GpuResource.h"

#include <glm/vec4.hpp>
#include <vulkan/vulkan.h>

#include <memory>

namespace Viva {

class Shader;
class Texture;

// What a mesh is drawn with: a shader, a texture and a color. Like a Unity Material.
//
// Its texture reaches the fragment shader through descriptor set 1 ("layout(set = 1,
// binding = 0) uniform sampler2D albedo"), a set this material owns. Drawing with the material
// means binding that set. The color travels with each draw as a push constant.
class Material : public GpuResource {
public:
    // `layout` is set 1's layout, which every shader's pipeline layout includes.
    static std::unique_ptr<Material> Create(VkDevice device, DescriptorAllocator& descriptors,
                                            VkDescriptorSetLayout layout, std::shared_ptr<Shader> shader,
                                            std::shared_ptr<Texture> texture, const glm::vec4& color);

    explicit Material(DescriptorAllocator& descriptors); // creates nothing: use Create()
    ~Material() override;

    const Shader& GetShader() const { return *m_Shader; }
    VkDescriptorSet GetDescriptorSet() const { return m_Descriptor.Set; }
    const glm::vec4& GetColor() const { return m_Color; }

private:
    DescriptorAllocator& m_Descriptors;
    DescriptorAllocation m_Descriptor;
    // Shared: many materials can use the same shader and texture. Holding them keeps them alive
    // as long as this material.
    std::shared_ptr<Shader> m_Shader;
    std::shared_ptr<Texture> m_Texture;
    glm::vec4 m_Color { 1.0f };
};

} // namespace Viva
