#pragma once

#include <cstdint>

namespace HBE::Core {

	enum class EventType {
		None = 0,
		WindowResize,
		KeyPressed,

		MouseMoved,
		MouseButtonPressed,
		MouseButtonReleased,
		MouseScrolled
	};

	class Event {
	public:
		virtual ~Event() = default;
		virtual EventType type() const { return EventType::None; }

		bool handled = false;
	};

	class WindowResizeEvent final : public Event {
	public:
		WindowResizeEvent(int widthPx, int heightPx, int viewportX, int viewportY, int viewportW, int viewportH) : width(widthPx), height(heightPx), vpX(viewportX), vpY(viewportY), vpW(viewportW), vpH(viewportH) {}

		EventType type() const override { return EventType::WindowResize; }

		int width = 0;
		int height = 0;

		// viewport rect actually used for rendering (letterbox)
		int vpX = 0;
		int vpY = 0;
		int vpW = 0;
		int vpH = 0;
	};

	class KeyPressedEvent final : public Event {
	public:
		KeyPressedEvent(int scancode, bool isRepeat) : keyScancode(scancode), repeat(isRepeat) {}

		EventType type() const override { return EventType::KeyPressed; }

		int keyScancode = 0; // sdl_scancode value
		bool repeat = false;
	};

	// --------------- Mouse Events -------------------
	class MouseMovedEvent final : public Event {
	public:
		MouseMovedEvent(float x, float y, float dx, float dy, bool inViewport, float logicalX, float logicalY)
			: x(x), y(y), dx(dx), dy(dy), inViewport(inViewport), logicalX(logicalX), logicalY(logicalY) {
		}

		EventType type() const override { return EventType::MouseMoved; }

		// window coords (top-left origin)
		float x = 0.0f;
		float y = 0.0f;
		float dx = 0.0f;
		float dy = 0.0f;

		// Letterboxed logical space( 0... logicalW, 0...LogicalH) if inside viewport
		bool inViewport = false;
		float logicalX = 0.0f;
		float logicalY = 0.0f;
	};

	class MouseButtonPressedEvent final : public Event {
	public:
		MouseButtonPressedEvent(int button, int clicks, float x, float y, bool inViewport, float logicalX, float logicalY)
			: button(button), clicks(clicks), x(x), y(y), inViewport(inViewport), logicalX(logicalX), logicalY(logicalY) {
		}

		EventType type() const override { return EventType::MouseButtonPressed; }

		int button = 0; // SDL mouse button index (1..5)
		int clicks = 0;
		
		float x = 0.0f;
		float y = 0.0f;

		bool inViewport = false;
		float logicalX = 0.0f;
		float logicalY = 0.0f;
	};

	class MouseButtonReleasedEvent final : public Event {
	public:
		MouseButtonReleasedEvent(int button, float x, float y, bool inViewport, float logicalX, float logicalY)
			: button(button), x(x), y(y), inViewport(inViewport), logicalX(logicalX), logicalY(logicalY) {
		}

		EventType type() const override { return EventType::MouseButtonReleased; }

		int button = 0; // SDL mouse button index (1..5)

		float x = 0.0f;
		float y = 0.0f;

		bool inViewport = false;
		float logicalX = 0.0f;
		float logicalY = 0.0f;
	};

	class MouseScrolledEvent final : public Event {
	public:
		MouseScrolledEvent(float wheelX, float wheelY, float mouseX, float mouseY, bool inViewport, float logicalX, float logicalY)
			: wheelX(wheelX), wheelY(wheelY), mouseX(mouseX), mouseY(mouseY), inViewport(inViewport), logicalX(logicalX), logicalY(logicalY) {
		}

		EventType type() const override { return EventType::MouseScrolled; }

		float wheelX = 0.0f; // +right, -left
		float wheelY = 0.0f; // +away, -toward (user)

		float mouseX = 0.0f;
		float mouseY = 0.0f;

		bool inViewport = false;
		float logicalX = 0.0f;
		float logicalY = 0.0f;
	};
}