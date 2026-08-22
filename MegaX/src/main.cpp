#include "HBE/Core/Application.h"
#include "HBE/Core/Log.h"

#include "Game/GameLayer.h"
#include "Game/PerfCapture.h"

#include <cstring>
#include <cstdlib>
#include <memory>

using namespace HBE::Core;
using namespace HBE::Platform;

int main(int argc, char** argv) {
	SetLogLevel(LogLevel::Info);

	for (int i = 1; i < argc; ++i)
	{
		if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0)
		{
			MegaX::PrintCaptureUsage();
			return 0;
		}
	}

	float fixedHz = 0.0f;
	int fixedCatchUp = 0;
	float renderHz = 0.0f;

	for (int i = 1; i < argc; ++i)
	{
		if (std::strcmp(argv[i], "--fixed-hz") == 0 && i + 1 < argc)
		{
			fixedHz = static_cast<float>(std::atof(argv[++i]));
			continue;
		}
		if (std::strcmp(argv[i], "--fixed-catchup") == 0 && i + 1 < argc)
		{
			fixedCatchUp = std::atoi(argv[++i]);
			continue;
		}
		if (std::strcmp(argv[i], "--render-hz") == 0 && i + 1 < argc)
		{
			renderHz = static_cast<float>(std::atof(argv[++i]));
			continue;
		}
	}

	const MegaX::PerfCaptureRequest captureRequest = MegaX::ParseCaptureArgs(argc, argv);

	WindowConfig cfg;
	cfg.title = "MegaX";
	cfg.width = 1280;
	cfg.height = 720;
	cfg.useOpenGL = true;
	cfg.mode = WindowMode::Windowed;
	cfg.vsync = !captureRequest.disableVsync;

	AssetPaths::Config assetCfg{};
	assetCfg.organization = "MegaX";
	assetCfg.application = "MegaX";
	assetCfg.siblingProjectNames = { "MegaX" };

	Application app;
	if (!app.initialize(cfg, assetCfg)) {
		return -1;
	}

	if (fixedHz > 0.0f || fixedCatchUp > 0)
	{
		FixedTimestepConfig ts{};
		if (fixedHz > 0.0f) ts.hz = fixedHz;
		if (fixedCatchUp > 0) ts.maxCatchUpSteps = fixedCatchUp;
		app.setFixedTimestep(ts);
	}

	if (renderHz > 0.0f)
	{
		app.setTargetFrameRate(renderHz);
	}

	auto gameLayer = std::make_unique<MegaX::GameLayer>();
	gameLayer->setCaptureRequest(captureRequest);
	app.pushLayer(std::move(gameLayer));
	
	app.run();

	return 0;
}