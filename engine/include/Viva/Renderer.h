#pragma once

#include "Viva/MeshData.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace Viva {

class Window;

// GPU resources. A game holds them through std::shared_ptr and hands them back to the renderer;
// what's inside is the renderer's business (the classes are defined in engine/src/Renderer/),
// which is why they're only declared here. When the last shared_ptr to one goes away, the
// renderer destroys it a couple of frames later, once the GPU can't be using it any more.
class Mesh;
class Texture;
class Material;

// What a material is made of. Fill it with designated initializers:
//     renderer.CreateMaterial({ .Texture = crate, .Color = { 1.0f, 0.5f, 0.5f, 1.0f } });
// Every material uses the engine's Unlit shader: texture times color, no lighting.
struct MaterialSettings {
    // The texture: null means plain white, so only the colors show.
    std::shared_ptr<Viva::Texture> Texture;
    // Multiplies the texture's color (and the vertex colors), like a Unity material's Color.
    glm::vec4 Color { 1.0f };
};

// Numbers about the last frame the renderer drew, for a stats display (see Renderer::GetStats).
struct RenderStats {
    // The scene: one draw call per Submit, and the triangles they drew.
    uint32_t DrawCalls = 0;
    uint32_t Triangles = 0;
    // The debug UI's draw calls (Dear ImGui batches many widgets into each one).
    uint32_t UiDrawCalls = 0;
    // GPU memory as VMA counts it (M5): every buffer and image is one allocation, and VMA packs
    // the allocations into a few large blocks of GPU memory. ImGui's own memory isn't included.
    uint32_t GpuAllocations = 0;
    uint64_t GpuAllocationBytes = 0;
    uint32_t GpuMemoryBlocks = 0;
    uint64_t GpuMemoryBlockBytes = 0;
};

// The renderer, as games see it: create GPU resources, then say each frame what to draw. Get it
// from Application::GetRenderer().
//
// Everything Vulkan lives in Renderer::Impl, defined in Renderer.cpp (the "pimpl" idiom: pointer
// to implementation). This header only names Impl, so games that include it never see Vulkan,
// and changing the renderer's internals doesn't recompile them.
class Renderer {
public:
    Renderer(); // creates nothing: Application calls Create()
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Resources. Each returns nullptr (after logging why) if it fails. Creating a mesh or a
    // texture copies its data to the GPU and waits for that to finish, so create them while
    // loading (OnStart), then reuse them, rather than creating them every frame.

    std::shared_ptr<Mesh> CreateMesh(const MeshData& data);
    // Loads a PNG or JPEG from the assets folder, for example "textures/crate.png". Loading the
    // same name again, while the texture is still in use, returns the same texture.
    std::shared_ptr<Texture> LoadTexture(const std::string& assetName);
    // A texture from pixels in memory: width x height pixels of 4 bytes (red, green, blue, alpha),
    // row after row from the top-left corner. Like a Unity Texture2D filled with SetPixels32.
    // Model::Load uses it for images stored inside model files.
    std::shared_ptr<Texture> CreateTexture(uint32_t width, uint32_t height, std::span<const uint8_t> pixels);
    std::shared_ptr<Material> CreateMaterial(const MaterialSettings& settings);

    // Queues `mesh`, drawn with `material` and placed in the world by `transform` (its model
    // matrix), for the frame being built. Like Unity's Graphics.DrawMesh: it lasts one frame, so
    // call it every frame, from OnUpdate (the only time a frame is being built). The scene submits
    // its MeshRenderers this way by itself; call it for anything extra.
    void Submit(const std::shared_ptr<Mesh>& mesh, const std::shared_ptr<Material>& material,
                const glm::mat4& transform);

    // Where the frame is seen from: a view matrix and a projection matrix (see Viva/Camera.h).
    // The scene sets them every frame from its main camera. They stay until set again.
    void SetCamera(const glm::mat4& view, const glm::mat4& projection);
    // The color each frame starts from, which shows wherever nothing is drawn: a linear color.
    // The scene sets it every frame from its main camera's BackgroundColor. Black until then.
    void SetClearColor(const glm::vec3& color);
    // The width of the image being drawn divided by its height: a camera's projection needs it.
    float GetAspectRatio() const;

    // Numbers about the last frame drawn: draw calls, triangles and GPU memory.
    const RenderStats& GetStats() const;

    // Vsync (see ApplicationSettings::VSync) can be switched while the game runs. The swapchain is
    // rebuilt with the new present mode before the next frame.
    void SetVSync(bool enabled);
    bool IsVSync() const;

private:
    // The engine side, used by Application only. Each frame is BeginFrame, the game's updates
    // (which call Submit), then EndFrame.
    friend class Application;

    // Returns nullptr (after logging why) if Vulkan can't be set up on this machine.
    // vsync: wait for the display's refresh between frames, rather than drawing as fast as possible.
    static std::unique_ptr<Renderer> Create(const Window& window, bool vsync);
    // Waits until the GPU can take another frame and the next swapchain image is free. Returns
    // false if this frame can't be drawn (the window is minimized or was just resized); then
    // EndFrame must not be called.
    bool BeginFrame();
    // Draws everything submitted since BeginFrame, seen from the camera last set, and shows it.
    void EndFrame();

    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace Viva
