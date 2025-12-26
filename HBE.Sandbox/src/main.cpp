#include "HBE/Core/Application.h"
#include "HBE/Core/Log.h"

#include "GameLayer.h"

using namespace HBE::Core;
using namespace HBE::Platform;

int main() {
	SetLogLevel(LogLevel::Trace);

	WindowConfig cfg;
	cfg.title = "HBE Sandbox - OpenGL";
	cfg.width = 1280;
	cfg.height = 720;
	cfg.useOpenGL = true;
	cfg.mode = WindowMode::Windowed;
	cfg.vsync = true;

	Application app;
	if (!app.initialize(cfg)) {
		LogFatal("SandBox: app.initialize failed.");
		return -1;
	}

	app.pushLayer(std::make_unique<GameLayer>());
	app.run();

	return 0;
}