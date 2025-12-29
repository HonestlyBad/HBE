#pragma once

#include <cstdint>

namespace HBE::Core {

	enum class EventType {
		None = 0,
		WindowResize,
		KeyPressed,
	};

	class Event {
	public:
		virtual ~Event() = default;

		bool handled = false;
		virtual EventType type() const = 0;
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
}