// The sandbox: a small test program that uses the engine the way a game would.
// It includes only engine headers (Viva/...) and GLM, never SDL or Vulkan.

#include "DebugWindows.h"
#include "DemoScene.h"
#include "LaneRunnerScene.h"
#include "TruckWheels.h"

#include "Viva/Application.h"
#include "Viva/Assert.h"
#include "Viva/ComponentRegistry.h"
#include "Viva/Input.h"
#include "Viva/Log.h"
#include "Viva/Scene.h"
#include "Viva/Time.h"

#include <string>
#include <string_view>
#include <utility>

namespace {

// Fine in a .cpp file. In a header it would leak into every file that includes it.
using namespace Viva;

constexpr const char* kTitle = "VivaEngine Lane Runner";
constexpr const char* kDemoTitle = "VivaEngine Sandbox";

// What the sandbox shows, and what it does with scene files (see main for the options).
struct SandboxOptions {
    bool Demo = false;      // the engine's demo scene (M10, M11) instead of the Lane Runner game
    std::string LoadPath;   // a scene file to show instead of either
    std::string SavePath;   // where to save the scene once it's built
};

constexpr const char* kFlyHelp = "Fly with W/A/S/D, Q/E for down/up, Shift for speed; hold the right mouse button to "
                                 "look around. F1 shows or hides the debug windows. Esc quits.";

// The game itself. It derives from Application and overrides the parts it needs, the way a
// MonoBehaviour overrides Start and Update. The scene's objects bring their own behavior as
// components (TruckController, FollowCamera, LaneRunner...); this class handles what concerns
// the whole program.
class SandboxApp : public Application {
public:
    SandboxApp(ApplicationSettings settings, SandboxOptions options)
        : Application(std::move(settings))
        , m_Options(std::move(options))
    {
    }

protected:
    void OnStart() override
    {
        // The sandbox's own component that scenes use, so scene files can hold it (M13). The
        // engine's own (MeshRenderer, Camera, Spinner, FlyCamera) are registered already. The Lane
        // Runner's components aren't: that game is built in code.
        ComponentRegistry::Register<TruckWheels>("TruckWheels");

        if (!m_Options.LoadPath.empty()) {
            GetScene().Load(m_Options.LoadPath, GetAssets());
            Log::Info(kFlyHelp);
        } else if (m_Options.Demo) {
            LoadDemoScene(GetScene(), GetAssets());
            Log::Info(kFlyHelp);
        } else {
            LoadLaneRunnerScene(GetScene(), GetAssets());
            // The game has its own HUD; the debug windows wait for F1.
            m_DebugWindows.SetVisible(false);
            Log::Info("Lane Runner: Space drives, A/D or the arrow keys change lanes. F1 shows or hides the debug "
                      "windows. Esc quits.");
        }

        // Saved before anything has run, so the file holds the scene as it was built.
        if (!m_Options.SavePath.empty())
            GetScene().Save(m_Options.SavePath);
    }

    void OnUpdate(float dt) override
    {
        m_DebugWindows.Draw(dt, GetRenderer(), GetScene());

        if (Input::GetKeyDown(Key::Escape))
            Quit();
    }

    void OnFixedUpdate(float /*fixedDt*/) override
    {
        m_DebugWindows.CountFixedUpdate();
    }

    void OnShutdown() override
    {
        Log::Info("Shutting down after {} frames ({:.1f} s)", Time::FrameCount(), Time::SinceStart());
    }

private:
    DebugWindows m_DebugWindows;
    SandboxOptions m_Options;
};

} // namespace

int main(int argc, char* argv[])
{
    // Command-line options, mostly for testing. ReadCommandLine takes the ones every program has
    // (--display <n>, --no-vsync, --quit-after <sec>; see Viva/Application.h). The Sandbox's own:
    //   --test-assert        fail an assert on purpose (Debug builds), to see what that looks like
    //   --demo               show the engine's demo scene instead of the game
    //   --load <file>        show the scene saved in that file instead (M13)
    //   --save <file>        save the scene into that file once it's built (M13)
    ApplicationSettings settings { .Title = kTitle };
    settings.ReadCommandLine(argc, argv);
    SandboxOptions options;
    bool testAssert = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--test-assert")
            testAssert = true;
        else if (arg == "--demo")
            options.Demo = true;
        else if (arg == "--load" && i + 1 < argc)
            options.LoadPath = argv[++i];
        else if (arg == "--save" && i + 1 < argc)
            options.SavePath = argv[++i];
    }
    if (options.Demo || !options.LoadPath.empty())
        settings.Title = kDemoTitle;
    VIVA_ASSERT(!testAssert, "failing on purpose because of --test-assert");

    // std::move hands the settings over instead of copying them: they aren't used again here.
    SandboxApp app(std::move(settings), std::move(options));
    return app.Run();
}
