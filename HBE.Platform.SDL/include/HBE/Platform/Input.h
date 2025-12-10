#pragma once

#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_events.h>

namespace HBE::Platform::Input {

	// call once at startup and shutdown
	void Initialize();
	void Shutdown();

	// call once per frame before processing events
	void NewFrame();

	// Feed every SDL_Event into this from SDLPlatform::pollquitrequested
	void HandleEvent(const SDL_Event& e);

	// query functions
	bool IsKeyDown(SDL_Scancode key); // held this frame
	bool IsKeyPressed(SDL_Scancode key); // went from up -> down this frame
	bool IsKeyReleased(SDL_Scancode key); // went from down -> up this frame
}