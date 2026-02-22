#pragma once
#include "HBE/Renderer/TileMap.h"

namespace HBE::Renderer {

	struct AABB {
		// center-based to match Transform2D + quad mesh
		float cx = 0.0f, cy = 0.0f;
		float w = 0.0f, h = 0.0f;
	};

	struct MoveResult2D {
		bool hitX = false;
		bool hitY = false;

		bool grounded = false; // hit something while moving down (or slope snap)
		bool ceiling = false;  // hit something while moving up

		bool steppedUp = false; // used step-up during horizontal motion
	};

	class TileCollision {
	public:
		// True for fully-solid tiles ("block"), not including one-way platforms.
		static bool isSolidTile(const TileMap& map, const TileMapLayer& layer, int tx, int ty);

		static bool isOneWayTile(const TileMap& map, const TileMapLayer& layer, int tx, int ty);
		static SlopeType slopeTypeAt(const TileMap& map, const TileMapLayer& layer, int tx, int ty);

		// Backwards-compatible basic mover (solid tiles only, no one-way/slopes/step-up).
		static void moveAndCollide(
			const TileMap& map,
			const TileMapLayer& layer,
			AABB& box,
			float& velX,
			float& velY,
			float dt
		);

		// Physics-lite mover:
		static MoveResult2D moveAndCollideEx(
			const TileMap& map,
			const TileMapLayer& layer,
			AABB& box,
			float& velX,
			float& velY,
			float dt,
			float maxStepUp,
			bool enableOneWay,
			bool enableSlopes,
			float oneWayPrevBottom // pass previous bottom world-space Y for correct one-way landing
		);
	};
}