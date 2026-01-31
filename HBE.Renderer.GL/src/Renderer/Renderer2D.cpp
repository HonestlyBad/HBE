#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/GLRenderer.h"
#include "HBE/Renderer/Camera2D.h"
#include "HBE/Renderer/SpriteBatch2D.h"
#include "HBE/Renderer/Mesh.h"

namespace HBE::Renderer {

	Renderer2D::Renderer2D(GLRenderer& backend) : m_backend(backend) {}

	void Renderer2D::ensureBatch() {
		if (!m_batch) {
			m_batch = new SpriteBatch2D();
			m_batch->setQuadMesh(m_spriteQuadMesh);
		}
	}

	void Renderer2D::setSpriteQuadMesh(const Mesh* quadMesh) {
		m_spriteQuadMesh = quadMesh;
		if (m_batch) {
			m_batch->setQuadMesh(m_spriteQuadMesh);
		}
	}

	void Renderer2D::beginScene(const Camera2D& camera) {
		m_activeCamera = &camera;
		m_backend.setCamera(camera);
		
		ensureBatch();
		m_batch->begin();
	}

	void Renderer2D::endScene() {
		// Flush Batched sprite quads
		if (m_batch) {
			float vp[16];
			m_backend.getViewProjection(vp);
			m_batch->flush(vp);
		}

		m_activeCamera = nullptr;
	}

	void Renderer2D::draw(const RenderItem& item) {
		// Batch only sprite-quads.
		if (m_batch && m_spriteQuadMesh && item.mesh == m_spriteQuadMesh) {
			m_batch->submit(item);
			return;
		}
		// Fallback for everything else (debug draw meshes, etc.)
		m_backend.draw(item);
	}
}