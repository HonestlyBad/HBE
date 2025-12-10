#pragma once

namespace HBE::Renderer {

	// simple 2D camera for an orthographic world
	struct Camera2D {
		float x = 0.0f;
		float y = 0.0f;

		// 1.0 = normal, >1 zooms in, <1 zooms out
		float zoom = 1.0f;

		// logical viewport size (in world units, e.g pixels)
		float viewportWidth = 1280.0f;
		float viewportHeight = 720.0f;
	};
}