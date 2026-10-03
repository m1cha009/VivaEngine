#pragma once

#include "Viva/MeshData.h"

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <memory>
#include <string>

namespace Viva {

struct Camera;
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
    std::shared_ptr<Material> CreateMaterial(const MaterialSettings& settings);

    // Queues `mesh`, drawn with `material` and placed in the world by `transform` (its model
    // matrix), for the frame being built. Like Unity's Graphics.DrawMesh: it lasts one frame, so
    // call it every frame, from OnUpdate (the only time a frame is being built).
    void Submit(const std::shared_ptr<Mesh>& mesh, const std::shared_ptr<Material>& material,
                const glm::mat4& transform);

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
    // Draws everything submitted since BeginFrame, seen from `camera`, and shows it.
    void EndFrame(const Camera& camera);

    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace Viva
