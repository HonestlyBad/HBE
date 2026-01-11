#include "HBE/Platform/Input.h"

#include <algorithm>
#include <cstring> // std::memcpy

namespace HBE::Platform::Input {

	// SDL scancodes are in the 0-511 range; 512 is more than enough
	static constexpr int kMaxKeys = 512;

	static bool g_currentKeys[kMaxKeys];
	static bool g_prevKeys[kMaxKeys];
	static bool g_initialized = false;

	// SDL Mouse buttons are 1..5 (Left, Middle, Right, X1, X2)
	static constexpr int kMaxMouseButtons = 8; // plenty
	static bool g_currentMouse[kMaxMouseButtons];
	static bool g_prevMouse[kMaxMouseButtons];

	static float g_mouseX = 0.0f;
	static float g_mouseY = 0.0f;
	static float g_mouseDeltaX = 0.0f;
	static float g_mouseDeltaY = 0.0f;
	static float g_wheelX = 0.0f;
	static float g_wheelY = 0.0f;

	static inline int ClampMouseIndex(int idx) {
		return std::clamp(idx, 0, kMaxMouseButtons - 1);
	}

	void Initialize() {
		if (g_initialized) return;

		std::memset(g_currentKeys, 0, sizeof(g_currentKeys));
		std::memset(g_prevKeys, 0, sizeof(g_prevKeys));
		std::memset(g_currentMouse, 0, sizeof(g_currentMouse));
		std::memset(g_prevMouse, 0, sizeof(g_prevMouse));

		g_mouseX = g_mouseY = 0.0f;
		g_mouseDeltaX = g_mouseDeltaY = 0.0f;
		g_wheelX = g_wheelY = 0.0f;

		g_initialized = true;
	}

	void Shutdown() {
		g_initialized = false;
	}

	void NewFrame() {
		if (!g_initialized) return;

		// previous = current
		std::memcpy(g_prevKeys, g_currentKeys, sizeof(g_currentKeys));
		std::memcpy(g_prevMouse, g_currentMouse, sizeof(g_currentMouse));

		// reset per-frame accumulators
		g_mouseDeltaX = 0.0f;
		g_mouseDeltaY = 0.0f;
		g_wheelX = 0.0f;
		g_wheelY = 0.0f;
	}

	void HandleEvent(const SDL_Event& e) {
		if (!g_initialized) return;

		switch (e.type) {
		// keybaord
		case SDL_EVENT_KEY_DOWN: {
			SDL_Scancode sc = e.key.scancode; // SDL3: scancode field
			if (sc >= 0 && sc < kMaxKeys) {
				g_currentKeys[sc] = true;
			}
			break;
		}
		case SDL_EVENT_KEY_UP: {
			SDL_Scancode sc = e.key.scancode;
			if (sc >= 0 && sc < kMaxKeys) {
				g_currentKeys[sc] = false;
			}
			break;
		}
		// mosue
		case SDL_EVENT_MOUSE_MOTION: {
			// SDL3 reports window_relative coords as floats
			g_mouseX = e.motion.x;
			g_mouseY = e.motion.y;
			g_mouseDeltaX += e.motion.xrel;
			g_mouseDeltaY += e.motion.yrel;
			break;
		}
		case SDL_EVENT_MOUSE_BUTTON_DOWN: {
			g_mouseX = e.button.x;
			g_mouseY = e.button.y;
			const int idx = ClampMouseIndex(static_cast<int>(e.button.button));
			g_currentMouse[idx] = true;
			break;
		}
		case SDL_EVENT_MOUSE_BUTTON_UP: {
			g_mouseX = e.button.x;
			g_mouseY = e.button.y;
			const int idx = ClampMouseIndex(static_cast<int>(e.button.button));
			g_currentMouse[idx] = false;
			break;
		}
		case SDL_EVENT_MOUSE_WHEEL: {
			// SDL3 has both x/y and integer_x/integer_y: use x/y for smooth scrolling
			g_wheelX += e.wheel.x;
			g_wheelY += e.wheel.y;

			// wheel event includes mouse position (window relative)
			g_mouseX = e.wheel.mouse_x;
			g_mouseY = e.wheel.mouse_y;
			break;
		}
		default:
			break;
		}
	}
	
	bool IsKeyDown(SDL_Scancode key) {
		if (!g_initialized || key < 0 || key >= kMaxKeys) return false;
		return g_currentKeys[key];
	}

	bool IsKeyPressed(SDL_Scancode key) {
		if (!g_initialized || key < 0 || key >= kMaxKeys) return false;
		return g_currentKeys[key] && !g_prevKeys[key];
	}

	bool IsKeyReleased(SDL_Scancode key) {
		if (!g_initialized || key < 0 || key >= kMaxKeys) return false;
		return !g_currentKeys[key] && g_prevKeys[key];
	}

	static inline int MouseEnumToIndex(MouseButton b) {
		return ClampMouseIndex(static_cast<int>(b));
	}

	bool IsMouseDown(MouseButton button) {
		if (!g_initialized) return false;
		const int idx = MouseEnumToIndex(button);
		return g_currentMouse[idx] && !g_prevMouse[idx];
	}

	bool IsMouseReleased(MouseButton button) {
		if (!g_initialized) return false;
		const int idx = MouseEnumToIndex(button);
		return !g_currentMouse[idx] && g_prevMouse[idx];
	}

	float GetMouseX() { return g_mouseX; }
	float GetMouseY() { return g_mouseY; }
	
	float GetMouseDeltaX() { return g_mouseDeltaX; }
	float GetMouseDeltaY() { return g_mouseDeltaY; }
	
	float GetMOuseWheelX() { return g_wheelX; }
	float GetMouseWheelY() { return g_wheelY; }
}