#pragma once

#include <vector>
#include <cstdint>

namespace HBE::Renderer {

	class Material;
	class Mesh;
	struct RenderItem;
	
	// Batches "sprite-style" quads (pos+uv) into a few draw calls.
	// Assumes the mesh submitted is the engine's standard unit quad
	// centered at origin with UVs 0..1 ( current quad_pos_uv).
	class SpriteBatch2D {
	public:
		SpriteBatch2D() = default;
		~SpriteBatch2D();

		SpriteBatch2D(const SpriteBatch2D&) = delete;
		SpriteBatch2D& operator=(const SpriteBatch2D&) = delete;

		void setQuadMesh(const Mesh* quadMesh) { m_quadMesh = quadMesh; }
		void begin(); // reset per frame
		void submit(const RenderItem& item);
		void flush(const float* viewProj); // draws queued quads

		// stats ( nice to haves, optional)
		int drawCalls() const { return m_drawCalls; }
		int quadCount() const { return m_quadsSubmitted; }

	private:
		struct Quad {
			const Material* material = nullptr;
			int layer = 0;
			float sortKey = 0.0f;
			uint32_t order = 0;
			float v[6 * 5]; // 6 verts * (x, y, z, u, v)
		};

		uint32_t m_orderCounter = 0;
		
		static bool quadLess(const Quad& a, const Quad& b);

		const Mesh* m_quadMesh = nullptr;

		bool m_glInited = false;
		unsigned int m_vao = 0;
		unsigned int m_vbo = 0;

		// Cpu-side queue of quads (sorted before drawing)
		std::vector<Quad> m_quads;

		// CPU staging buffer for one flush group
		std::vector<float> m_vertexStaging;

		// capacity control
		int m_maxQuadsPerFlush = 5000; // tune-able

		// stats
		int m_drawCalls = 0;
		int m_quadsSubmitted = 0;

		void initGL();
		void destroyGL();

		void emitQuadVertices(const RenderItem& item, float out30[30]) const;

		static bool materialLess(const Quad& a, const Quad& b);
		void drawRange(const Material* mat, const float* viewProj, const float* verts, int vertexCount);
	};
}