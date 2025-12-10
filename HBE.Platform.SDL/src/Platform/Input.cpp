#include "HBE/Platform/Input.h"

#include <array>
#include <cstring> // std::memcpy

namespace HBE::Platform::Input {

	// SDL scancodes are in the 0-511 range; 512 is more than enough
	static constexpr int kMaxKeys = 512;

	static bool g_currentKeys[kMaxKeys];
	static bool g_prevKeys[kMaxKeys];
	static bool g_initialized = false;

	void Initialize() {
		if (g_initialized) return;
		std::memset(g_currentKeys, 0, sizeof(g_currentKeys));
		std::memset(g_prevKeys, 0, sizeof(g_prevKeys));
		g_initialized = true;
	}

	void Shutdown() {
		g_initialized = false;
	}

	void NewFrame() {
		if (!g_initialized) return;
		// previous = current
		std::memcpy(g_prevKeys, g_currentKeys, sizeof(g_currentKeys));
	}

	void HandleEvent(const SDL_Event& e) {
		if (!g_initialized) return;

		switch (e.type) {
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
}