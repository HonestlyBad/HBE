#pragma once

#include "HBE/Core/LayerStack.h"

#include "HBE/Platform/SDLPlatform.h"
#include "HBE/Renderer/GLRenderer.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/ResourceCache.h"

namespace HBE::Core {

	class Application {
	public:
		Application() = default;
		~Application();

		Application(const Application&) = delete;
		Application& operator=(const Application&) = delete;

		bool initialize(const HBE::Platform::WindowConfig& windowCfg);

		void run();
		void requestQuit() { m_running = false;	}

		void pushLayer(std::unique_ptr<Layer> layer);
		void pushOverlay(std::unique_ptr<Layer> overlay);

		// accessors for layers
		HBE::Platform::SDLPlatform& platform() { return m_platform; }
		HBE::Renderer::GLRenderer& gl() { return m_gl; }
		HBE::Renderer::Renderer2D& renderer2D() { return m_renderer2D; }
		HBE::Renderer::ResourceCache& resources() { return m_resources; }

		int windowWidthPixels() const { return m_winW; }
		int windowHeightPixels() const { return m_winH; }

	private:
		bool m_initialized = false;
		bool m_running = false;

		HBE::Platform::SDLPlatform m_platform;

		HBE::Renderer::GLRenderer m_gl;
		HBE::Renderer::Renderer2D m_renderer2D{ m_gl };
		HBE::Renderer::ResourceCache m_resources;

		LayerStack m_layers;

		int m_winW = 0;
		int m_winH = 0;

		void syncWindowSizeAndViewport();
	};
}