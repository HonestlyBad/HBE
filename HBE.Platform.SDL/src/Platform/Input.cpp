#include "HBE/Platform/Input.h"

#include <algorithm>
#include <cstring> // std::memcpy
#include <cmath>

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

	static inline int ClampInt(int v, int lo, int hi) {
		return (v < lo) ? lo : (v > hi) ? hi : v;
	}

	static inline float ClampFloat(float v, float lo, float hi) {
		return (v < lo) ? lo : (v > hi) ? hi : v;
	}

	// ---------------- Gamepad state ----------------
	static SDL_Gamepad* g_gamepad = nullptr;
	static constexpr int kMaxPadButtons = 64;

	static bool g_currPadButtons[kMaxPadButtons];
	static bool g_prevPadButtons[kMaxPadButtons];

	static constexpr int kMaxPadAxes = 16;
	static float g_currPadAxes[kMaxPadAxes];
	static float g_prevPadAxes[kMaxPadAxes];

	static inline int ClampMouseIndex(int idx) {
		return ClampInt(idx, 0, kMaxMouseButtons - 1);
	}
	static inline int ClampPadButtonIndex(int idx) {
		return ClampInt(idx, 0, kMaxPadButtons - 1);
	}
	static inline int ClampPadAxisIndex(int idx) {
		return ClampInt(idx, 0, kMaxPadAxes - 1);
	}

	static void CloseGamepad() {
		if (g_gamepad) {
			SDL_CloseGamepad(g_gamepad);
			g_gamepad = nullptr;
		}
	}

	static void TryOpenFirstGamepad() {
		if (g_gamepad) return;

		int count = 0;
		SDL_JoystickID* ids = SDL_GetGamepads(&count);
		if (!ids || count <= 0) {
			if (ids) SDL_free(ids);
			return;
		}

		// Open the first available
		g_gamepad = SDL_OpenGamepad(ids[0]);
		SDL_free(ids);

		// Clear states on open
		std::memset(g_currPadButtons, 0, sizeof(g_currPadButtons));
		std::memset(g_prevPadButtons, 0, sizeof(g_prevPadButtons));
		std::memset(g_currPadAxes, 0, sizeof(g_currPadAxes));
		std::memset(g_prevPadAxes, 0, sizeof(g_prevPadAxes));
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

		std::memset(g_currPadButtons, 0, sizeof(g_currPadButtons));
		std::memset(g_prevPadButtons, 0, sizeof(g_prevPadButtons));
		std::memset(g_currPadAxes, 0, sizeof(g_currPadAxes));
		std::memset(g_prevPadAxes, 0, sizeof(g_prevPadAxes));

		TryOpenFirstGamepad();

		g_initialized = true;
	}

	void Shutdown() {
		CloseGamepad();
		g_initialized = false;
	}

	void NewFrame() {
		if (!g_initialized) return;

		// previous = current
		std::memcpy(g_prevKeys, g_currentKeys, sizeof(g_currentKeys));
		std::memcpy(g_prevMouse, g_currentMouse, sizeof(g_currentMouse));

		std::memcpy(g_prevPadButtons, g_currPadButtons, sizeof(g_currPadButtons));
		std::memcpy(g_prevPadAxes, g_currPadAxes, sizeof(g_currPadAxes));

		// reset per-frame accumulators
		g_mouseDeltaX = 0.0f;
		g_mouseDeltaY = 0.0f;
		g_wheelX = 0.0f;
		g_wheelY = 0.0f;
	}

	void HandleEvent(const SDL_Event& e) {
		if (!g_initialized) return;

		switch (e.type) {
			// -------- Keyboard --------
		case SDL_EVENT_KEY_DOWN: {
			SDL_Scancode sc = e.key.scancode;
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

							 // -------- Mouse --------
		case SDL_EVENT_MOUSE_MOTION: {
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
			g_wheelX += e.wheel.x;
			g_wheelY += e.wheel.y;

			g_mouseX = e.wheel.mouse_x;
			g_mouseY = e.wheel.mouse_y;
			break;
		}

								  // -------- Gamepad device --------
		case SDL_EVENT_GAMEPAD_ADDED: {
			// If no pad open, try to open first available
			TryOpenFirstGamepad();
			break;
		}
		case SDL_EVENT_GAMEPAD_REMOVED: {
			// If our open pad disappeared, close and try reopen.
			CloseGamepad();
			TryOpenFirstGamepad();
			break;
		}

									  // -------- Gamepad input --------
		case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
			const int b = ClampPadButtonIndex(static_cast<int>(e.gbutton.button));
			g_currPadButtons[b] = true;
			break;
		}
		case SDL_EVENT_GAMEPAD_BUTTON_UP: {
			const int b = ClampPadButtonIndex(static_cast<int>(e.gbutton.button));
			g_currPadButtons[b] = false;
			break;
		}
		case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
			const int ax = ClampPadAxisIndex(static_cast<int>(e.gaxis.axis));

			// SDL gives Sint16 typically; SDL3 event exposes .value as Sint16
			// Normalize:
			const float raw = static_cast<float>(e.gaxis.value);

			float norm = 0.0f;
			// sticks: [-32768..32767], triggers often [0..32767] depending on backend
			if (raw >= 0.0f) norm = raw / 32767.0f;
			else norm = raw / 32768.0f;

			g_currPadAxes[ax] = ClampFloat(norm, -1.0f, 1.0f);
			break;
		}

		default:
			break;
		}
	}

	// -------- Keyboard queries --------
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

	// -------- Mouse queries --------
	static inline int MouseEnumToIndex(MouseButton b) {
		return ClampMouseIndex(static_cast<int>(b));
	}

	bool IsMouseDown(MouseButton button) {
		if (!g_initialized) return false;
		const int idx = MouseEnumToIndex(button);
		return g_currentMouse[idx];
	}

	bool IsMousePressed(MouseButton button) {
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

	float GetMouseWheelX() { return g_wheelX; }
	float GetMouseWheelY() { return g_wheelY; }

	// -------- Gamepad queries --------
	bool HasGamepad() {
		return g_gamepad != nullptr;
	}

	bool IsGamepadButtonDown(SDL_GamepadButton button) {
		if (!g_initialized) return false;
		const int b = ClampPadButtonIndex(static_cast<int>(button));
		return g_currPadButtons[b];
	}

	bool IsGamepadButtonPressed(SDL_GamepadButton button) {
		if (!g_initialized) return false;
		const int b = ClampPadButtonIndex(static_cast<int>(button));
		return g_currPadButtons[b] && !g_prevPadButtons[b];
	}

	bool IsGamepadButtonReleased(SDL_GamepadButton button) {
		if (!g_initialized) return false;
		const int b = ClampPadButtonIndex(static_cast<int>(button));
		return !g_currPadButtons[b] && g_prevPadButtons[b];
	}

	float GetGamepadAxis(SDL_GamepadAxis axis) {
		if (!g_initialized) return 0.0f;
		const int ax = ClampPadAxisIndex(static_cast<int>(axis));
		return g_currPadAxes[ax];
	}

} // namespace HBE::Platform::Input