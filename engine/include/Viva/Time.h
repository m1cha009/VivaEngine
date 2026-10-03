#pragma once

#include <cstdint>

namespace Viva {

// Frame timing, like Unity's Time class. The main loop updates it once per frame; read it from
// the main thread (OnUpdate, OnFixedUpdate and friends).
class Time {
public:
    Time() = delete;

    // Seconds the last frame took, capped at 0.25. Multiply movement by it to make it independent
    // of the frame rate, exactly like Time.deltaTime. Inside OnFixedUpdate it returns the fixed
    // step instead, as in Unity.
    static float DeltaTime();

    // Seconds between two OnFixedUpdate calls: 0.02 (50 per second) by default, like
    // Time.fixedDeltaTime.
    static float FixedDeltaTime();
    static void SetFixedDeltaTime(float seconds);

    // Seconds of game time since the first frame (the sum of all frame times), like Time.time.
    static double SinceStart();

    // Frames since the start, like Time.frameCount.
    static uint64_t FrameCount();

private:
    // "friend" lets one specific class use private members, here so that only the main loop can
    // advance time. It's a per-class version of C#'s InternalsVisibleTo.
    friend class Application;
    static void BeginFrame(float deltaTime);
    static void SetDeltaTime(float seconds);
};

} // namespace Viva
