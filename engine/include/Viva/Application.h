#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace Viva {

// Declared, not defined: the game never sees these classes' definitions (they live in
// src/Platform/ and src/Renderer/). A pointer or unique_ptr to a type only needs a declaration.
class Renderer;
class Window;

// Settings for the application's window. C++20 lets you fill in just the fields you need:
//     Application({ .Title = "My Game", .Width = 1600 })
struct ApplicationSettings {
    std::string Title = "VivaEngine";
    uint32_t Width = 1280;
    uint32_t Height = 720;
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

    // Called once, after the window exists and before the first frame. Like Start().
    virtual void OnStart() {}
    // Called once per frame, with the frame's duration in seconds. Like Update().
    virtual void OnUpdate(float /*dt*/) {}
    // Called at a fixed rate, 50 times per second by default, no matter the frame rate: zero,
    // one or several times per frame. Like FixedUpdate().
    virtual void OnFixedUpdate(float /*fixedDt*/) {}
    // Called once after the loop ends, while the window still exists. Like OnDestroy().
    virtual void OnShutdown() {}

    // Changes the text in the window's title bar.
    void SetWindowTitle(const std::string& title);

private:
    ApplicationSettings m_Settings;
    std::unique_ptr<Window> m_Window;
    // Declared after m_Window, so it's destroyed first: members are destroyed in reverse order,
    // and the renderer's Vulkan surface belongs to the window.
    std::unique_ptr<Renderer> m_Renderer;
    bool m_QuitRequested = false;
};

} // namespace Viva
