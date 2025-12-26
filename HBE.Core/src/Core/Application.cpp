#include "HBE/Core/Application.h"

#include "HBE/Core/Log.h"
#include "HBE/Core/Time.h"
#include "HBE/Platform/Input.h"

#include <SDL3/SDL.h>

namespace HBE::Core {

	using HBE::Core::LogError;
	using HBE::Core::LogInfo;
	using HBE::Core::LogFatal;

	Application::~Application() {
		m_layers.clear();
		// SDLPlatform destructur already calls shutdown()
	}

	bool Application::initialize(const HBE::Platform::WindowConfig& windowCfg) {
		if (m_initialized) return true;

		if (!m_platform.initialize(windowCfg)) {
			LogFatal("Application: SDLPlatform init failed.");
			return false;
		}

		if (!m_gl.initialize(m_platform)) {
			LogFatal("Application: GLRenderer init failed.");
			return false;
		}

		syncWindowSizeAndViewport();

		m_initialized = true;
		LogInfo("Application initialized");
		return true;
	}

	void Application::pushLayer(std::unique_ptr<Layer> layer) {
		m_layers.pushLayer(std::move(layer), *this);
	}

	void Application::pushOverlay(std::unique_ptr<Layer>overlay) {
		m_layers.pushOverlay(std::move(overlay), *this);
	}

	void Application::syncWindowSizeAndViewport() {
		int w = 0, h = 0;
		SDL_GetWindowSizeInPixels(m_platform.getWindow(), &w, &h);

		if (w <= 0 || h <= 0) {
			w = 1280;
			h = 720;
		}

		m_winW = w;
		m_winH = h;
		

		// Note: GLRenderer::resizeViewport early-outs if not initialized but at this point it is.
		m_gl.resizeViewport(m_winW, m_winH);
	}

	void Application::run() {
		if (!m_initialized) {
			LogError("Application::run called before initialized()");
			return;
		}

		m_running = true;

		double prevTime = GetTimeSeconds();

		while (m_running) {
			//Start input frame
			HBE::Platform::Input::NewFrame();

			// OS events (also feeds Input::HandleEvent inside SDLPlatform)
			if (m_platform.pollQuitRequested()) {
				m_running = false;
				break;
			}

			// dt
			double now = GetTimeSeconds();
			float dt = static_cast<float>(now - prevTime);
			if (dt < 0.0f) dt = 0.0f;
			if (dt > 0.25f) dt = 0.25f; // clamp big hitches
			prevTime = now;

			// resize check
			int curW = 0, curH = 0;
			SDL_GetWindowSizeInPixels(m_platform.getWindow(), &curW, &curH);
			if (curW != m_winW || curH != m_winH) {
				syncWindowSizeAndViewport();
			}

			// update
			for (auto& layer : m_layers) {
				if (layer) layer->onUpdate(dt);
			}

			// render
			m_gl.beginFrame();
			for (auto& layer : m_layers) {
				if (layer) layer->onRender();
			}
			m_gl.endFrame(m_platform);

			// simple "don't melt cpu" delay
			m_platform.delayMillis(1);
		}

		LogInfo("Application exiting run loop.");
	}
}