#pragma once

#include "Game/Enemy.h"

#include <vector>

namespace HBE::Renderer {
    class ResourceCache;
    class Mesh;
    class GLShader;
    class Renderer2D;
    class DebugDraw2D;
    struct TileMap;
    struct TileMapLayer;
}

namespace MegaX {

    class BulletManager;
    class Effects;
    class Player;

    class EnemyManager {
    public:
        bool init(HBE::Renderer::ResourceCache& resources,
            HBE::Renderer::Mesh* quadMesh,
            HBE::Renderer::GLShader* spriteShader);

        Enemy* spawn(float x, float groundY, int facing);

        int checkBulletHits(BulletManager& bullets, Effects* effects,
            int damagePerBullet = 1);

        // Item 09: needs player+collision refs set BEFORE update(). Call
        // both once in GameLayer::onAttach after init/spawn.
        void setPlayerRef(const Player* p) { m_player = p; }
        void setCollision(const HBE::Renderer::TileMap* map,
            const HBE::Renderer::TileMapLayer* solidLayer);

        void update(float dt);
        void render(HBE::Renderer::Renderer2D& r2d);

        // Item 08 debug boxes (hitbox/hurtbox outlines).
        void debugDrawBoxes(HBE::Renderer::DebugDraw2D& dbg,
            HBE::Renderer::Renderer2D& r2d) const;

        // Item 09: always-on "?" and "!" bubbles above each enemy.
        // Called every frame regardless of the B toggle so gameplay
        // feedback is always visible.
        void renderBubbles(HBE::Renderer::DebugDraw2D& dbg,
            HBE::Renderer::Renderer2D& r2d) const;

        // Item 09: draws each enemy's vision cone + hearing radius.
        // Gated by the B toggle in GameLayer.
        void debugDrawSenses(HBE::Renderer::DebugDraw2D& dbg,
            HBE::Renderer::Renderer2D& r2d) const;

        // Item 09: broadcast a gunshot ping from (sx, sy). Each alive
        // enemy within its own `gunshotHearRadius` gets bumped to
        // Suspicious (see Enemy::onHeardGunshot).
        void notifyGunshot(float sx, float sy);

        int aliveCount() const;
        const std::vector<Enemy>& enemies() const { return m_enemies; }
        std::vector<Enemy>& enemies() { return m_enemies; }

        void clear() { m_enemies.clear(); }

    private:
        HBE::Renderer::ResourceCache* m_resources = nullptr;
        HBE::Renderer::Mesh* m_quadMesh = nullptr;
        HBE::Renderer::GLShader* m_spriteShader = nullptr;

        // Stored refs (not owned)
        const Player* m_player = nullptr;
        const HBE::Renderer::TileMap* m_map = nullptr;
        const HBE::Renderer::TileMapLayer* m_solid = nullptr;

        std::vector<Enemy> m_enemies;
    };
}