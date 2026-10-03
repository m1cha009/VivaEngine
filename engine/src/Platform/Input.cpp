// The input state behind Viva/Input.h, and the code that fills it from SDL's events. It lives in
// Platform/ because it reads SDL events and key names directly.

#include "Viva/Input.h"

#include "Platform/InputEvents.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>

#include <array>
#include <cstddef>
#include <optional>

namespace Viva {

// Key's values must equal SDL's scancodes (both are USB HID codes), because this file converts
// between the two with a plain cast. static_assert checks a condition while compiling: if a
// number in Input.h were wrong, the build would stop right here. The checks cover the first and
// last key of every run of numbers in the enum.
static_assert(static_cast<int>(Key::A) == SDL_SCANCODE_A && static_cast<int>(Key::Z) == SDL_SCANCODE_Z);
static_assert(static_cast<int>(Key::Alpha1) == SDL_SCANCODE_1 && static_cast<int>(Key::Alpha0) == SDL_SCANCODE_0);
static_assert(static_cast<int>(Key::Return) == SDL_SCANCODE_RETURN && static_cast<int>(Key::Space) == SDL_SCANCODE_SPACE);
static_assert(static_cast<int>(Key::Minus) == SDL_SCANCODE_MINUS && static_cast<int>(Key::Backslash) == SDL_SCANCODE_BACKSLASH);
static_assert(static_cast<int>(Key::Semicolon) == SDL_SCANCODE_SEMICOLON && static_cast<int>(Key::CapsLock) == SDL_SCANCODE_CAPSLOCK);
static_assert(static_cast<int>(Key::F1) == SDL_SCANCODE_F1 && static_cast<int>(Key::F12) == SDL_SCANCODE_F12);
static_assert(static_cast<int>(Key::PrintScreen) == SDL_SCANCODE_PRINTSCREEN && static_cast<int>(Key::PageDown) == SDL_SCANCODE_PAGEDOWN);
static_assert(static_cast<int>(Key::RightArrow) == SDL_SCANCODE_RIGHT && static_cast<int>(Key::UpArrow) == SDL_SCANCODE_UP);
static_assert(static_cast<int>(Key::LeftControl) == SDL_SCANCODE_LCTRL && static_cast<int>(Key::RightMeta) == SDL_SCANCODE_RGUI);

namespace {

// Everything we know about one key or mouse button. Separate "pressed" and "released" flags,
// rather than comparing this frame's state with the previous one, also catch a quick tap that
// goes down and up within a single frame.
struct ButtonState {
    bool Held = false;
    bool Pressed = false;  // went down this frame
    bool Released = false; // went up this frame
};

constexpr std::size_t kKeyCount = SDL_SCANCODE_COUNT;
constexpr std::size_t kMouseButtonCount = 3;

// The input state. Variables with static storage get an s_ prefix. std::array is a fixed-size
// array that knows its own size, like a C# array whose length is part of its type.
std::array<ButtonState, kKeyCount> s_Keys {};
std::array<ButtonState, kMouseButtonCount> s_MouseButtons {};
glm::vec2 s_MousePosition { 0.0f };
glm::vec2 s_MouseDelta { 0.0f };
float s_MouseScroll = 0.0f;

std::size_t Index(MouseButton button) { return static_cast<std::size_t>(button); }

// A key code outside the table (no real key sends one) reads as a key that's never pressed.
ButtonState KeyState(Key key)
{
    const auto index = static_cast<std::size_t>(key);
    return index < kKeyCount ? s_Keys[index] : ButtonState {};
}

// "ButtonState&" is a reference: another name for the caller's object, like a C# "ref"
// parameter, so the function changes the original rather than a copy.
void Record(ButtonState& state, bool down)
{
    if (down && !state.Held)
        state.Pressed = true;
    if (!down && state.Held)
        state.Released = true;
    state.Held = down;
}

void ClearFrameFlags(ButtonState& state)
{
    state.Pressed = false;
    state.Released = false;
}

// std::optional<T> holds either a T or nothing, like C#'s Nullable<T> (T?).
std::optional<MouseButton> ToMouseButton(Uint8 sdlButton)
{
    switch (sdlButton) {
    case SDL_BUTTON_LEFT:   return MouseButton::Left;
    case SDL_BUTTON_RIGHT:  return MouseButton::Right;
    case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
    default:                return std::nullopt; // extra buttons: not tracked
    }
}

} // namespace

bool Input::GetKey(Key key) { return KeyState(key).Held; }
bool Input::GetKeyDown(Key key) { return KeyState(key).Pressed; }
bool Input::GetKeyUp(Key key) { return KeyState(key).Released; }

bool Input::GetMouseButton(MouseButton button) { return s_MouseButtons[Index(button)].Held; }
bool Input::GetMouseButtonDown(MouseButton button) { return s_MouseButtons[Index(button)].Pressed; }
bool Input::GetMouseButtonUp(MouseButton button) { return s_MouseButtons[Index(button)].Released; }

glm::vec2 Input::MousePosition() { return s_MousePosition; }
glm::vec2 Input::MouseDelta() { return s_MouseDelta; }
float Input::MouseScroll() { return s_MouseScroll; }

std::string_view Input::GetKeyName(Key key)
{
    // SDL returns "" for codes it has no name for.
    return SDL_GetScancodeName(static_cast<SDL_Scancode>(key));
}

void BeginInputFrame()
{
    // A range-based for loop, like C#'s foreach. The & makes each element a reference, so the
    // loop clears the real entries rather than copies of them.
    for (ButtonState& key : s_Keys)
        ClearFrameFlags(key);
    for (ButtonState& button : s_MouseButtons)
        ClearFrameFlags(button);
    s_MouseDelta = glm::vec2(0.0f);
    s_MouseScroll = 0.0f;
}

void ProcessInputEvent(const SDL_Event& event)
{
    // SDL_Event is a union: one block of memory that holds whichever event struct "type" says.
    // event.key is only valid for keyboard events, event.button for mouse buttons, and so on.
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        // Holding a key makes the OS repeat it. Input only cares about the real press and release.
        // SDL's scancodes are always below SDL_SCANCODE_COUNT, the size of s_Keys.
        if (!event.key.repeat)
            Record(s_Keys[static_cast<std::size_t>(event.key.scancode)], event.key.down);
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (const std::optional<MouseButton> button = ToMouseButton(event.button.button))
            Record(s_MouseButtons[Index(*button)], event.button.down);
        break;

    case SDL_EVENT_MOUSE_MOTION:
        s_MousePosition = { event.motion.x, event.motion.y };
        s_MouseDelta += glm::vec2(event.motion.xrel, event.motion.yrel); // several can arrive per frame
        break;

    case SDL_EVENT_MOUSE_WHEEL:
        s_MouseScroll += event.wheel.y;
        break;

    default:
        break;
    }
}

} // namespace Viva
