#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace Viva {

class Window;

// The Vulkan objects that everything else is built on. They're created once at startup and
// destroyed in reverse order at shutdown:
//
//   VkInstance        The engine's connection to Vulkan: the loader, plus the drivers it found.
//   Debug messenger   Debug builds only: passes the validation layer's messages to Log.
//   VkSurfaceKHR      The window, seen as something Vulkan can show images on.
//   VkPhysicalDevice  The GPU we chose. It isn't created, only picked from a list.
//   VkDevice          Our logical connection to that GPU, with the features we turned on.
//   VkQueue           Where work is submitted: command buffers to run, images to present.
class VulkanContext {
public:
    // The Vulkan version the engine is written for. The instance asks for it, a GPU must support
    // it, and libraries added later (VMA, ImGui) must be told the same version.
    static constexpr uint32_t kApiVersion = VK_API_VERSION_1_3;

    // Creates all of the above, or returns nullptr (after logging why) if this machine can't run
    // the engine, for example because no GPU supports Vulkan 1.3.
    static std::unique_ptr<VulkanContext> Create(const Window& window);

    VulkanContext() = default; // creates nothing: use Create()
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    VkInstance GetInstance() const { return m_Instance; }
    VkSurfaceKHR GetSurface() const { return m_Surface; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
    VkDevice GetDevice() const { return m_Device; }
    uint32_t GetGraphicsQueueFamily() const { return m_GraphicsQueueFamily; }
    VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
    uint32_t GetPresentQueueFamily() const { return m_PresentQueueFamily; }
    VkQueue GetPresentQueue() const { return m_PresentQueue; }

private:
    // The steps of Create(), in order. Each returns false (after logging why) if it fails.
    bool CreateInstance(std::vector<const char*> extensions);
    void CreateDebugMessenger();
    bool PickPhysicalDevice();
    bool CreateDevice();

    VkInstance m_Instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice m_Device = VK_NULL_HANDLE;
    uint32_t m_GraphicsQueueFamily = 0;
    uint32_t m_PresentQueueFamily = 0;
    VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
    VkQueue m_PresentQueue = VK_NULL_HANDLE;
};

} // namespace Viva
