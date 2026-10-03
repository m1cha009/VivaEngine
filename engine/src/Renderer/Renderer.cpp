#include "Renderer/Renderer.h"

#include "Renderer/VulkanContext.h"

#include <utility>

namespace Viva {

std::unique_ptr<Renderer> Renderer::Create(Window& window)
{
    std::unique_ptr<VulkanContext> context = VulkanContext::Create(window);
    if (!context)
        return nullptr;
    // A unique_ptr can't be copied, only moved: std::move hands the VulkanContext over to the
    // Renderer, and "context" is left empty.
    return std::make_unique<Renderer>(std::move(context));
}

Renderer::Renderer(std::unique_ptr<VulkanContext> context)
    : m_Context(std::move(context))
{
}

// Defined here, where VulkanContext is a complete type, for the same reason as ~Application().
Renderer::~Renderer() = default;

} // namespace Viva
