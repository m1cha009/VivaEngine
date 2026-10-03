#pragma once

// SDL's event and window types, declared without including SDL: Core's Application includes
// this header.
union SDL_Event;
struct SDL_Window;

namespace Viva {

// The engine's side of Input. Viva/Input.h is what games use to read input; these two functions
// are how the engine writes it. They're defined in Platform/Input.cpp, next to the input state.

// Clears the "pressed/released this frame" flags, mouse movement and wheel. The main loop calls
// it right after the game's update has seen them, so the next update sees only newer input.
void BeginInputFrame();

// Updates the input state from one SDL event. Events that aren't keyboard or mouse input are
// ignored.
void ProcessInputEvent(const SDL_Event& event);

// Tells Input which window Input::SetCursorLocked acts on. Window sets it when it's created and
// clears it (nullptr) when it's destroyed.
void SetInputWindow(SDL_Window* window);

} // namespace Viva
