#pragma once

#include <string> 
#include "HBE/Platform/SDLPlatform.h"

namespace HBE::Platform {

	// high-level graphics settings the game can tweak
	struct GraphicsSettings {
		int width = 1280;
		int height = 720;

		WindowMode mode = WindowMode::Windowed; // Windowed / FullScreenDesktop
		bool vsync = true;

		// 0 - uncapped (only vsync), > 0 = simple frame cap in main loop
		int targetFPS = 60;
	};

	// Load from a simple key=value text file. Returs true on success.
	bool LoadGraphicsSettings(GraphicsSettings& out, const std::string& path);

	// Save to simple key=value text file, returns true on success.
	bool SaveGraphicsSettings(const GraphicsSettings& gs, const std::string& path);
}