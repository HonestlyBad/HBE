#pragma once
#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/TileCollision.h"
#include "HBE/Renderer/Camera2D.h"

#include <vector>

namespace HBE::Renderer {
    class ResourceCache;
    class Mesh;
    class GLShader;
    class Renderer2D;
}

namespace MegaX {

    class EnemyBulletManager {
    public:
        struct Impact {
            float x = 0.0f;
            float y = 0.0f;
            int   tileId = 0;
        };

        struct Bullet {
            float x = 0.0f, y = 0.0f;
            float px = 0.0f, py = 0.0f;
            float vx = 0.0f, vy = 0.0f;
            int damage = 1;
            bool alive = true;
        };

        bool init(HBE::Renderer::ResourceCache& resources,
            HBE::Renderer::Mesh* quadMesh,
            HBE::Renderer::GLShader* spriteShader);

        void spawn(float sx, float sy, float aimX, float aimY, float speed, int damage);

        void update(float dt, const HBE::Renderer::TileMap* map, const HBE::Renderer::TileMapLayer* solidLayer, const HBE::Renderer::Camera2D& cam);
        void render(HBE::Renderer::Renderer2D& r2d, float alpha);
        void clear() { m_bullets.clear(); m_impacts.clear(); }

        bool consumeImpacts(std::vector<Impact>& out) {
            if (m_impacts.empty()) return false;
            out.insert(out.end(), m_impacts.begin(), m_impacts.end());
            m_impacts.clear();
            return true;
        }
        
        int count() const { return static_cast<int>(m_bullets.size()); }

        std::vector<Bullet>& bullets() { return m_bullets; }
        const std::vector<Bullet>& bullets() const { return m_bullets; }

        float length = 14.0f;
        float height = 4.0f;
        float offscreenTiles = 4.0f;

    private:
        bool pointInSolid(const HBE::Renderer::TileMap* map, const HBE::Renderer::TileMapLayer* layer, float x, float y) const; 

        std::vector<Bullet> m_bullets;
        std::vector<Impact> m_impacts;

        HBE::Renderer::Material m_material{};
        HBE::Renderer::RenderItem m_item{};
        HBE::Renderer::Mesh* m_quad = nullptr;
    };
}
