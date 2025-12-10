#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/GLRenderer.h"
#include "HBE/Renderer/Camera2D.h"

namespace HBE::Renderer {

	Renderer2D::Renderer2D(GLRenderer& backend) : m_backend(backend) {}

	void Renderer2D::beginScene(const Camera2D& camera) {
		m_activeCamera = &camera;
		m_backend.setCamera(camera);
		// later: reset stats, prepare batches, etc.
	}

	void Renderer2D::endScene() {
		// later: flush batches, stats, debug, etc.
		m_activeCamera = nullptr;
	}

	void Renderer2D::draw(const RenderItem& item) {
		// for now just forward directly to the backend.
		// later this can batch quads, sort by texture, etc.
		m_backend.draw(item);
	}
}