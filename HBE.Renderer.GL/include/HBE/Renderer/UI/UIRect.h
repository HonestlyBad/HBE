#pragma once

namespace HBE::Renderer::UI {

	struct UIRect {
		float x = 0.0f; // bottom-left
		float y = 0.0f; // bottom-left
		float w = 0.0f;
		float h = 0.0f;

		float left() const { return x; }
		float right() const { return x + w; }
		float bottom() const { return y; }
		float top() const { return y + h; }

		bool contains(float px, float py) const {
			return (px >= left() && px <= right() && py >= bottom() && py <= top());
		}
	};
}