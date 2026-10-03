// The sandbox: a small test program that uses the engine the way a game would.
// It includes only engine headers (Viva/...) and GLM, never SDL or Vulkan.

#include "Viva/Application.h"
#include "Viva/Assert.h"
#include "Viva/Input.h"
#include "Viva/Log.h"
#include "Viva/Time.h"

#include <cstdint>
#include <cstdlib>
#include <format>
#include <string_view>
#include <utility>

namespace {

// Fine in a .cpp file. In a header it would leak into every file that includes it.
using namespace Viva;

constexpr const char* kTitle = "VivaEngine Sandbox";

// The game itself. It derives from Application and overrides the parts it needs, the way a
// MonoBehaviour overrides Start and Update.
class SandboxApp : public Application {
public:
    // quitAfter: seconds until the app quits by itself, or 0 to run until Esc or the window closes.
    SandboxApp(ApplicationSettings settings, float quitAfter)
        : Application(std::move(settings))
        , m_QuitAfter(quitAfter)
    {
    }

protected:
    void OnStart() override
    {
        Log::Info("Press keys and mouse buttons to see them logged. Esc quits.");
    }

    void OnUpdate(float dt) override
    {
        LogInput();

        const bool timeIsUp = m_QuitAfter > 0.0f && Time::SinceStart() >= m_QuitAfter;
        if (Input::GetKeyDown(Key::Escape) || timeIsUp)
            Quit();

        // Once a second, show the frame rate in the title bar and how many fixed updates ran.
        m_SecondTimer += dt;
        ++m_FramesThisSecond;
        if (m_SecondTimer >= 1.0f) {
            const float fps = static_cast<float>(m_FramesThisSecond) / m_SecondTimer;
            SetWindowTitle(std::format("{} | {:.0f} FPS | {:.2f} ms", kTitle, fps, 1000.0f / fps));
            Log::Trace("Last second: {} frames, {} fixed updates", m_FramesThisSecond, m_FixedUpdatesThisSecond);
            m_SecondTimer = 0.0f;
            m_FramesThisSecond = 0;
            m_FixedUpdatesThisSecond = 0;
        }
    }

    void OnFixedUpdate(float /*fixedDt*/) override
    {
        ++m_FixedUpdatesThisSecond;
    }

    void OnShutdown() override
    {
        Log::Info("Shutting down after {} frames ({:.1f} s)", Time::FrameCount(), Time::SinceStart());
    }

private:
    static void LogInput()
    {
        // Checking every key code each frame is a few hundred array reads: fine for a demo.
        for (uint16_t code = 1; code <= static_cast<uint16_t>(Key::RightMeta); ++code) {
            const Key key = static_cast<Key>(code);
            if (Input::GetKeyDown(key))
                Log::Info("Key down: {}", Input::GetKeyName(key));
            if (Input::GetKeyUp(key))
                Log::Info("Key up:   {}", Input::GetKeyName(key));
        }

        struct NamedButton {
            MouseButton Button;
            const char* Name;
        };
        constexpr NamedButton kButtons[] = {
            { MouseButton::Left, "Left" }, { MouseButton::Right, "Right" }, { MouseButton::Middle, "Middle" }
        };
        const glm::vec2 position = Input::MousePosition();
        for (const NamedButton& button : kButtons) {
            if (Input::GetMouseButtonDown(button.Button))
                Log::Info("Mouse {} down at ({:.0f}, {:.0f})", button.Name, position.x, position.y);
            if (Input::GetMouseButtonUp(button.Button))
                Log::Info("Mouse {} up", button.Name);
        }

        if (Input::MouseScroll() != 0.0f)
            Log::Info("Mouse wheel: {:+.1f}", Input::MouseScroll());
    }

    float m_QuitAfter = 0.0f;
    float m_SecondTimer = 0.0f;
    uint32_t m_FramesThisSecond = 0;
    uint32_t m_FixedUpdatesThisSecond = 0;
};

} // namespace

int main(int argc, char* argv[])
{
    // Command-line options, mostly for testing:
    //   --test-assert        fail an assert on purpose (Debug builds), to see what that looks like
    //   --quit-after <sec>   quit by itself after that many seconds (used by CI and scripted tests)
    //   --no-vsync           draw as fast as possible instead of waiting for the display
    //   --display <n>        open the window on monitor n (0 is usually the main one)
    ApplicationSettings settings { .Title = kTitle };
    bool testAssert = false;
    float quitAfter = 0.0f;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--test-assert")
            testAssert = true;
        else if (arg == "--quit-after" && i + 1 < argc)
            quitAfter = std::strtof(argv[++i], nullptr);
        else if (arg == "--no-vsync")
            settings.VSync = false;
        else if (arg == "--display" && i + 1 < argc)
            settings.Display = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
    }
    VIVA_ASSERT(!testAssert, "failing on purpose because of --test-assert");

    // std::move hands the settings over instead of copying them: "settings" isn't used again.
    SandboxApp app(std::move(settings), quitAfter);
    return app.Run();
}
