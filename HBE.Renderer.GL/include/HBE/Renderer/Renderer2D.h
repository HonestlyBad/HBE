#pragma once

#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Camera2D.h"

namespace HBE::Renderer {
	class GLRenderer;

	// high-level 2D renderer that wraps a specific backend (currently GLRender)
	class Renderer2D {
	public:
		explicit Renderer2D(GLRenderer& backend);

		// set up the camera for this scene
		void beginScene(const Camera2D& camera);

		// for now this is a no-op but later we can flush batches
		void endScene();
		
		// draw a single 2D item
		void draw(const RenderItem& item);

	private:
		GLRenderer& m_backend;
		const Camera2D* m_activeCamera = nullptr;
	};
}