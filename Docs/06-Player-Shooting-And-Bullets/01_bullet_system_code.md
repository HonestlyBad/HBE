# Item 06 · Doc 01 — The Bullet System

Two new files. Create them, then register them in the project (doc 03).

---

## 1. `include/Game/Bullet.h`

```cpp
#pragma once

#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/TileCollision.h"   // TileMap, TileMapLayer
#include "HBE/Renderer/Camera2D.h"

#include <vector>

namespace HBE::Renderer {
	class ResourceCache;
	class Mesh;
	class GLShader;
	class Renderer2D;
}

namespace MegaX {

	// Owns and simulates the player's bullets. Rendered as sleek, tinted quads.
	// Bullets die when they hit a solid tile or travel past the view + margin.
	class BulletManager {
	public:
		bool init(HBE::Renderer::ResourceCache& resources,
			HBE::Renderer::Mesh* quadMesh,
			HBE::Renderer::GLShader* spriteShader);

		// Spawn a bullet at world (x,y) travelling horizontally in dir (+1 / -1).
		void spawn(float x, float y, int dir);

		// Advance bullets; cull those that hit a solid tile or leave the view.
		void update(float dt,
			const HBE::Renderer::TileMap* map,
			const HBE::Renderer::TileMapLayer* solidLayer,
			const HBE::Renderer::Camera2D& cam);

		void render(HBE::Renderer::Renderer2D& r2d);

		void clear() { m_bullets.clear(); }
		int  count() const { return static_cast<int>(m_bullets.size()); }

		// tunables
		float speed = 640.0f;        // world px / sec
		float offscreenTiles = 4.0f; // despawn this many tiles past the view edge
		float length = 16.0f;        // visual length (world px)
		float height = 4.0f;         // visual thickness (world px)

	private:
		struct Bullet {
			float x = 0.0f, y = 0.0f;
			float vx = 0.0f;
			bool  alive = true;
		};

		bool pointInSolid(const HBE::Renderer::TileMap* map,
			const HBE::Renderer::TileMapLayer* layer,
			float x, float y) const;

		std::vector<Bullet> m_bullets;

		HBE::Renderer::Material   m_material{};
		HBE::Renderer::RenderItem m_item{};
		HBE::Renderer::Mesh*      m_quad = nullptr;
	};
}
```

---

## 2. `src/Game/Bullet.cpp`

```cpp
#include "Game/Bullet.h"

#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/Renderer2D.h"

#include <algorithm>
#include <cmath>

using namespace HBE::Renderer;

namespace MegaX {

	bool BulletManager::init(ResourceCache& resources, Mesh* quadMesh, GLShader* spriteShader) {
		if (!quadMesh || !spriteShader) return false;

		const unsigned char white[4] = { 255, 255, 255, 255 };
		Texture2D* tex = resources.getOrCreateTextureFromRGBA("megax_white1x1", 1, 1, white);
		if (!tex) return false;

		m_quad = quadMesh;
		m_material.shader  = spriteShader;
		m_material.texture = tex;   // solid white; tinted per draw

		m_item.mesh = m_quad;
		m_item.material = &m_material;
		m_item.layer = 101;                                   // in front of the player (100)
		m_item.pass = RenderPass::World;
		m_item.tint = Color4{ 1.0f, 0.95f, 0.4f, 1.0f };      // sleek bright bullet
		return true;
	}

	void BulletManager::spawn(float x, float y, int dir) {
		Bullet b;
		b.x = x;
		b.y = y;
		b.vx = (dir >= 0 ? 1.0f : -1.0f) * speed;
		b.alive = true;
		m_bullets.push_back(b);
	}

	bool BulletManager::pointInSolid(const TileMap* map, const TileMapLayer* layer, float x, float y) const {
		if (!map || !layer) return false;
		const float tw = map->worldTileW();
		const float th = map->worldTileH();
		const int tx = static_cast<int>(std::floor(x / tw));
		const int ty = static_cast<int>(std::floor(y / th));
		const int id = layer->at(tx, ty);
		if (id == 0) return false;
		return map->tilesets[layer->tilesetIndex].isSolid(id);
	}

	void BulletManager::update(float dt, const TileMap* map, const TileMapLayer* layer, const Camera2D& cam) {
		const float zoom = (cam.zoom <= 0.0f) ? 1.0f : cam.zoom;
		const float halfW = cam.viewportWidth  / (2.0f * zoom);
		const float halfH = cam.viewportHeight / (2.0f * zoom);
		const float tw = map ? map->worldTileW() : 32.0f;
		const float margin = offscreenTiles * tw;
		const float minX = cam.x - halfW - margin, maxX = cam.x + halfW + margin;
		const float minY = cam.y - halfH - margin, maxY = cam.y + halfH + margin;

		for (auto& b : m_bullets) {
			if (!b.alive) continue;
			b.x += b.vx * dt;
			if (pointInSolid(map, layer, b.x, b.y)) { b.alive = false; continue; }
			if (b.x < minX || b.x > maxX || b.y < minY || b.y > maxY) b.alive = false;
		}

		m_bullets.erase(
			std::remove_if(m_bullets.begin(), m_bullets.end(),
				[](const Bullet& b) { return !b.alive; }),
			m_bullets.end());
	}

	void BulletManager::render(Renderer2D& r2d) {
		for (const auto& b : m_bullets) {
			m_item.transform.posX = b.x;
			m_item.transform.posY = b.y;
			m_item.transform.scaleX = length;
			m_item.transform.scaleY = height;
			r2d.draw(m_item);
		}
	}
}
```

### Notes
- **`getOrCreateTextureFromRGBA`** (engine `ResourceCache`) makes a reusable 1×1
  white texture; the `RenderItem::tint` colours it, so all bullets share one
  material and batch into a single draw call. `Texture2D` is visible via the
  `ResourceCache.h` include.
- **`pointInSolid`** mirrors the player's tile scan: `layer->at(tx,ty)` →
  `tilesets[tilesetIndex].isSolid(id)`. It uses the **same `Ground` layer** the
  player collides against, so bullets and the player agree on what's solid.
- The off‑screen test converts the camera's logical viewport to world units with
  `viewport / (2*zoom)` and adds a `4 tile` margin, matching the work‑item spec.
- `render` reuses one `RenderItem`, just re‑stamping position/scale per bullet.

Next: `02_player_shooting_code.md`.
