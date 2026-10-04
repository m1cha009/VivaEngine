#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace Viva {

// Declared, not defined: a pointer or unique_ptr to a type only needs a declaration. The game
// never sees Window's definition (it lives in src/Platform/). The others are public, in
// Viva/Assets.h, Viva/Renderer.h and Viva/Scene.h, for the games that use them.
class Assets;
class Renderer;
class Scene;
class Window;

// Settings for the application's window. C++20 lets you fill in just the fields you need:
//     Application({ .Title = "My Game", .Width = 1600 })
struct ApplicationSettings {
    std::string Title = "VivaEngine";
    uint32_t Width = 1280;
    uint32_t Height = 720;
    // Wait for the display's refresh between frames (no tearing, frame rate = refresh rate).
    // Off: draw as fast as possible.
    bool VSync = true;
    // Which monitor the window opens on, counting from 0 in the order the OS lists them (0 is
    // usually the main one). An index with no monitor falls back to the main one.
    uint32_t Display = 0;
    // Quit by itself after this many seconds, or 0 to run until Quit() or the window closes. For
    // scripted tests.
    float QuitAfter = 0.0f;

    // Reads the options every program understands from its command line, and leaves the others
    // for the program to read:
    //   --display <n>       Display
    //   --no-vsync          VSync off
    //   --quit-after <sec>  QuitAfter
    void ReadCommandLine(int argc, char* argv[]);
};

// The base class of a game. It owns the window and the main loop. A game derives from it and
// overrides the On... functions, the way a MonoBehaviour overrides Start and Update:
//
//     class MyGame : public Viva::Application {
//         void OnUpdate(float dt) override { ... }
//     };
//     int main() { MyGame game; return game.Run(); }
class Application {
public:
    explicit Application(ApplicationSettings settings = {});

    // A class with virtual functions needs a virtual destructor, so that destroying a MyGame
    // through an Application pointer also runs MyGame's destructor.
    virtual ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // Opens the window and runs the main loop until Quit() is called or the window is closed.
    // Returns the exit code for main().
    int Run();

    // Ends the main loop after the current frame.
    void Quit();

protected:
    // "virtual" means a derived class can replace the function, as in C#. Unlike C#, the
    // replacement is marked with "override" after the parameter list, not before the name.
    // The parameter names are commented out because these default versions don't use them.

    // These work like a MonoBehaviour on a "game manager" object. Most game logic belongs in
    // components on GameObjects (see Viva/Component.h); these are for what concerns the whole game.

    // Called once, after the window and the empty scene exist and before the first frame: the
    // place to build the scene. Like Start().
    virtual void OnStart() {}
    // Called once per frame, with the frame's duration in seconds, after the scene's components
    // have updated (their OnUpdate, then their OnLateUpdate), so it sees where everything ended up.
    // Also the place for Dear ImGui debug windows (include <imgui.h>), like OnGUI().
    virtual void OnUpdate(float /*dt*/) {}
    // Called at a fixed rate, 50 times per second by default, no matter the frame rate: zero,
    // one or several times per frame, after the components' OnFixedUpdate. Like FixedUpdate().
    virtual void OnFixedUpdate(float /*fixedDt*/) {}
    // Called once after the loop ends, while the window still exists. Like OnDestroy().
    virtual void OnShutdown() {}
    // Called once per frame after the scene has been handed to the renderer (its MeshRenderers
    // submitted, its main camera set) and before the frame is drawn: the place to draw from
    // another camera (the editor's Scene view) or to submit more.
    virtual void OnRender() {}
    // Called when the user closes the window. Return false to keep running, for example to ask
    // whether to save first, then call Quit() when it's time. Unity's Application.wantsToQuit.
    virtual bool OnQuitRequested() { return true; }

    // Changes the text in the window's title bar.
    void SetWindowTitle(const std::string& title);

    // Whether the scene's components update (OnStart, OnUpdate, OnLateUpdate, OnFixedUpdate) each
    // frame; it's still drawn either way. On by default. The editor turns it off while editing,
    // like Unity's Edit mode, where scripts don't run.
    void SetSceneUpdating(bool updating) { m_SceneUpdating = updating; }

    // The renderer: create meshes, textures and materials with it (from OnStart on). See
    // Viva/Renderer.h.
    Renderer& GetRenderer() { return *m_Renderer; }

    // The assets: meshes, textures, materials and models by name (see Viva/Assets.h), from OnStart
    // on. Their names start in the assets folder next to the executable, unless SetAssetsFolder
    // says otherwise.
    Assets& GetAssets() { return *m_Assets; }
    // Makes asset names start in `folder` (ending with a separator), for example a project's Assets
    // folder: a fresh Assets replaces the old one. What's loaded already stays while it's used.
    void SetAssetsFolder(const std::string& folder);

    // The scene: the GameObjects that make up the game world, from OnStart to OnShutdown. It's
    // drawn from its main camera every frame (see Viva/Scene.h).
    Scene& GetScene() { return *m_Scene; }

private:
    ApplicationSettings m_Settings;
    // Members are destroyed in reverse order of declaration: the assets, whose GPU resources go
    // back to the renderer, then the renderer, then the window, because the renderer's Vulkan
    // surface belongs to the window. They live until the Application itself is destroyed, after
    // the game's own members (see ~Application). The scene is destroyed at the end of Run, after
    // OnShutdown, while the game and the renderer still exist; it's declared last in case Run
    // returns early.
    std::unique_ptr<Window> m_Window;
    std::unique_ptr<Renderer> m_Renderer;
    std::unique_ptr<Assets> m_Assets;
    std::unique_ptr<Scene> m_Scene;
    bool m_QuitRequested = false;
    bool m_SceneUpdating = true;
};

} // namespace Viva
