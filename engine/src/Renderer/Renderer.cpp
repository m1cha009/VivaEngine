#include "Renderer/Renderer.h"

#include "Renderer/FrameResources.h"
#include "Renderer/Mesh.h"
#include "Renderer/Pipeline.h"
#include "Renderer/Swapchain.h"
#include "Renderer/VulkanCheck.h"
#include "Renderer/VulkanContext.h"
#include "Viva/Assert.h"
#include "Viva/Time.h"

#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>

#include <cmath>
#include <numbers>
#include <utility>

namespace Viva {

namespace {

// An image memory barrier, in the synchronization2 style. It does two jobs:
//  - Ordering: work in srcStage (and its memory writes, srcAccess) must finish, and be visible,
//    before work in dstStage (doing dstAccess) starts. The GPU runs commands in parallel and out
//    of order unless barriers like this say otherwise.
//  - Layout: it moves the image from oldLayout to newLayout. GPUs store images differently for
//    different uses (being drawn into, being shown on screen, being sampled as a texture), and
//    Vulkan makes us say when an image changes role.
void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                     VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess, VkPipelineStageFlags2 dstStage,
                     VkAccessFlags2 dstAccess)
{
    const VkImageMemoryBarrier2 barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = srcStage,
        .srcAccessMask = srcAccess,
        .dstStageMask = dstStage,
        .dstAccessMask = dstAccess,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, // not moving between queue families
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 },
    };
    const VkDependencyInfo dependency {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(cmd, &dependency);
}

// A dark color that slowly cycles through the hues: red, green and blue each follow a sine wave,
// a third of a turn apart. (std::numbers::pi is C++20's Mathf.PI.)
VkClearColorValue CyclingColor(double seconds)
{
    constexpr float kThirdTurn = 2.0f * std::numbers::pi_v<float> / 3.0f;
    const auto t = static_cast<float>(seconds);
    return { { 0.05f + 0.05f * std::sin(t), 0.05f + 0.05f * std::sin(t + kThirdTurn),
               0.05f + 0.05f * std::sin(t + 2.0f * kThirdTurn), 1.0f } };
}

// M5's demo shapes, in clip space: x and y from -1 to 1, y pointing down. Every triangle lists
// its corners clockwise as seen on screen.

// The RGB triangle from M4, now made of real vertices: three of them, one triangle.
MeshData Triangle(glm::vec2 center, float size)
{
    return {
        .Vertices = {
            { { center.x, center.y - size, 0.0f }, { 1.0f, 0.0f, 0.0f } },        // top: red
            { { center.x + size, center.y + size, 0.0f }, { 0.0f, 1.0f, 0.0f } }, // bottom right: green
            { { center.x - size, center.y + size, 0.0f }, { 0.0f, 0.0f, 1.0f } }, // bottom left: blue
        },
        .Indices = { 0, 1, 2 },
    };
}

// A square: two triangles that share two corners (the diagonal), so 4 vertices instead of 6.
MeshData Quad(glm::vec2 center, float halfSize)
{
    return {
        .Vertices = {
            { { center.x - halfSize, center.y - halfSize, 0.0f }, { 1.0f, 0.5f, 0.0f } }, // 0 top left: orange
            { { center.x + halfSize, center.y - halfSize, 0.0f }, { 1.0f, 1.0f, 0.2f } }, // 1 top right: yellow
            { { center.x + halfSize, center.y + halfSize, 0.0f }, { 0.0f, 0.8f, 1.0f } }, // 2 bottom right: sky blue
            { { center.x - halfSize, center.y + halfSize, 0.0f }, { 0.6f, 0.0f, 1.0f } }, // 3 bottom left: purple
        },
        .Indices = { 0, 1, 2, 2, 3, 0 },
    };
}

// A hexagon: a white center vertex plus six rainbow corners, drawn as six triangles that all
// share the center. 7 vertices instead of 18.
MeshData Hexagon(glm::vec2 center, float radius)
{
    constexpr glm::vec3 kCornerColors[] = {
        { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
        { 0.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 1.0f },
    };
    MeshData mesh;
    mesh.Vertices.push_back({ { center, 0.0f }, { 1.0f, 1.0f, 1.0f } });
    for (uint32_t i = 0; i < 6; ++i) {
        // Corner i sits i * 60 degrees around the center. With y pointing down, growing angles go
        // clockwise on screen.
        const float angle = glm::radians(60.0f * static_cast<float>(i));
        const glm::vec2 corner = center + radius * glm::vec2(std::cos(angle), std::sin(angle));
        mesh.Vertices.push_back({ { corner, 0.0f }, kCornerColors[i] });
        // Vertex 0 is the center; corners are 1 to 6, and the last triangle wraps back to corner 1.
        mesh.Indices.insert(mesh.Indices.end(), { 0, 1 + i, 1 + (i + 1) % 6 });
    }
    return mesh;
}

} // namespace

std::unique_ptr<Renderer> Renderer::Create(const Window& window, bool vsync)
{
    auto renderer = std::make_unique<Renderer>(window, vsync);

    renderer->m_Context = VulkanContext::Create(window);
    if (!renderer->m_Context)
        return nullptr;

    renderer->m_Frames = std::make_unique<FrameResources>(renderer->m_Context->GetDevice(),
                                                          renderer->m_Context->GetGraphicsQueueFamily());
    if (!renderer->RecreateSwapchain())
        return nullptr;

    renderer->m_VertexColorPipeline = Pipeline::Create(renderer->m_Context->GetDevice(), {
        .VertexShader = "VertexColor.vert",
        .FragmentShader = "VertexColor.frag",
        .VertexBindings = kVertexBindings,
        .VertexAttributes = kVertexAttributes,
        .ColorFormat = renderer->m_Swapchain->GetFormat(),
    });
    if (!renderer->m_VertexColorPipeline)
        return nullptr;

    // The demo shapes, side by side, uploaded to GPU memory once.
    for (const MeshData& data : { Triangle({ -0.6f, 0.0f }, 0.3f), Quad({ 0.0f, 0.0f }, 0.25f),
                                  Hexagon({ 0.6f, 0.0f }, 0.3f) })
        renderer->m_Meshes.push_back(Mesh::Create(*renderer->m_Context, data));
    renderer->m_Context->LogMemoryUsage();
    return renderer;
}

Renderer::Renderer(const Window& window, bool vsync)
    : m_Window(window)
    , m_VSync(vsync)
{
}

Renderer::~Renderer()
{
    // Wait until the GPU has finished all submitted work, so nothing below is destroyed while it's
    // still in use. The destructor's body runs before the members are destroyed.
    if (m_Context)
        VK_CHECK(vkDeviceWaitIdle(m_Context->GetDevice()));
}

bool Renderer::RecreateSwapchain()
{
    // The GPU may still be using the old swapchain's images. Waiting until it's idle is the
    // simplest safe moment to replace them. It only happens on resize, so the pause doesn't matter.
    VK_CHECK(vkDeviceWaitIdle(m_Context->GetDevice()));

    const Extent size = m_Window.GetPixelSize();
    const VkSwapchainKHR old = m_Swapchain ? m_Swapchain->GetHandle() : VK_NULL_HANDLE;
    std::unique_ptr<Swapchain> swapchain = Swapchain::Create(*m_Context, { size.Width, size.Height }, m_VSync, old);
    if (!swapchain)
        return false;

    // Everything that draws into swapchain images is built for one format (M4's pipeline), so a
    // rebuild must keep it. With the same surface it always does; this checks that assumption.
    VIVA_ASSERT(!m_Swapchain || swapchain->GetFormat() == m_Swapchain->GetFormat(), "the swapchain's format changed");

    // The old swapchain is destroyed here, after the new one was built from it.
    m_Swapchain = std::move(swapchain);
    m_Frames->EnsureRenderFinishedSemaphores(m_Swapchain->GetImageCount());
    m_SwapchainWindowSize = size;
    m_SwapchainOutdated = false;
    return true;
}

void Renderer::DrawFrame()
{
    // Nothing to draw into while the window has no area. (Application already skips those frames;
    // this guards the moment the window shrinks to nothing between two checks.)
    const Extent size = m_Window.GetPixelSize();
    if (size.IsEmpty())
        return;

    // A new window size, or a swapchain Vulkan reported as outdated, needs a new swapchain.
    if ((size != m_SwapchainWindowSize || m_SwapchainOutdated) && !RecreateSwapchain())
        return;

    VkDevice device = m_Context->GetDevice();
    FrameData& frame = m_Frames->GetFrame(m_FrameIndex);

    // 1. Wait until the GPU has finished the last frame that used this slot (two frames ago).
    //    A fence is how the GPU tells the CPU "done". After this, the frame's command buffer and
    //    semaphore are free to reuse.
    VK_CHECK(vkWaitForFences(device, 1, &frame.InFlight, VK_TRUE, UINT64_MAX));

    // 2. Ask the swapchain which image to draw into next. The call returns as soon as it knows the
    //    index; the semaphore is signaled once the image is really free (the display may still be
    //    showing it).
    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(device, m_Swapchain->GetHandle(), UINT64_MAX, frame.ImageAcquired,
                                            VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        m_SwapchainOutdated = true; // the window changed under us: rebuild next frame
        return;
    }
    if (result != VK_SUBOPTIMAL_KHR) // "suboptimal" still works for this frame
        VK_CHECK(result);

    // Reset the fence only now that work will certainly be submitted. Reset earlier, a "return"
    // above would leave it unsignaled forever, and the next wait on it would never end.
    VK_CHECK(vkResetFences(device, 1, &frame.InFlight));

    // 3. Record this frame's commands: make the image drawable, clear it and draw into it, make it
    //    presentable.
    VK_CHECK(vkResetCommandPool(device, frame.CommandPool, 0));
    VkCommandBuffer cmd = frame.CommandBuffer;
    const VkCommandBufferBeginInfo beginInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, // recorded fresh every frame
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    const VkImage image = m_Swapchain->GetImage(imageIndex);

    // The old contents don't matter (we're about to clear), so the old layout is UNDEFINED. The
    // source stage is the one the "image acquired" semaphore wait applies to (see the submit), so
    // the transition happens only after the image is really free.
    TransitionImage(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_NONE,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

    // Dynamic rendering: draw straight into the image's view. loadOp CLEAR fills it with the clear
    // color when rendering begins; storeOp STORE keeps the result for presenting. The render area,
    // viewport and scissor all cover the whole image.
    const VkExtent2D extent = m_Swapchain->GetExtent();
    const VkRenderingAttachmentInfo colorAttachment {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = m_Swapchain->GetImageView(imageIndex),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = { .color = CyclingColor(Time::SinceStart()) },
    };
    const VkRenderingInfo renderingInfo {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = { .extent = extent },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment,
    };
    vkCmdBeginRendering(cmd, &renderingInfo);

    // The pipelines' dynamic state: viewport and scissor cover the whole image. Set once here,
    // they apply to every draw that follows in this command buffer.
    const VkViewport viewport {
        .width = static_cast<float>(extent.width),
        .height = static_cast<float>(extent.height),
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    const VkRect2D scissor { .extent = extent };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    RecordDraws(cmd);

    vkCmdEndRendering(cmd);

    // Hand the image to presentation once the drawing's writes are done. Nothing after it in this
    // frame uses the image, so the destination stage is NONE: the semaphore covers the rest.
    TransitionImage(cmd, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE);

    VK_CHECK(vkEndCommandBuffer(cmd));

    // 4. Submit the commands to the graphics queue. They wait for "image acquired" before the
    //    color-output stage, then signal "render finished" for presentation and the fence for us.
    const VkSemaphore renderFinished = m_Frames->GetRenderFinished(imageIndex);
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

    // 5. Present: show the image once "render finished" is signaled. With FIFO this is where the
    //    frame rate gets tied to the display's refresh.
    const VkSwapchainKHR swapchain = m_Swapchain->GetHandle();
    const VkPresentInfoKHR presentInfo {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderFinished,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &imageIndex,
    };
    result = vkQueuePresentKHR(m_Context->GetPresentQueue(), &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        m_SwapchainOutdated = true;
    else
        VK_CHECK(result);

    m_FrameIndex = (m_FrameIndex + 1) % FrameResources::kFramesInFlight;
}

void Renderer::RecordDraws(VkCommandBuffer cmd)
{
    // All the meshes use the same pipeline, so it's bound once. Each mesh then binds its own
    // vertex and index buffers and draws.
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_VertexColorPipeline->GetHandle());
    for (const std::unique_ptr<Mesh>& mesh : m_Meshes)
        mesh->Draw(cmd);
}

} // namespace Viva
