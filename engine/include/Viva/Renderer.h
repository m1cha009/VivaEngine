#pragma once

#include "Viva/MeshData.h"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
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
    // How often the texture repeats across the mesh, and how far it's shifted: a Unity material's
    // Tiling and Offset. A texture coordinate becomes UV * Tiling + Offset.
    glm::vec2 Tiling { 1.0f };
    glm::vec2 Offset { 0.0f };

    // Two settings are equal when every field is: the same texture (the same object, not just
    // the same pixels), color, tiling and offset. "= default" has the compiler compare each field
    // in turn (C++20).
    bool operator==(const MaterialSettings&) const = default;
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

    // Resources made from data in memory. Each returns nullptr (after logging why) if it fails.
    // Creating a mesh or a texture copies its data to the GPU and waits for that to finish, so
    // create them while loading (OnStart), then reuse them, rather than creating them every frame.
    // Assets made from files (image files, models, the built-in primitives) come from Assets
    // instead (see Viva/Assets.h), which creates them through these.
    //
    // assetName: the name the mesh or texture is known by in asset files, which a scene file
    // writes to refer to it. Assets gives its meshes and textures their names. Leave it empty for
    // those a game makes in code: like a Unity mesh made in a script, a scene file can't refer to
    // one, so a MeshRenderer that uses it is saved without it.
    std::shared_ptr<Mesh> CreateMesh(const MeshData& data, std::string assetName = {});
    // A texture from pixels in memory: width x height pixels of 4 bytes (red, green, blue, alpha),
    // row after row from the top-left corner. Like a Unity Texture2D filled with SetPixels32.
    std::shared_ptr<Texture> CreateTexture(uint32_t width, uint32_t height, std::span<const uint8_t> pixels,
                                           std::string assetName = {});
    std::shared_ptr<Material> CreateMaterial(const MaterialSettings& settings);

    // What a resource was made from, for writing it into a file: a mesh's or a texture's asset
    // name ("" if it was made in code), and a material's settings. Static: they only read the
    // resource, not the renderer.
    static const std::string& GetAssetName(const Mesh& mesh);
    static const std::string& GetAssetName(const Texture& texture);
    static const MaterialSettings& GetSettings(const Material& material);

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
    // The width of the image the scene is drawn into divided by its height: a camera's projection
    // needs it.
    float GetAspectRatio() const;

    // The editor's Scene view (M15): draws the scene into an image of its own, width x height
    // pixels, instead of into the window. The window then shows only the UI, and the UI shows the
    // scene with ImGui::Image(GetSceneTexture(), size), as a "render texture". Call it every frame
    // the image is shown: in a frame without the call, the image is kept but not drawn into. 0 x 0,
    // the default, draws the scene into the window, as games do. A new size takes effect at the
    // next BeginFrame.
    void SetSceneTargetSize(uint32_t width, uint32_t height);
    // The scene's image as an ImTextureID, for ImGui::Image, or 0 while there's none.
    uint64_t GetSceneTexture() const;

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
