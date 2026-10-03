#include "Renderer/VulkanContext.h"

#include "Platform/Window.h"
#include "Renderer/VulkanCheck.h"
#include "Renderer/VulkanVersion.h"
#include "Viva/Log.h"
#include "Viva/Version.h"

// vulkan.h only includes vulkan_beta.h (extensions that are still provisional) when
// VK_ENABLE_BETA_EXTENSIONS is defined. We only need the portability subset's name from it.
#include <vulkan/vulkan_beta.h>

#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

namespace Viva {

namespace {

constexpr const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";

// The validation layer checks every Vulkan call against the spec. It's slow, so only Debug builds
// use it. A constant (rather than an #if around the code) keeps the code compiled in both builds.
#if defined(VIVA_DEBUG)
constexpr bool kEnableValidation = true;
#else
constexpr bool kEnableValidation = false;
#endif

// The validation layer and the loader call this for every message, on whichever thread made the
// Vulkan call. VKAPI_ATTR and VKAPI_CALL give the function the calling convention Vulkan expects.
VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                             VkDebugUtilsMessageTypeFlagsEXT /*types*/,
                                             const VkDebugUtilsMessengerCallbackDataEXT* data, void* /*userData*/)
{
    // The severities are single bits ordered VERBOSE < INFO < WARNING < ERROR, so >= works.
    Log::Level level = severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT     ? Log::Level::Error
                     : severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ? Log::Level::Warn
                                                                                    : Log::Level::Info;

    // The loader's own warnings describe how Vulkan is installed on this machine, for example an
    // overlay layer from another program that was built for an older Vulkan version. They're
    // worth seeing but aren't about our code, so they're logged as Info. The validation layer's
    // messages, which are about our code, keep their severity.
    const bool fromLoader = data->pMessageIdName && std::string_view(data->pMessageIdName) == "Loader Message";
    if (fromLoader && level == Log::Level::Warn)
        level = Log::Level::Info;

    Log::Message(level, "[Vulkan{}] {}", fromLoader ? " loader" : "", data->pMessage);

    // VK_FALSE: let the Vulkan call that caused the message carry on as usual.
    return VK_FALSE;
}

// Designated initializers (.sType = ...) name the fields being set, like a C# object initializer.
// Every field that isn't named is zero, which for Vulkan structs means "not used".
VkDebugUtilsMessengerCreateInfoEXT DebugMessengerInfo()
{
    return {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = DebugCallback,
    };
}

// A lambda is an inline function, like a C# lambda. [name] "captures" name for use inside it.
bool HasExtension(const std::vector<VkExtensionProperties>& extensions, std::string_view name)
{
    return std::ranges::any_of(extensions, [name](const VkExtensionProperties& e) { return name == e.extensionName; });
}

// Vulkan's two-call pattern: ask how many items there are, make room, then ask for the items.
// With a layer name it lists the extensions that layer provides; without, the global ones.
std::vector<VkExtensionProperties> GetInstanceExtensions(const char* layer = nullptr)
{
    uint32_t count = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(layer, &count, nullptr));
    std::vector<VkExtensionProperties> extensions(count);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(layer, &count, extensions.data()));
    return extensions;
}

std::vector<VkExtensionProperties> GetDeviceExtensions(VkPhysicalDevice device)
{
    uint32_t count = 0;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr));
    std::vector<VkExtensionProperties> extensions(count);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data()));
    return extensions;
}

bool HasValidationLayer()
{
    uint32_t count = 0;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&count, nullptr));
    std::vector<VkLayerProperties> layers(count);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&count, layers.data()));
    return std::ranges::any_of(layers, [](const VkLayerProperties& layer) {
        return std::string_view(layer.layerName) == kValidationLayer;
    });
}

std::string_view DeviceTypeName(VkPhysicalDeviceType type)
{
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return "discrete GPU";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated GPU";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return "virtual GPU";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:            return "CPU";
    default:                                     return "other";
    }
}

// Higher is better: a discrete GPU (its own chip and memory) beats one built into the CPU.
int DeviceScore(VkPhysicalDeviceType type)
{
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return 3;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 2;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return 1;
    default:                                     return 0;
    }
}

// What we found out about a GPU while checking it.
struct DeviceInfo {
    VkPhysicalDevice Device = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties Properties {};
    uint32_t GraphicsQueueFamily = 0;
    uint32_t PresentQueueFamily = 0;
    std::string_view Problem; // why the engine can't use it; empty if it can
};

// Checks everything the engine needs from a GPU, finding its queue families on the way.
DeviceInfo CheckDevice(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    DeviceInfo info { .Device = device };
    vkGetPhysicalDeviceProperties(device, &info.Properties);

    // Records why this GPU can't be used, so each check below fits in one line.
    auto fail = [&info](std::string_view problem) {
        info.Problem = problem;
        return info;
    };

    if (info.Properties.apiVersion < VulkanContext::kApiVersion)
        return fail("needs Vulkan 1.3");

    // Queue families are groups of queues with the same abilities. We need one that can run
    // drawing commands (graphics) and one that can show images on our surface (present).
    // Usually one family does both, and that's preferred.
    uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());

    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;
    for (uint32_t i = 0; i < familyCount; ++i) {
        VkBool32 canPresent = VK_FALSE;
        VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &canPresent));
        const bool canDraw = (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
        if (canDraw && canPresent) {
            graphics = i;
            present = i;
            break;
        }
        if (canDraw && !graphics)
            graphics = i;
        if (canPresent && !present)
            present = i;
    }
    if (!graphics || !present)
        return fail("no queue family can draw and present");
    info.GraphicsQueueFamily = *graphics;
    info.PresentQueueFamily = *present;

    // Device extensions: the swapchain (M3) is how a GPU shows images in a window.
    if (!HasExtension(GetDeviceExtensions(device), VK_KHR_SWAPCHAIN_EXTENSION_NAME))
        return fail("no swapchain support");

    // Features: optional abilities, asked for with a "pNext chain". Vulkan extends its structs by
    // linking extra ones through pNext, each identified by its sType; here the Vulkan 1.3
    // features ride along with the basic features query.
    VkPhysicalDeviceVulkan13Features features13 { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    VkPhysicalDeviceFeatures2 features { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &features13 };
    vkGetPhysicalDeviceFeatures2(device, &features);
    if (!features13.dynamicRendering || !features13.synchronization2)
        return fail("no dynamic rendering or synchronization2");

    return info;
}

// Memory properties as short names, like "DEVICE_LOCAL | HOST_VISIBLE" (Buffer.cpp explains them).
std::string MemoryPropertyNames(VkMemoryPropertyFlags flags)
{
    constexpr std::pair<VkMemoryPropertyFlags, std::string_view> kNames[] = {
        { VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "DEVICE_LOCAL" },
        { VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, "HOST_VISIBLE" },
        { VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "HOST_COHERENT" },
        { VK_MEMORY_PROPERTY_HOST_CACHED_BIT, "HOST_CACHED" },
        { VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, "LAZILY_ALLOCATED" },
    };
    std::string names;
    for (const auto& [bit, name] : kNames) {
        if ((flags & bit) == 0)
            continue;
        if (!names.empty())
            names += " | ";
        names += name;
    }
    return names.empty() ? "no special properties" : names;
}

// Lists the GPU's memory: its heaps (physical pools of memory, like VRAM and system RAM) and the
// memory types in each (the ways that memory can be used).
void LogMemoryHeaps(VkPhysicalDevice device)
{
    VkPhysicalDeviceMemoryProperties memory {};
    vkGetPhysicalDeviceMemoryProperties(device, &memory);
    for (uint32_t heap = 0; heap < memory.memoryHeapCount; ++heap) {
        const double gib = static_cast<double>(memory.memoryHeaps[heap].size) / (1024.0 * 1024.0 * 1024.0);
        const bool onGpu = (memory.memoryHeaps[heap].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0;
        Log::Trace("Memory heap {}: {:.1f} GiB{}", heap, gib, onGpu ? ", on the GPU" : "");
        for (uint32_t type = 0; type < memory.memoryTypeCount; ++type) {
            if (memory.memoryTypes[type].heapIndex == heap)
                Log::Trace("  memory type {}: {}", type, MemoryPropertyNames(memory.memoryTypes[type].propertyFlags));
        }
    }
}

} // namespace

std::unique_ptr<VulkanContext> VulkanContext::Create(const Window& window)
{
    auto context = std::make_unique<VulkanContext>();

    // Each step builds on the ones before it. If one fails, returning destroys "context", and its
    // destructor cleans up whatever was created so far.
    if (!context->CreateInstance(window.GetRequiredVulkanExtensions()))
        return nullptr;

    context->m_Surface = window.CreateVulkanSurface(context->m_Instance);
    if (!context->m_Surface)
        return nullptr;

    if (!context->PickPhysicalDevice() || !context->CreateDevice())
        return nullptr;
    return context;
}

// The destructor undoes whatever was created, newest first: each object may depend on the ones
// made before it (the device on the instance, the surface on the instance, and so on).
VulkanContext::~VulkanContext()
{
    if (m_Device)
        vkDestroyDevice(m_Device, nullptr);
    if (m_Surface)
        vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
    if (m_DebugMessenger) {
        auto destroyMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_Instance, "vkDestroyDebugUtilsMessengerEXT"));
        destroyMessenger(m_Instance, m_DebugMessenger, nullptr);
    }
    if (m_Instance)
        vkDestroyInstance(m_Instance, nullptr);
}

bool VulkanContext::CreateInstance(std::vector<const char*> extensions)
{
    const std::vector<VkExtensionProperties> available = GetInstanceExtensions();
    std::vector<const char*> layers;
    VkInstanceCreateFlags flags = 0;

    // On macOS, Vulkan runs on top of Metal through a "portability" driver (MoltenVK or
    // KosmicKrisp). The loader only lists such drivers if the application asks for them like this.
    if (HasExtension(available, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }

    bool validation = false;
    bool syncValidation = false;
    if (kEnableValidation) {
        if (HasValidationLayer() && HasExtension(available, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
            layers.push_back(kValidationLayer);
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            validation = true;
            // The layer's own extension for configuring it from code, used below to turn on
            // synchronization validation.
            if (HasExtension(GetInstanceExtensions(kValidationLayer), VK_EXT_LAYER_SETTINGS_EXTENSION_NAME)) {
                extensions.push_back(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME);
                syncValidation = true;
            }
            Log::Info("Vulkan validation layer enabled{}", syncValidation ? ", with synchronization validation" : "");
        } else {
            Log::Warn("Vulkan validation layer not found, running without it (it comes with the Vulkan SDK)");
        }
    }

    const Version engine = GetEngineVersion();
    const VkApplicationInfo appInfo {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pEngineName = "VivaEngine",
        .engineVersion = VK_MAKE_API_VERSION(0, engine.Major, engine.Minor, engine.Patch),
        .apiVersion = kApiVersion,
    };

    // Giving vkCreateInstance the messenger's settings too means problems in vkCreateInstance and
    // vkDestroyInstance get reported, when the real messenger doesn't exist (yet, or anymore).
    VkDebugUtilsMessengerCreateInfoEXT debugInfo = DebugMessengerInfo();

    // Synchronization validation, which is off by default: the layer then also checks that our
    // barriers and semaphores order the GPU's reads and writes correctly. These settings hang off
    // the messenger settings in the same pNext chain (the order in a chain doesn't matter: each
    // struct is found by its sType).
    const VkBool32 enabled = VK_TRUE;
    const VkLayerSettingEXT syncSetting {
        .pLayerName = kValidationLayer,
        .pSettingName = "validate_sync",
        .type = VK_LAYER_SETTING_TYPE_BOOL32_EXT,
        .valueCount = 1,
        .pValues = &enabled,
    };
    const VkLayerSettingsCreateInfoEXT layerSettings {
        .sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT,
        .settingCount = 1,
        .pSettings = &syncSetting,
    };
    if (syncValidation)
        debugInfo.pNext = &layerSettings;

    const VkInstanceCreateInfo createInfo {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = validation ? &debugInfo : nullptr,
        .flags = flags,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(layers.size()),
        .ppEnabledLayerNames = layers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };

    const VkResult result = vkCreateInstance(&createInfo, nullptr, &m_Instance);
    if (result != VK_SUCCESS) {
        Log::Error("Couldn't create the Vulkan instance: {}", VkResultName(result));
        return false;
    }

    if (validation)
        CreateDebugMessenger();
    return true;
}

void VulkanContext::CreateDebugMessenger()
{
    // The loader exports the core Vulkan functions directly, but extension functions have to be
    // looked up by name. reinterpret_cast tells the compiler "treat this pointer as that type":
    // it trusts us completely, so it's only used where an API requires it, like here.
    auto createMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_Instance, "vkCreateDebugUtilsMessengerEXT"));
    const VkDebugUtilsMessengerCreateInfoEXT info = DebugMessengerInfo();
    VK_CHECK(createMessenger(m_Instance, &info, nullptr, &m_DebugMessenger));
}

bool VulkanContext::PickPhysicalDevice()
{
    // List every GPU the loader found, and keep the best one that has everything we need.
    uint32_t count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(m_Instance, &count, nullptr));
    std::vector<VkPhysicalDevice> devices(count);
    VK_CHECK(vkEnumeratePhysicalDevices(m_Instance, &count, devices.data()));
    Log::Info("Vulkan found {} GPU(s):", count);

    std::optional<DeviceInfo> best;
    for (VkPhysicalDevice device : devices) {
        const DeviceInfo info = CheckDevice(device, m_Surface);
        const std::string_view name = info.Properties.deviceName;
        const std::string_view type = DeviceTypeName(info.Properties.deviceType);
        if (!info.Problem.empty()) {
            Log::Info("  {} ({}): skipped, {}", name, type, info.Problem);
            continue;
        }
        Log::Info("  {} ({}): suitable", name, type);
        if (!best || DeviceScore(info.Properties.deviceType) > DeviceScore(best->Properties.deviceType))
            best = info;
    }

    if (!best) {
        Log::Error("No suitable GPU found (see the reasons above)");
        return false;
    }
    m_PhysicalDevice = best->Device;
    m_GraphicsQueueFamily = best->GraphicsQueueFamily;
    m_PresentQueueFamily = best->PresentQueueFamily;

    // The driver's name and version come from the Vulkan 1.2 properties, another pNext chain.
    VkPhysicalDeviceVulkan12Properties properties12 { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES };
    VkPhysicalDeviceProperties2 properties { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &properties12 };
    vkGetPhysicalDeviceProperties2(m_PhysicalDevice, &properties);
    Log::Info("Using {} ({}), Vulkan {}, driver {} {}", properties.properties.deviceName,
              DeviceTypeName(properties.properties.deviceType),
              FromVulkanVersion(properties.properties.apiVersion).ToString(), properties12.driverName,
              properties12.driverInfo);
    LogMemoryHeaps(m_PhysicalDevice);
    return true;
}

bool VulkanContext::CreateDevice()
{
    // One queue from each family we use. The std::set drops the duplicate when graphics and
    // present are the same family, which is the usual case.
    const std::set<uint32_t> families = { m_GraphicsQueueFamily, m_PresentQueueFamily };
    const float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    for (uint32_t family : families) {
        queueInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = family,
            .queueCount = 1,
            .pQueuePriorities = &priority,
        });
    }

    std::vector<const char*> extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    // On macOS, a driver that lists the portability subset must have it enabled. It marks the few
    // Vulkan features Metal can't fully provide.
    if (HasExtension(GetDeviceExtensions(m_PhysicalDevice), VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
        extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);

    // Turn on the Vulkan 1.3 features the renderer uses. Being "core" in 1.3 means every 1.3 GPU
    // supports them, but like every feature they still have to be switched on.
    VkPhysicalDeviceVulkan13Features features13 {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };
    const VkPhysicalDeviceFeatures2 features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &features13,
    };

    const VkDeviceCreateInfo createInfo {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features,
        .queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size()),
        .pQueueCreateInfos = queueInfos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };

    const VkResult result = vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device);
    if (result != VK_SUCCESS) {
        Log::Error("Couldn't create the Vulkan device: {}", VkResultName(result));
        return false;
    }
    vkGetDeviceQueue(m_Device, m_GraphicsQueueFamily, 0, &m_GraphicsQueue);
    vkGetDeviceQueue(m_Device, m_PresentQueueFamily, 0, &m_PresentQueue);
    return true;
}

void VulkanContext::ImmediateSubmit(const std::function<void(VkCommandBuffer)>& record) const
{
    // A command pool and buffer just for this call. Creating them is cheap next to waiting for
    // the GPU, and nothing is left over between calls. TRANSIENT: its command buffers are
    // short-lived.
    const VkCommandPoolCreateInfo poolInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = m_GraphicsQueueFamily,
    };
    VkCommandPool pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateCommandPool(m_Device, &poolInfo, nullptr, &pool));

    const VkCommandBufferAllocateInfo allocateInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(m_Device, &allocateInfo, &cmd));

    const VkCommandBufferBeginInfo beginInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));
    record(cmd);
    VK_CHECK(vkEndCommandBuffer(cmd));

    // Submit, with a fence the GPU signals when it's done, and wait for it.
    const VkFenceCreateInfo fenceInfo { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkFence fence = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFence(m_Device, &fenceInfo, nullptr, &fence));
    const VkCommandBufferSubmitInfo commandInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmd,
    };
    const VkSubmitInfo2 submitInfo {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &commandInfo,
    };
    VK_CHECK(vkQueueSubmit2(m_GraphicsQueue, 1, &submitInfo, fence));
    VK_CHECK(vkWaitForFences(m_Device, 1, &fence, VK_TRUE, UINT64_MAX));

    vkDestroyFence(m_Device, fence, nullptr);
    vkDestroyCommandPool(m_Device, pool, nullptr); // frees its command buffer too
}

} // namespace Viva
