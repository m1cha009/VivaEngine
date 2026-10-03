#pragma once

#include <glm/vec2.hpp>

#include <cstdint>
#include <string_view>

namespace Viva {

// Physical keys, named like Unity's KeyCode.
//
// The numbers are USB HID usage codes: the standard codes keyboards themselves send, which SDL's
// scancodes use too. "Physical" means Key::W is the key in the W position of a US keyboard,
// whatever letter a French (AZERTY) or German layout prints on it. That's what you want for WASD
// controls. An enum class with a fixed type (uint16_t) can also hold codes that have no name here.
enum class Key : uint16_t {
    None = 0,
    A = 4, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Alpha1 = 30, Alpha2, Alpha3, Alpha4, Alpha5, Alpha6, Alpha7, Alpha8, Alpha9, Alpha0,
    Return = 40, Escape, Backspace, Tab, Space,
    Minus = 45, Equals, LeftBracket, RightBracket, Backslash,
    Semicolon = 51, Quote, BackQuote, Comma, Period, Slash, CapsLock,
    F1 = 58, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    PrintScreen = 70, ScrollLock, Pause, Insert, Home, PageUp, Delete, End, PageDown,
    RightArrow = 79, LeftArrow, DownArrow, UpArrow,
    LeftControl = 224, LeftShift, LeftAlt, LeftMeta, RightControl, RightShift, RightAlt, RightMeta,
};

// Mouse buttons, in the same order as Unity's GetMouseButton(0), (1) and (2).
enum class MouseButton : uint8_t { Left, Right, Middle };

// Keyboard and mouse state, like Unity's Input class. The engine updates it from the window's
// events at the start of every frame (see src/Platform/Input.cpp); read it from the main thread.
class Input {
public:
    Input() = delete;

    // True for every frame the key is held down. Like Input.GetKey.
    static bool GetKey(Key key);
    // True only in the frame the key went down. Like Input.GetKeyDown.
    static bool GetKeyDown(Key key);
    // True only in the frame the key was released. Like Input.GetKeyUp.
    static bool GetKeyUp(Key key);

    static bool GetMouseButton(MouseButton button);
    static bool GetMouseButtonDown(MouseButton button);
    static bool GetMouseButtonUp(MouseButton button);

    // Mouse position in window coordinates: points, not pixels, with (0, 0) at the top-left.
    static glm::vec2 MousePosition();
    // How far the mouse moved this frame, in points.
    static glm::vec2 MouseDelta();
    // How far the wheel turned this frame: positive is away from you. Like
    // Input.mouseScrollDelta.y.
    static float MouseScroll();

    // A readable name such as "W", "Space" or "Left Shift", for logs and UI.
    static std::string_view GetKeyName(Key key);

    // Locked: the cursor is hidden and stays put, while MouseDelta() keeps reporting movement, so
    // mouse look can turn forever without the cursor hitting the screen's edge. Like setting
    // Unity's Cursor.lockState to Locked. It only takes effect while the window has focus.
    static void SetCursorLocked(bool locked);
};

} // namespace Viva
