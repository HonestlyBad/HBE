#pragma once

#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_events.h>

namespace HBE::Platform::Input {

	// Mouse buttons that maps to SDL Button Indices
	enum class MouseButton : int {
		Left = 1,
		Middle = 2,
		Right = 3,
		X1 = 4,
		X2 = 5
	};

	// call once at startup and shutdown
	void Initialize();
	void Shutdown();

	// call once per frame before processing events
	void NewFrame();

	// Feed every SDL_Event into this from SDLPlatform::pollquitrequested
	void HandleEvent(const SDL_Event& e);

	// Keyboard query functions
	bool IsKeyDown(SDL_Scancode key); // held this frame
	bool IsKeyPressed(SDL_Scancode key); // went from up -> down this frame
	bool IsKeyReleased(SDL_Scancode key); // went from down -> up this frame

	// Mouse query functions
	bool IsMouseDown(MouseButton button); // held this frame
	bool IsMousePressed(MouseButton button); // went from up -> down this frame
	bool IsMouseReleased(MouseButton button); // went from down -> up this frame

	// Mouse Position in window cordinates (top-left origin, pixels as floats)
	float GetMouseX();
	float GetMouseY();

	// per-frame delta accumulated from motion events (pixels)
	float GetMouseDeltaX();
	float GetMouseDeltaY();

	// per-frame wheel delta accumulated from wheel events (SDL convention: +Y = away from user)
	float GetMouseWheelX();
	float GetMouseWheelY();
}