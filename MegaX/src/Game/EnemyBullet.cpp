#include "Game/EnemyBullet.h"

#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/Renderer2D.h"

#include <algorithm>
#include <cmath>

using namespace HBE::Renderer;

namespace MegaX {
	bool EnemyBulletManager::init(ResourceCache& resources, Mesh* quadMesh, GLShader* spriteShader) {
		if (!quadMesh || !spriteShader) return false;

		const unsigned char white[4] = { 255, 255, 255, 255 };
		Texture2D* tex = resources.getOrCreateTextureFromRGBA("megax_white1x1", 1, 1, white);
		if (!tex) return false;

		m_quad = quadMesh;
		m_material.shader = spriteShader;
		m_material.texture = tex;

		m_item.mesh = m_quad;
		m_item.material = &m_material;
		m_item.layer = 101;
		m_item.pass = RenderPass::World;
		m_item.tint = Color4{ 1.0f, 0.35f, 0.35f, 1.0f };
		return true;
	}

	void EnemyBulletManager::spawn(float sx, float sy, float aimX, float aimY, float speed, int damage) {
		float dx = aimX - sx;
		float dy = aimY - sy;
		const float len = std::sqrt(dx * dx + dy * dy);
		if (len < 0.0001f) {
			dx = 1.0f; dy = 0.0f;
		}
		else {
			dx /= len; dy /= len;
		}
		Bullet b;
		b.x = sx;
		b.y = sy;
		b.vx = dx * speed;
		b.vy = dy * speed;
		b.damage = std::max(1, damage);
		b.alive = true;
		m_bullets.push_back(b);
	}

	bool EnemyBulletManager::pointInSolid(const TileMap* map, const TileMapLayer* layer, float x, float y) const {
		if (!map || !layer) return false;
		const float tw = map->worldTileW();
		const float th = map->worldTileH();
		if (tw <= 0.0f || th <= 0.0f) return false;
		const int tx = static_cast<int>(std::floor(x / tw));
		const int ty = static_cast<int>(std::floor(y / th));
		const int id = layer->at(tx, ty);
		if (id == 0) return false;
		return map->tilesets[layer->tilesetIndex].isSolid(id);
	}

	void EnemyBulletManager::update(float dt, const TileMap* map, const TileMapLayer* layer, const Camera2D& cam) {
		const float zoom = (cam.zoom <= 0.0f) ? 1.0f : cam.zoom;
		const float halfW = cam.viewportWidth / (2.0f * zoom);
		const float halfH = cam.viewportHeight / (2.0f * zoom);
		const float tw = map ? map->worldTileW() : 32.0f;
		const float margin = offscreenTiles * tw;
		const float minX = cam.x - halfW - margin, maxX = cam.x + halfW + margin;
		const float minY = cam.y - halfH - margin, maxY = cam.y + halfH + margin;

		for (auto& b : m_bullets) {
			if (!b.alive) continue;
			b.x += b.vx * dt;
			b.y += b.vy * dt;

			if (pointInSolid(map, layer, b.x, b.y)) {
				int tileId = 0;
				if (map && layer) {
					const float tw = map->worldTileW();
					const float th = map->worldTileH();
					if (tw > 0.0f && th > 0.0f) {
						const int tx = static_cast<int>(std::floor(b.x / tw));
						const int ty = static_cast<int>(std::floor(b.y / th));
						tileId = layer->at(tx, ty);
					}
				}
				m_impacts.push_back(Impact{ b.x, b.y, tileId });
				b.alive = false;
				continue;
			}
			if (b.x < minX || b.x > maxX || b.y < minY || b.y > maxY) {
				b.alive = false;
			}
		}

		m_bullets.erase(std::remove_if(m_bullets.begin(), m_bullets.end(), [](const Bullet& b) {return !b.alive; }), m_bullets.end());
	}

	void EnemyBulletManager::render(Renderer2D& r2d) {
		for (auto& b : m_bullets) {
			if (!b.alive) continue;
			m_item.transform.posX = b.x;
			m_item.transform.posY = b.y;
			const float ang = std::atan2(b.vy, b.vx);
			m_item.transform.rotation = ang;
			m_item.transform.scaleX = length;
			m_item.transform.scaleY = height;
			r2d.draw(m_item);
		}
	}
}