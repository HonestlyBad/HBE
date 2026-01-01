#pragma once
#include "HBE/Renderer/TileMap.h"

namespace HBE::Renderer {
	struct AABB {
		// center-based t0 match Transform2D + quad mesh
		float cx = 0, cy = 0;
		float w = 0, h = 0;
	};

	class TileCollision {
	public:
		static bool isSolidTile(const TileMap& map, const TileMapLayer& layer, int tx, int ty);

		static void moveAndCollide(
			const TileMap& map,
			const TileMapLayer& layer,
			AABB& box,
			float& velX,
			float& velY,
			float dt
		);
	};
}