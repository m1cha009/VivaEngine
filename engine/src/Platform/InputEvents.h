#pragma once

// SDL's event type, declared without including SDL: Core's Application includes this header.
union SDL_Event;

namespace Viva {

// The engine's side of Input. Viva/Input.h is what games use to read input; these two functions
// are how the engine writes it. They're defined in Platform/Input.cpp, next to the input state.

// Clears last frame's "pressed/released this frame" flags, mouse movement and wheel. The main
// loop calls it once per frame, before handling that frame's events.
void BeginInputFrame();

// Updates the input state from one SDL event. Events that aren't keyboard or mouse input are
// ignored.
void ProcessInputEvent(const SDL_Event& event);

} // namespace Viva
