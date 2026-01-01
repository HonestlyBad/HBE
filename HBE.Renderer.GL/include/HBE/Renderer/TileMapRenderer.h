#pragma once
#include <vector>
#include "HBE/Renderer/TileMap.h"
#include "HBE/Renderer/Material.h"

namespace HBE::Renderer {

	class Renderer2D;
	class ResourceCache;
	class Mesh;
	class GLShader;

	class TileMapRenderer {
	public:
		bool build(Renderer2D& r2d,
			ResourceCache& cache,
			GLShader* spriteShader,
			Mesh* quadMesh,
			const TileMap& map);
		void draw(Renderer2D& r2d, const TileMap& map);
	private:
		struct TilesetDrawData {
			Material material;
			int texW = 0;
			int texH = 0;
			int tileW = 16;
			int tileH = 16;
			int margin = 0;
			int spacing = 0;
		};

		Mesh* m_quadMesh = nullptr;
		std::vector<TilesetDrawData> m_tilesets;

		void computeTileUV(const TilesetDrawData& ts, int tileIndex, float outUV[4]) const;
	};
}