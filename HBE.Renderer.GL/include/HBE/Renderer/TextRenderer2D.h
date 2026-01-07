#pragma once
#include <string>
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/Color.h"

namespace HBE::Renderer {

	class Renderer2D;
	class ResourceCache;
	class Mesh;
	class GLShader;
	class Texture2D;

	class TextRenderer2D {
	public:
		bool initialize(ResourceCache& cache, GLShader* spriteShader, Mesh* quadMesh);

		// x, y are screen-space pixles (origin bottom-left)
		void drawText(Renderer2D& r2d, float x, float y, const std::string& text, float pixelScale = 2.0f, Color4 tint = { 1,1,1,1 });

		int glyphW() const { return m_glyphW; }
		int glyphH() const { return m_glyphH; }

	private:
		Texture2D* m_fontTex = nullptr;
		Material m_mat{};
		Mesh* m_quad = nullptr;

		int m_texW = 0, m_texH = 0;
		int m_glyphW = 8, m_glyphH = 8;
		int m_cols = 16;

		void uvForChar(unsigned char c, float outUV[4]) const;
		bool buildDebugAtlas(ResourceCache& cache);
	};
}