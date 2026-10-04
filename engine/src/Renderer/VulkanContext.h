#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <functional>
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
//   VmaAllocator      VMA, the library that hands out GPU memory to buffers (and images, M7).
class VulkanContext {
public:
    // The Vulkan version the engine is written for. The instance asks for it, a GPU must support
    // it, and libraries added later (VMA, ImGui) must be told the same version.
    static constexpr uint32_t kApiVersion = VK_API_VERSION_1_3;
    // The depth buffer's format: one 32-bit float per pixel. Picking a GPU checks it's supported.
    static constexpr VkFormat kDepthFormat = VK_FORMAT_D32_SFLOAT;

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
    VmaAllocator GetAllocator() const { return m_Allocator; }
    // The strongest anisotropic filtering samplers may use, or 0 if the GPU has none.
    float GetMaxSamplerAnisotropy() const { return m_MaxSamplerAnisotropy; }
    // Offsets into uniform buffers must be multiples of this (see FrameUniforms).
    uint32_t GetUniformBufferAlignment() const { return m_UniformBufferAlignment; }

    // Records commands with `record`, runs them on the graphics queue and waits until the GPU has
    // finished them. For one-off work while loading, like copying data into GPU memory
    // (Buffer::CreateWithData). Waiting for the GPU makes it far too slow to use every frame.
    // std::function is C++'s Action<T>: anything callable, here usually a lambda.
    void ImmediateSubmit(const std::function<void(VkCommandBuffer)>& record) const;

private:
    // The steps of Create(), in order. Each returns false (after logging why) if it fails.
    bool CreateInstance(std::vector<const char*> extensions);
    void CreateDebugMessenger();
    bool PickPhysicalDevice();
    bool CreateDevice();
    void CreateAllocator();

    VkInstance m_Instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice m_Device = VK_NULL_HANDLE;
    uint32_t m_GraphicsQueueFamily = 0;
    uint32_t m_PresentQueueFamily = 0;
    VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
    VkQueue m_PresentQueue = VK_NULL_HANDLE;
    float m_MaxSamplerAnisotropy = 0.0f;
    uint32_t m_UniformBufferAlignment = 256;
    VmaAllocator m_Allocator = VK_NULL_HANDLE;
};

} // namespace Viva
