#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_gamepad.h>

namespace HBE::Input {

	// High-level semantic inputs used by gameplay + UI.
	enum class Action : std::uint16_t {
		Jump,
		Attack,
		UIConfirm,
		UICancel,
		Pause,
		FullscreenToggle,
	};

	enum class Axis : std::uint16_t {
		MoveX, // -1 left, +1 right
		MoveY, // -1 up, +1 down
	};

	// A single binding: keyboard key, mouse button, gamepad button, or gamepad axis threshold.
	struct Binding {
		enum class Type : std::uint8_t {
			None,
			Key,
			MouseButton,
			GamepadButton,
			GamepadAxisThreshold,
		};

		Type type = Type::None;

		// For Key: SDL_Scancode
		SDL_Scancode key = SDL_SCANCODE_UNKNOWN;

		// For MouseButton: SDL button index (1..)
		int mouseButton = 0;

		// For GamepadButton
		SDL_GamepadButton padButton = SDL_GAMEPAD_BUTTON_INVALID;

		// For GamepadAxisThreshold
		SDL_GamepadAxis padAxis = SDL_GAMEPAD_AXIS_INVALID;
		float axisThreshold = 0.5f; // trigger when abs(axis) >= threshold (or sign check below)
		int axisSign = 0;          // 0 = abs, -1 = negative only, +1 = positive only

		static Binding None() { return {}; }
		static Binding Key(SDL_Scancode sc) { Binding b; b.type = Type::Key; b.key = sc; return b; }
		static Binding Mouse(int button) { Binding b; b.type = Type::MouseButton; b.mouseButton = button; return b; }
		static Binding GamepadButton(SDL_GamepadButton btn) { Binding b; b.type = Type::GamepadButton; b.padButton = btn; return b; }
		static Binding GamepadAxis(SDL_GamepadAxis axis, float threshold = 0.5f, int sign = 0) {
			Binding b; b.type = Type::GamepadAxisThreshold; b.padAxis = axis; b.axisThreshold = threshold; b.axisSign = sign; return b;
		}
	};

	struct ActionBinding {
		Binding primary;
		Binding secondary;
	};

	struct AxisBinding {
		// Digital bindings (keyboard / dpad buttons) for composite axis.
		Binding negative;
		Binding positive;
		Binding negative2;
		Binding positive2;

		// Optional analog binding.
		bool useGamepadAxis = false;
		SDL_GamepadAxis gamepadAxis = SDL_GAMEPAD_AXIS_INVALID;
		float deadzone = 0.20f;
		bool invert = false;
		float scale = 1.0f;
	};

	class InputMap {
	public:
		InputMap() = default;

		// NEW: Engine doesn't own defaults anymore; game does.
		void clear();

		// Call once per frame AFTER Platform::Input::NewFrame().
		void newFrame();

		// Feed SDL events here to support rebinding.
		void handleEvent(const SDL_Event& e);

		// Query
		bool actionDown(Action a) const;
		bool actionPressed(Action a) const;
		bool actionReleased(Action a) const;
		float axis(Axis ax) const;

		// Rebinding
		void beginRebind(Action a, bool primary = true);
		void cancelRebind();
		bool isRebinding() const { return m_rebinding; }

		// Direct binding edits
		void bindAction(Action a, const Binding& b, bool primary = true);
		void bindAxis(Axis ax, const AxisBinding& b);
		const ActionBinding& getActionBinding(Action a) const;
		const AxisBinding& getAxisBinding(Axis ax) const;

		// Persistence (optional, can be stubbed initially)
		bool saveToFile(const std::string& path) const;
		bool loadFromFile(const std::string& path);

		static const char* actionName(Action a);
		static const char* axisName(Axis a);

	private:
		bool bindingDown(const Binding& b) const;
		bool bindingPressed(const Binding& b) const;
		bool bindingReleased(const Binding& b) const;
		float axisValueForBinding(const AxisBinding& b) const;

		float currentAxisForThresholdBinding(const Binding& b) const;
		float prevAxisForThresholdBinding(const Binding& b) const;

		std::unordered_map<Action, ActionBinding> m_actions;
		std::unordered_map<Axis, AxisBinding> m_axes;

		// Axis history used for threshold-based actions (edge detection).
		mutable std::unordered_map<int, float> m_prevAxis;
		mutable std::unordered_map<int, float> m_currAxis;

		// Rebind state
		bool m_rebinding = false;
		bool m_rebindPrimary = true;
		Action m_rebindAction = Action::Jump;
	};

	// ---------------------------------------------------------
	// Global convenience API
	// ---------------------------------------------------------

	// NEW: game supplies defaults through a callback.
	using DefaultBindingsFn = void(*)(InputMap& map);

	void Initialize(DefaultBindingsFn defaultsProvider);
	void Shutdown();
	void NewFrame();
	void HandleEvent(const SDL_Event& e);

	bool ActionDown(Action a);
	bool ActionPressed(Action a);
	bool ActionReleased(Action a);
	float AxisValue(Axis a);

	void BeginRebind(Action a, bool primary = true);
	void CancelRebind();
	bool IsRebinding();

	InputMap& Get();

} // namespace HBE::Input