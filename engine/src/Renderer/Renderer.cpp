#include "Viva/Renderer.h"

#include "Platform/Window.h"
#include "Renderer/DescriptorAllocator.h"
#include "Renderer/FrameResources.h"
#include "Renderer/FrameUniforms.h"
#include "Renderer/GpuResource.h"
#include "Renderer/Image.h"
#include "Renderer/ImGuiRenderer.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/Pipeline.h"
#include "Renderer/Shader.h"
#include "Renderer/Swapchain.h"
#include "Renderer/Texture.h"
#include "Renderer/VulkanCheck.h"
#include "Renderer/VulkanContext.h"
#include "Renderer/VulkanHelpers.h"
#include "Viva/Assert.h"
#include "Viva/Log.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iterator>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace Viva {

namespace {

// The per-draw data, pushed with vkCmdPushConstants right before each draw, laid out like
// "push_constant uniform Object" in the shaders: the object's model matrix, then its material's
// color, then its material's texture tiling (xy) and offset (zw). Push constants are the quickest
// way to hand a draw a little data; every GPU takes at least 128 bytes of them, and this is 96.
struct ObjectPushConstants {
    glm::mat4 Model;
    glm::vec4 Color;
    glm::vec4 TilingOffset;
};
constexpr VkPushConstantRange kPushConstantRange {
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    .offset = 0,
    .size = sizeof(ObjectPushConstants),
};

// One Submit: what to draw, with what, and where. Plain pointers are safe here: Submit only
// works while a frame is being built, and anything released during the frame waits in a release
// list until the GPU is done with it.
struct DrawCommand {
    const Viva::Shader* Shader = nullptr;
    const Viva::Material* Material = nullptr;
    const Viva::Mesh* Mesh = nullptr;
    glm::mat4 Transform { 1.0f };
};

} // namespace

// The renderer's state and logic, hidden from the public header (see Renderer.h). Renderer's
// public functions just forward here.
struct Renderer::Impl {
public:
    Impl(const Window& window, bool vsync);
    ~Impl();

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    // The steps of Renderer::Create. Returns false (after logging why) if one fails.
    bool Initialize();

    std::shared_ptr<Mesh> CreateMesh(const MeshData& data, std::string assetName);
    std::shared_ptr<Texture> CreateTexture(uint32_t width, uint32_t height, std::span<const uint8_t> pixels,
                                           std::string assetName);
    std::shared_ptr<Material> CreateMaterial(const MaterialSettings& settings);
    void Submit(const Mesh& mesh, const Material& material, const glm::mat4& transform);
    const RenderStats& GetStats() const { return m_Stats; }
    void SetVSync(bool enabled);
    bool IsVSync() const { return m_VSync; }
    void SetCamera(const glm::mat4& view, const glm::mat4& projection);
    void SetClearColor(const glm::vec3& color) { m_ClearColor = color; }
    float GetAspectRatio() const;
    bool BeginFrame();
    void EndFrame();

private:
    std::shared_ptr<Shader> LoadShader(const std::string& name);
    bool RecreateSwapchain();
    void RecordDraws(VkCommandBuffer cmd);
    void UpdateMemoryStats();
    static void DestroyReleased(FrameData& frame);

    // Hands a new resource to the game as a std::shared_ptr. When the game lets go of the last
    // reference, the shared_ptr's deleter runs; instead of deleting, it parks the resource in a
    // frame's release list, to be destroyed once the GPU is done with that frame.
    template<typename T>
    std::shared_ptr<T> Track(std::unique_ptr<T> resource)
    {
        if (!resource)
            return nullptr;
        ++m_LiveResources;
        return std::shared_ptr<T>(resource.release(), [this](T* released) {
            --m_LiveResources;
            std::unique_ptr<GpuResource> owned(released);
            // Parked in the list of the newest frame that may still draw with it: the one being
            // built, or the one just submitted. Shutting down, the GPU is idle, so `owned`
            // destroys it right here instead.
            if (!m_DestroyNow)
                m_Frames->GetFrame(m_ReleaseSlot).ReleasedResources.push_back(std::move(owned));
        });
    }

    // A reference member: another name for the Window that Application owns, which outlives us.
    const Window& m_Window;
    bool m_VSync = true;

    // Declared in creation order, so they're destroyed in reverse: the context (with the device
    // everything else was made from) last.
    std::unique_ptr<VulkanContext> m_Context;
    std::unique_ptr<DescriptorAllocator> m_Descriptors;
    std::unique_ptr<FrameResources> m_Frames;
    std::unique_ptr<FrameUniforms> m_Uniforms;                 // descriptor set 0: the camera
    VkDescriptorSetLayout m_MaterialSetLayout = VK_NULL_HANDLE; // set 1: a material's texture
    // What every pipeline's shaders receive besides vertices: set 0, set 1 and the push
    // constants. One layout shared by all pipelines keeps descriptor sets bound across them.
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    std::unique_ptr<Swapchain> m_Swapchain;
    std::unique_ptr<Image> m_DepthImage; // the swapchain images' size, so it's rebuilt with them
    std::unique_ptr<ImGuiRenderer> m_ImGui; // draws the debug UI over the scene

    // The engine's defaults: the shader every material uses, and the texture for materials
    // created without one.
    std::shared_ptr<Shader> m_DefaultShader; // Unlit
    std::shared_ptr<Texture> m_WhiteTexture; // 1x1 white: "no texture"

    std::vector<DrawCommand> m_DrawList; // this frame's Submits
    CameraUniforms m_Camera {};          // from SetCamera
    glm::vec3 m_ClearColor { 0.0f };     // from SetClearColor
    RenderStats m_Stats;                 // about the last frame drawn
    int m_LiveResources = 0;             // handed to the game and not released yet
    bool m_DestroyNow = false;           // shutting down: release means destroy

    Extent m_SwapchainWindowSize;     // the window's pixel size when the swapchain was built
    bool m_SwapchainOutdated = false; // set when Vulkan reports the swapchain no longer fits
    bool m_FrameOpen = false;         // between a successful BeginFrame and its EndFrame
    uint32_t m_FrameIndex = 0;        // the frame slot being built, or built next
    uint32_t m_ReleaseSlot = 0;       // the frame slot of the newest frame begun
    uint32_t m_ImageIndex = 0;        // the swapchain image BeginFrame acquired
};

// The public functions: Create, then forwarding to Impl.

std::unique_ptr<Renderer> Renderer::Create(const Window& window, bool vsync)
{
    auto renderer = std::make_unique<Renderer>();
    renderer->m_Impl = std::make_unique<Impl>(window, vsync);
    if (!renderer->m_Impl->Initialize())
        return nullptr;
    return renderer;
}

Renderer::Renderer() = default;
Renderer::~Renderer() = default;

std::shared_ptr<Mesh> Renderer::CreateMesh(const MeshData& data, std::string assetName) { return m_Impl->CreateMesh(data, std::move(assetName)); }
std::shared_ptr<Texture> Renderer::CreateTexture(uint32_t width, uint32_t height, std::span<const uint8_t> pixels, std::string assetName) { return m_Impl->CreateTexture(width, height, pixels, std::move(assetName)); }
std::shared_ptr<Material> Renderer::CreateMaterial(const MaterialSettings& settings) { return m_Impl->CreateMaterial(settings); }
const RenderStats& Renderer::GetStats() const { return m_Impl->GetStats(); }
void Renderer::SetVSync(bool enabled) { m_Impl->SetVSync(enabled); }
bool Renderer::IsVSync() const { return m_Impl->IsVSync(); }
void Renderer::SetCamera(const glm::mat4& view, const glm::mat4& projection) { m_Impl->SetCamera(view, projection); }
void Renderer::SetClearColor(const glm::vec3& color) { m_Impl->SetClearColor(color); }
float Renderer::GetAspectRatio() const { return m_Impl->GetAspectRatio(); }
bool Renderer::BeginFrame() { return m_Impl->BeginFrame(); }
void Renderer::EndFrame() { m_Impl->EndFrame(); }
const std::string& Renderer::GetAssetName(const Mesh& mesh) { return mesh.GetAssetName(); }
const std::string& Renderer::GetAssetName(const Texture& texture) { return texture.GetAssetName(); }
const MaterialSettings& Renderer::GetSettings(const Material& material) { return material.GetSettings(); }

void Renderer::Submit(const std::shared_ptr<Mesh>& mesh, const std::shared_ptr<Material>& material,
                      const glm::mat4& transform)
{
    VIVA_ASSERT(mesh && material, "Submit needs a mesh and a material");
    if (mesh && material)
        m_Impl->Submit(*mesh, *material, transform);
}

// Impl.

Renderer::Impl::Impl(const Window& window, bool vsync)
    : m_Window(window)
    , m_VSync(vsync)
{
}

Renderer::Impl::~Impl()
{
    if (!m_Context)
        return; // Initialize failed before anything was created
    VkDevice device = m_Context->GetDevice();

    // Wait until the GPU has finished all submitted work: then everything may be destroyed. From
    // here on, a resource that's released is destroyed at once instead of parked (see Track),
    // which also takes care of resources released by others (a material its texture).
    VK_CHECK(vkDeviceWaitIdle(device));
    m_DestroyNow = true;
    m_DefaultShader.reset();
    m_WhiteTexture.reset();
    if (m_Frames) {
        for (uint32_t i = 0; i < FrameResources::kFramesInFlight; ++i)
            m_Frames->GetFrame(i).ReleasedResources.clear();
    }

    // A resource the game still holds now would be destroyed after the device, and its release
    // would reach into this destroyed renderer: a bug in the game, for example a resource kept in
    // a global variable. Logged in every build, because it crashes later.
    if (m_LiveResources != 0) {
        Log::Error("{} GPU resource(s) outlived the renderer", m_LiveResources);
        VIVA_ASSERT(m_LiveResources == 0);
    }

    vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, m_MaterialSetLayout, nullptr);
}

bool Renderer::Impl::Initialize()
{
    m_Context = VulkanContext::Create(m_Window);
    if (!m_Context)
        return false;
    VkDevice device = m_Context->GetDevice();

    m_Descriptors = std::make_unique<DescriptorAllocator>(device);
    m_Frames = std::make_unique<FrameResources>(device, m_Context->GetGraphicsQueueFamily());
    m_Uniforms = FrameUniforms::Create(*m_Context, *m_Descriptors);

    // Set 1, a material: one combined image sampler (its texture), read by the fragment shader as
    // "layout(set = 1, binding = 0) uniform sampler2D albedo".
    const VkDescriptorSetLayoutBinding materialBindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };
    m_MaterialSetLayout = CreateDescriptorSetLayout(device, materialBindings);

    // The pipeline layout every shader shares: the camera (set 0), the material (set 1) and the
    // per-draw push constants.
    const VkDescriptorSetLayout setLayouts[] = { m_Uniforms->GetLayout(), m_MaterialSetLayout };
    const VkPipelineLayoutCreateInfo layoutInfo {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<uint32_t>(std::size(setLayouts)),
        .pSetLayouts = setLayouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &kPushConstantRange,
    };
    VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_PipelineLayout));

    if (!RecreateSwapchain())
        return false;

    m_DefaultShader = LoadShader("Unlit");
    if (!m_DefaultShader)
        return false;
    constexpr uint8_t kWhitePixel[] = { 255, 255, 255, 255 };
    m_WhiteTexture = CreateTexture(1, 1, kWhitePixel, {});

    // The debug UI is drawn in the same rendering as the scene, so its pipeline is built for the
    // same color and depth formats.
    m_ImGui = ImGuiRenderer::Create(*m_Context, m_Swapchain->GetFormat(), VulkanContext::kDepthFormat,
                                    FrameResources::kFramesInFlight);
    return m_ImGui != nullptr;
}

std::shared_ptr<Mesh> Renderer::Impl::CreateMesh(const MeshData& data, std::string assetName)
{
    if (data.Vertices.empty() || data.Indices.empty()) {
        Log::Error("Can't create a mesh without vertices and indices");
        return nullptr;
    }
    std::unique_ptr<Mesh> mesh = Mesh::Create(*m_Context, data);
    mesh->SetAssetName(std::move(assetName));
    return Track(std::move(mesh));
}

std::shared_ptr<Texture> Renderer::Impl::CreateTexture(uint32_t width, uint32_t height, std::span<const uint8_t> pixels,
                                                       std::string assetName)
{
    // Every texture comes through here, so this one check covers them all. The pixels may come
    // from the game, so a mismatch is reported in every build, rather than read past the end.
    if (width == 0 || height == 0 || pixels.size() != size_t { width } * height * 4) {
        Log::Error("A {}x{} texture needs {} bytes of RGBA pixels, not {}", width, height,
                   size_t { width } * height * 4, pixels.size());
        return nullptr;
    }
    std::unique_ptr<Texture> texture = Texture::Create(*m_Context, width, height, pixels);
    texture->SetAssetName(std::move(assetName));
    return Track(std::move(texture));
}

std::shared_ptr<Shader> Renderer::Impl::LoadShader(const std::string& name)
{
    return Track(Shader::Create(m_Context->GetDevice(), {
        .VertexShader = name + ".vert",
        .FragmentShader = name + ".frag",
        .VertexBindings = kVertexBindings,
        .VertexAttributes = kVertexAttributes,
        .Layout = m_PipelineLayout,
        .ColorFormat = m_Swapchain->GetFormat(),
        .DepthFormat = VulkanContext::kDepthFormat,
        .CullMode = VK_CULL_MODE_BACK_BIT,
    }));
}

std::shared_ptr<Material> Renderer::Impl::CreateMaterial(const MaterialSettings& settings)
{
    return Track(Material::Create(m_Context->GetDevice(), *m_Descriptors, m_MaterialSetLayout, m_DefaultShader,
                                  settings.Texture ? settings.Texture : m_WhiteTexture, settings));
}

void Renderer::Impl::Submit(const Mesh& mesh, const Material& material, const glm::mat4& transform)
{
    // Outside a frame, the draw list's plain pointers could outlive their resources (one released
    // before the frame begins is destroyed by BeginFrame). Release builds skip such a draw.
    VIVA_ASSERT(m_FrameOpen, "Submit only works while a frame is being built: call it from OnUpdate");
    if (!m_FrameOpen)
        return;
    m_DrawList.push_back({ .Shader = &material.GetShader(), .Material = &material, .Mesh = &mesh, .Transform = transform });
}

void Renderer::Impl::SetCamera(const glm::mat4& view, const glm::mat4& projection)
{
    m_Camera = { .View = view, .Projection = projection };
}

float Renderer::Impl::GetAspectRatio() const
{
    // The swapchain's size, which BeginFrame keeps in step with the window. Using it, rather than
    // the window's, means the picture never stretches while the window is being resized.
    const VkExtent2D extent = m_Swapchain->GetExtent();
    return static_cast<float>(extent.width) / static_cast<float>(extent.height);
}

void Renderer::Impl::SetVSync(bool enabled)
{
    // The present mode is picked when the swapchain is built (FIFO for vsync, see Swapchain.cpp),
    // so switching means building a new one, which the next BeginFrame does.
    if (enabled == m_VSync)
        return;
    m_VSync = enabled;
    m_SwapchainOutdated = true;
}

bool Renderer::Impl::RecreateSwapchain()
{
    // The GPU may still be using the old swapchain's images. Waiting until it's idle is the
    // simplest safe moment to replace them. It only happens on resize, so the pause doesn't matter.
    VK_CHECK(vkDeviceWaitIdle(m_Context->GetDevice()));

    const Extent size = m_Window.GetPixelSize();
    const VkSwapchainKHR old = m_Swapchain ? m_Swapchain->GetHandle() : VK_NULL_HANDLE;
    std::unique_ptr<Swapchain> swapchain = Swapchain::Create(*m_Context, { size.Width, size.Height }, m_VSync, old);
    if (!swapchain)
        return false;

    // Every pipeline is built for one color format, so a rebuild must keep it. With the same
    // surface it always does; this checks that assumption.
    VIVA_ASSERT(!m_Swapchain || swapchain->GetFormat() == m_Swapchain->GetFormat(), "the swapchain's format changed");

    // The old swapchain is destroyed here, after the new one was built from it.
    m_Swapchain = std::move(swapchain);
    m_Frames->EnsureRenderFinishedSemaphores(m_Swapchain->GetImageCount());

    // The depth buffer must be exactly as big as the images it's drawn with.
    m_DepthImage = Image::Create(*m_Context, {
        .Extent = m_Swapchain->GetExtent(),
        .Format = VulkanContext::kDepthFormat,
        .Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .Aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
    });

    m_SwapchainWindowSize = size;
    m_SwapchainOutdated = false;
    return true;
}

void Renderer::Impl::DestroyReleased(FrameData& frame)
{
    // Destroying a resource can release others it held (a material its texture), and those land
    // in a release list, maybe this very one. So take the list out first: std::exchange puts an
    // empty list in its place and returns the old one, whose resources are destroyed when
    // `released` goes away at the end of this function.
    std::vector<std::unique_ptr<GpuResource>> released = std::exchange(frame.ReleasedResources, {});
}

bool Renderer::Impl::BeginFrame()
{
    VIVA_ASSERT(!m_FrameOpen, "BeginFrame called twice without EndFrame");

    // Nothing to draw into while the window is minimized or has no area.
    if (!m_Window.IsDrawable())
        return false;

    // A new window size, or a swapchain Vulkan reported as outdated, needs a new swapchain.
    if ((m_Window.GetPixelSize() != m_SwapchainWindowSize || m_SwapchainOutdated) && !RecreateSwapchain())
        return false;

    VkDevice device = m_Context->GetDevice();
    FrameData& frame = m_Frames->GetFrame(m_FrameIndex);

    // 1. Wait until the GPU has finished the last frame that used this slot (two frames ago).
    //    A fence is how the GPU tells the CPU "done". After this, the frame's command buffer,
    //    semaphore and uniform buffer are free to reuse, and the resources released back then
    //    can't be in use any more: the GPU finishes frames in order.
    VK_CHECK(vkWaitForFences(device, 1, &frame.InFlight, VK_TRUE, UINT64_MAX));
    DestroyReleased(frame);

    // 2. Ask the swapchain which image to draw into next. The call returns as soon as it knows the
    //    index; the semaphore is signaled once the image is really free (the display may still be
    //    showing it).
    const VkResult result = vkAcquireNextImageKHR(device, m_Swapchain->GetHandle(), UINT64_MAX, frame.ImageAcquired,
                                                  VK_NULL_HANDLE, &m_ImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        m_SwapchainOutdated = true; // the window changed under us: rebuild next frame
        return false;
    }
    if (result != VK_SUBOPTIMAL_KHR) // "suboptimal" still works for this frame
        VK_CHECK(result);

    // Reset the fence only now that EndFrame will certainly submit work that signals it. Reset
    // earlier, a "return false" above would leave it unsignaled forever, and the next wait on it
    // would never end.
    VK_CHECK(vkResetFences(device, 1, &frame.InFlight));

    // This frame is now the newest that can draw with any resource: until the next BeginFrame,
    // whatever the game releases waits in its list.
    m_ReleaseSlot = m_FrameIndex;
    m_FrameOpen = true;
    m_ImGui->NewFrame();
    return true;
}

void Renderer::Impl::EndFrame()
{
    VIVA_ASSERT(m_FrameOpen, "EndFrame without a successful BeginFrame");
    VkDevice device = m_Context->GetDevice();
    FrameData& frame = m_Frames->GetFrame(m_FrameIndex);

    // 3. Fill this frame's uniform buffer with the camera (BeginFrame's fence wait made it free).
    //    The projection follows OpenGL's convention, where clip space y points up; Vulkan's points
    //    down, so flipping the y scale keeps +Y up on screen. (Unity does the same in
    //    GL.GetGPUProjectionMatrix.)
    CameraUniforms camera = m_Camera;
    camera.Projection[1][1] *= -1.0f;
    m_Uniforms->Write(m_FrameIndex, camera);

    // 4. Record this frame's commands: make the images drawable, clear them and draw, make the
    //    color image presentable.
    VK_CHECK(vkResetCommandPool(device, frame.CommandPool, 0));
    VkCommandBuffer cmd = frame.CommandBuffer;
    const VkCommandBufferBeginInfo beginInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, // recorded fresh every frame
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    const VkImage image = m_Swapchain->GetImage(m_ImageIndex);
    const VkExtent2D extent = m_Swapchain->GetExtent();

    // The old contents don't matter (we're about to clear), so the old layout is UNDEFINED. The
    // source stage is the one the "image acquired" semaphore wait applies to (see the submit), so
    // the transition happens only after the image is really free.
    TransitionImage(cmd, {
        .Image = image,
        .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .NewLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .SrcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .SrcAccess = VK_ACCESS_2_NONE,
        .DstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .DstAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    });

    // The depth buffer's old contents don't matter either. Both frames in flight share it, so the
    // previous frame's depth tests (which read and write it in the fragment-test stages) must be
    // finished before this frame clears it.
    TransitionImage(cmd, {
        .Image = m_DepthImage->GetHandle(),
        .Aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
        .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .NewLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .SrcStage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        .SrcAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .DstStage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        .DstAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    });

    // Dynamic rendering: draw straight into the image views. The color image is cleared to the
    // background (the camera's color) and kept for presenting (STORE). The depth buffer is cleared to 1.0, the far
    // plane, so anything drawn is nearer; it isn't needed after the frame, so DONT_CARE lets the
    // GPU skip writing it back to memory.
    const VkRenderingAttachmentInfo colorAttachment {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = m_Swapchain->GetImageView(m_ImageIndex),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = { .color = { { m_ClearColor.r, m_ClearColor.g, m_ClearColor.b, 1.0f } } },
    };
    const VkRenderingAttachmentInfo depthAttachment {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = m_DepthImage->GetView(),
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = { .depthStencil = { .depth = 1.0f } },
    };
    const VkRenderingInfo renderingInfo {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = { .extent = extent },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment,
        .pDepthAttachment = &depthAttachment,
    };
    vkCmdBeginRendering(cmd, &renderingInfo);

    // The pipelines' dynamic state: viewport and scissor cover the whole image. Set once here,
    // they apply to every draw that follows in this command buffer. The viewport's depth range
    // is Vulkan's 0..1.
    const VkViewport viewport {
        .width = static_cast<float>(extent.width),
        .height = static_cast<float>(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    const VkRect2D scissor { .extent = extent };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    RecordDraws(cmd);

    // The debug UI goes last, in the same rendering, so it's drawn over the scene. ImGui's
    // pipeline doesn't test depth, so the scene can't hide it.
    m_Stats.UiDrawCalls = m_ImGui->Record(cmd);

    vkCmdEndRendering(cmd);

    // Hand the image to presentation once the drawing's writes are done. Nothing after it in this
    // frame uses the image, so the destination stage is NONE: the semaphore covers the rest.
    TransitionImage(cmd, {
        .Image = image,
        .OldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .NewLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .SrcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .SrcAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .DstStage = VK_PIPELINE_STAGE_2_NONE,
        .DstAccess = VK_ACCESS_2_NONE,
    });

    VK_CHECK(vkEndCommandBuffer(cmd));

    // 5. Submit the commands to the graphics queue. They wait for "image acquired" before the
    //    color-output stage, then signal "render finished" for presentation and the fence for us.
    const VkSemaphore renderFinished = m_Frames->GetRenderFinished(m_ImageIndex);
    const VkSemaphoreSubmitInfo waitInfo {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = frame.ImageAcquired,
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    };
    const VkSemaphoreSubmitInfo signalInfo {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = renderFinished,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    };
    const VkCommandBufferSubmitInfo commandInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmd,
    };
    const VkSubmitInfo2 submitInfo {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &waitInfo,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &commandInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signalInfo,
    };
    VK_CHECK(vkQueueSubmit2(m_Context->GetGraphicsQueue(), 1, &submitInfo, frame.InFlight));

    // 6. Present: show the image once "render finished" is signaled. With FIFO this is where the
    //    frame rate gets tied to the display's refresh.
    const VkSwapchainKHR swapchain = m_Swapchain->GetHandle();
    const VkPresentInfoKHR presentInfo {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderFinished,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &m_ImageIndex,
    };
    const VkResult result = vkQueuePresentKHR(m_Context->GetPresentQueue(), &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        m_SwapchainOutdated = true;
    else
        VK_CHECK(result);

    UpdateMemoryStats();

    // The submits were used up by this frame; the next frame builds its own list.
    m_DrawList.clear();
    m_FrameOpen = false;
    m_FrameIndex = (m_FrameIndex + 1) % FrameResources::kFramesInFlight;
}

void Renderer::Impl::UpdateMemoryStats()
{
    // VMA reports per memory heap (see the heaps logged at startup in Debug builds), and the stats
    // add them up. vmaGetHeapBudgets is cheap enough to call every frame, unlike
    // vmaCalculateStatistics. It fills in only the heaps this GPU has; the rest stay zero.
    VmaBudget budgets[VK_MAX_MEMORY_HEAPS] {};
    vmaGetHeapBudgets(m_Context->GetAllocator(), budgets);

    m_Stats.GpuAllocations = 0;
    m_Stats.GpuAllocationBytes = 0;
    m_Stats.GpuMemoryBlocks = 0;
    m_Stats.GpuMemoryBlockBytes = 0;
    for (const VmaBudget& budget : budgets) {
        m_Stats.GpuAllocations += budget.statistics.allocationCount;
        m_Stats.GpuAllocationBytes += budget.statistics.allocationBytes;
        m_Stats.GpuMemoryBlocks += budget.statistics.blockCount;
        m_Stats.GpuMemoryBlockBytes += budget.statistics.blockBytes;
    }
}

void Renderer::Impl::RecordDraws(VkCommandBuffer cmd)
{
    // Sort the draws so that those sharing a shader, then a material, then a mesh come together:
    // every switch costs a bind, so grouping saves binds. The order doesn't change the picture,
    // because the depth buffer sorts out what's in front. std::less<> gives pointers a consistent
    // order (the < operator on unrelated pointers isn't guaranteed to).
    std::ranges::sort(m_DrawList, [](const DrawCommand& a, const DrawCommand& b) {
        const std::less<> less;
        if (a.Shader != b.Shader)
            return less(a.Shader, b.Shader);
        if (a.Material != b.Material)
            return less(a.Material, b.Material);
        return less(a.Mesh, b.Mesh);
    });

    // Set 0, this frame's camera, is bound once for every draw. All pipelines share
    // m_PipelineLayout, so it stays bound when the pipeline changes.
    const VkDescriptorSet cameraSet = m_Uniforms->GetSet(m_FrameIndex);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &cameraSet, 0, nullptr);

    m_Stats.DrawCalls = static_cast<uint32_t>(m_DrawList.size());
    m_Stats.Triangles = 0;

    const Shader* boundShader = nullptr;
    const Material* boundMaterial = nullptr;
    const Mesh* boundMesh = nullptr;
    for (const DrawCommand& draw : m_DrawList) {
        if (draw.Shader != boundShader) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.Shader->GetPipeline().GetHandle());
            boundShader = draw.Shader;
        }
        if (draw.Material != boundMaterial) {
            // Set 1: the material's texture.
            const VkDescriptorSet materialSet = draw.Material->GetDescriptorSet();
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 1, 1, &materialSet, 0, nullptr);
            boundMaterial = draw.Material;
        }
        if (draw.Mesh != boundMesh) {
            draw.Mesh->Bind(cmd);
            boundMesh = draw.Mesh;
        }

        const MaterialSettings& material = draw.Material->GetSettings();
        const ObjectPushConstants constants {
            .Model = draw.Transform,
            .Color = material.Color,
            .TilingOffset = glm::vec4(material.Tiling, material.Offset),
        };
        vkCmdPushConstants(cmd, m_PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(constants), &constants);
        draw.Mesh->Draw(cmd);
        m_Stats.Triangles += draw.Mesh->GetIndexCount() / 3;
    }
}

} // namespace Viva
