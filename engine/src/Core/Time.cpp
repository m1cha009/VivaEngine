#include "Viva/Time.h"

#include "Viva/Assert.h"

namespace Viva {

namespace {

float s_DeltaTime = 0.0f;
float s_FixedDeltaTime = 0.02f; // 50 fixed updates per second, Unity's default
double s_SinceStart = 0.0;
uint64_t s_FrameCount = 0;

} // namespace

float Time::DeltaTime() { return s_DeltaTime; }
float Time::FixedDeltaTime() { return s_FixedDeltaTime; }
double Time::SinceStart() { return s_SinceStart; }
uint64_t Time::FrameCount() { return s_FrameCount; }

void Time::SetFixedDeltaTime(float seconds)
{
    // Zero or less would make the fixed-update loop in Application::Run() spin forever.
    VIVA_ASSERT(seconds > 0.0f, "the fixed time step must be positive, got {}", seconds);
    s_FixedDeltaTime = seconds;
}

void Time::BeginFrame(float deltaTime)
{
    s_DeltaTime = deltaTime;
    s_SinceStart += deltaTime;
    ++s_FrameCount;
}

void Time::SetDeltaTime(float seconds)
{
    s_DeltaTime = seconds;
}

} // namespace Viva
