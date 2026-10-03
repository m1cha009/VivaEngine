#include "Renderer/Renderer.h"

#include "Renderer/FrameResources.h"
#include "Renderer/FrameUniforms.h"
#include "Renderer/Image.h"
#include "Renderer/Mesh.h"
#include "Renderer/Pipeline.h"
#include "Renderer/Swapchain.h"
#include "Renderer/VulkanCheck.h"
#include "Renderer/VulkanContext.h"
#include "Renderer/VulkanHelpers.h"
#include "Viva/Assert.h"
#include "Viva/Camera.h"
#include "Viva/Time.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <cmath>
#include <utility>

namespace Viva {

namespace {

// The background: a dark blue-grey, as a linear color (the sRGB swapchain brightens it on the way
// to the screen).
constexpr VkClearColorValue kClearColor { { 0.02f, 0.025f, 0.04f, 1.0f } };

// The per-draw data: the object's model matrix, pushed with vkCmdPushConstants before each draw.
// Push constants are the quickest way to hand a draw a little data; every GPU takes at least
// 128 bytes of them, and a mat4 is 64.
constexpr VkPushConstantRange kPushConstantRanges[] = {
    { .stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .offset = 0, .size = sizeof(glm::mat4) },
};

// The demo scene's meshes, built around their own origin ("model space"). Every triangle lists
// its corners counter-clockwise as seen from outside, which makes that side its front.

// Adds a square as two triangles that share the diagonal from corner 0 to corner 2. The corners
// must go counter-clockwise as seen from the side that should be the front.
void AddQuad(MeshData& mesh, const std::array<Vertex, 4>& corners)
{
    const auto first = static_cast<uint32_t>(mesh.Vertices.size());
    mesh.Vertices.insert(mesh.Vertices.end(), corners.begin(), corners.end());
    mesh.Indices.insert(mesh.Indices.end(), { first, first + 1, first + 2, first, first + 2, first + 3 });
}

// A cube from -0.5 to 0.5 on every axis, with a color per face. A corner belongs to three faces
// with three colors, so it's stored three times: 24 vertices, 36 indices.
MeshData Cube()
{
    // For each face: the direction it faces, and two directions along it (U and V) chosen so that
    // cross(U, V) = Normal. Then the corners -U-V, +U-V, +U+V, -U+V go counter-clockwise when
    // seen from outside.
    struct Face {
        glm::vec3 Normal;
        glm::vec3 U;
        glm::vec3 V;
        glm::vec3 Color;
    };
    constexpr Face kFaces[] = {
        { { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.90f, 0.20f, 0.20f } },  // +X red
        { { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f }, { 0.20f, 0.80f, 0.80f } }, // -X cyan
        { { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.30f, 0.85f, 0.30f } },  // +Y green
        { { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.80f, 0.30f, 0.80f } }, // -Y magenta
        { { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.25f, 0.40f, 0.95f } },  // +Z blue
        { { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.95f, 0.85f, 0.20f } }, // -Z yellow
    };
    constexpr glm::vec2 kCorners[] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } };
    // Each face is a little darker towards its first corner: a gentle gradient across it.
    constexpr float kShades[] = { 0.7f, 0.85f, 1.0f, 0.85f };

    MeshData mesh;
    for (const Face& face : kFaces) {
        std::array<Vertex, 4> corners;
        for (size_t i = 0; i < corners.size(); ++i) {
            const glm::vec3 position = 0.5f * (face.Normal + kCorners[i].x * face.U + kCorners[i].y * face.V);
            corners[i] = { position, face.Color * kShades[i] };
        }
        AddQuad(mesh, corners);
    }
    return mesh;
}

// A checkerboard on the ground (y = 0): tiles x tiles squares of 1 unit, centered on the origin.
MeshData Floor(int tiles)
{
    constexpr glm::vec3 kLight { 0.45f, 0.45f, 0.48f };
    constexpr glm::vec3 kDark { 0.22f, 0.22f, 0.25f };
    const float half = static_cast<float>(tiles) / 2.0f;

    MeshData mesh;
    for (int row = 0; row < tiles; ++row) {
        for (int column = 0; column < tiles; ++column) {
            const float x = static_cast<float>(column) - half;
            const float z = static_cast<float>(row) - half;
            const glm::vec3 color = (row + column) % 2 == 0 ? kLight : kDark;
            // Seen from above (-Z at the top), these corners go top-left, bottom-left,
            // bottom-right, top-right: counter-clockwise, so the floor's front faces up.
            AddQuad(mesh, { { { { x, 0.0f, z }, color },
                              { { x, 0.0f, z + 1.0f }, color },
                              { { x + 1.0f, 0.0f, z + 1.0f }, color },
                              { { x + 1.0f, 0.0f, z }, color } } });
        }
    }
    return mesh;
}

// Model matrices place a mesh in the world. They're built here as translate * rotate * scale,
// which applies to the mesh right to left: scale it, then turn it, then move it into place, just
// like a Unity Transform's scale, rotation and position.

// The cube in the middle: hovering above the floor and turning around a tilted axis.
glm::mat4 SpinningCubeTransform(float seconds)
{
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.5f, 0.0f));
    model = glm::rotate(model, 0.8f * seconds, glm::normalize(glm::vec3(0.4f, 1.0f, 0.2f)));
    return glm::scale(model, glm::vec3(1.2f));
}

// Pillar i of `count`, standing in a ring around the middle: the same cube mesh, stretched tall.
// The ring is turned by half a step, so no pillar stands between the starting camera and the cube.
glm::mat4 PillarTransform(int i, int count)
{
    const float angle = glm::two_pi<float>() * (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
    constexpr float kRadius = 6.0f;
    constexpr float kHeight = 3.0f;
    const glm::mat4 model = glm::translate(glm::mat4(1.0f),
                                           glm::vec3(kRadius * std::cos(angle), kHeight / 2.0f, kRadius * std::sin(angle)));
    return glm::scale(model, glm::vec3(0.6f, kHeight, 0.6f));
}

// Hands the next draw its model matrix, then draws the mesh.
void DrawMesh(VkCommandBuffer cmd, VkPipelineLayout layout, const Mesh& mesh, const glm::mat4& model)
{
    vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(model), &model);
    mesh.Draw(cmd);
}

} // namespace

std::unique_ptr<Renderer> Renderer::Create(const Window& window, bool vsync)
{
    auto renderer = std::make_unique<Renderer>(window, vsync);

    renderer->m_Context = VulkanContext::Create(window);
    if (!renderer->m_Context)
        return nullptr;
    const VulkanContext& context = *renderer->m_Context;

    renderer->m_Frames = std::make_unique<FrameResources>(context.GetDevice(), context.GetGraphicsQueueFamily());
    renderer->m_FrameUniforms = FrameUniforms::Create(context);
    if (!renderer->RecreateSwapchain())
        return nullptr;

    // The pipeline reads Vertex from vertex buffers, gets the camera through descriptor set 0 and
    // the model matrix as a push constant, tests depth and skips back faces.
    const VkDescriptorSetLayout setLayouts[] = { renderer->m_FrameUniforms->GetLayout() };
    renderer->m_VertexColorPipeline = Pipeline::Create(context.GetDevice(), {
        .VertexShader = "VertexColor.vert",
        .FragmentShader = "VertexColor.frag",
        .VertexBindings = kVertexBindings,
        .VertexAttributes = kVertexAttributes,
        .DescriptorSetLayouts = setLayouts,
        .PushConstantRanges = kPushConstantRanges,
        .ColorFormat = renderer->m_Swapchain->GetFormat(),
        .DepthFormat = VulkanContext::kDepthFormat,
        .CullMode = VK_CULL_MODE_BACK_BIT,
    });
    if (!renderer->m_VertexColorPipeline)
        return nullptr;

    renderer->m_CubeMesh = Mesh::Create(context, Cube());
    renderer->m_FloorMesh = Mesh::Create(context, Floor(24));
    context.LogMemoryUsage();
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

void Renderer::DrawFrame(const Camera& camera)
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
    //    A fence is how the GPU tells the CPU "done". After this, the frame's command buffer,
    //    semaphore and uniform buffer are free to reuse.
    VK_CHECK(vkWaitForFences(device, 1, &frame.InFlight, VK_TRUE, UINT64_MAX));

    // 2. Fill this frame's uniform buffer with the camera. The aspect ratio comes from the
    //    swapchain, so the picture never stretches when the window is resized. The projection
    //    follows OpenGL's convention, where clip space y points up; Vulkan's points down, so
    //    flipping the y scale keeps +Y up on screen. (Unity does the same in
    //    GL.GetGPUProjectionMatrix.)
    const VkExtent2D extent = m_Swapchain->GetExtent();
    const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    glm::mat4 projection = camera.ProjectionMatrix(aspect);
    projection[1][1] *= -1.0f;
    m_FrameUniforms->Write(m_FrameIndex, { .View = camera.ViewMatrix(), .Projection = projection });

    // 3. Ask the swapchain which image to draw into next. The call returns as soon as it knows the
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

    // 4. Record this frame's commands: make the images drawable, clear them and draw, make the
    //    color image presentable.
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
    // background and kept for presenting (STORE). The depth buffer is cleared to 1.0, the far
    // plane, so anything drawn is nearer; it isn't needed after the frame, so DONT_CARE lets the
    // GPU skip writing it back to memory.
    const VkRenderingAttachmentInfo colorAttachment {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = m_Swapchain->GetImageView(imageIndex),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = { .color = kClearColor },
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

    // 6. Present: show the image once "render finished" is signaled. With FIFO this is where the
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
    const VkPipelineLayout layout = m_VertexColorPipeline->GetLayout();
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_VertexColorPipeline->GetHandle());

    // Set 0: this frame's camera. Bound once, it stays bound for every draw that follows.
    const VkDescriptorSet cameraSet = m_FrameUniforms->GetSet(m_FrameIndex);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &cameraSet, 0, nullptr);

    // The demo scene. Every draw pushes its own model matrix; the pillars and the spinning cube
    // all draw the same cube mesh, just placed differently.
    DrawMesh(cmd, layout, *m_FloorMesh, glm::mat4(1.0f)); // the identity matrix: the floor as built
    constexpr int kPillars = 8;
    for (int i = 0; i < kPillars; ++i)
        DrawMesh(cmd, layout, *m_CubeMesh, PillarTransform(i, kPillars));
    DrawMesh(cmd, layout, *m_CubeMesh, SpinningCubeTransform(static_cast<float>(Time::SinceStart())));
}

} // namespace Viva
