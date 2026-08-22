#pragma once

#include "Game/Enemy.h"

#include <memory>
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

    enum class Difficulty {
        Casual,
        Difficult,
        Challenging
    };

    struct DifficultyProfile {
        float chaseSpeedMul = 1.0f;
        float sightRangeMul = 1.0f;
        float hearingRadiusMul = 1.0f;
        float loseAggroDelayMul = 1.0f;
        float startHpMul = 1.0f;

        int bulletDamage = 1;
        float fireCooldownSec = 0.90f;
        float bulletSpeed = 480.0f;
        float leadFactor = 0.0f;

        const char* label = "Difficult";
        float labelR = 1.0f, labelG = 0.9f, labelB = 0.25f;
    };

    DifficultyProfile MakeProfile(Difficulty d);

    class BulletManager;
    class Effects;
    class Player;

    class EnemyManager {
    public:
        EnemyManager();
        ~EnemyManager();

        bool init(HBE::Renderer::ResourceCache& resources,
            HBE::Renderer::Mesh* quadMesh,
            HBE::Renderer::GLShader* spriteShader);

        Enemy* spawn(float x, float groundY, int facing);

        int checkBulletHits(BulletManager& bullets, Effects* effects,

            int damagePerBullet = 1);
        void setPlayerRef(const Player* p) { m_player = p; }
        void setCollision(const HBE::Renderer::TileMap* map,
            const HBE::Renderer::TileMapLayer* solidLayer);

        void setEffects(class Effects* fx) { m_effects = fx; }
        Effects* effects() const { return m_effects; }

        void fixedUpdate(float h);
        void updateVisual(float dt);
        void render(HBE::Renderer::Renderer2D& r2d, float alpha);

        void debugDrawBoxes(HBE::Renderer::DebugDraw2D& dbg,
            HBE::Renderer::Renderer2D& r2d) const;
        void renderBubbles(HBE::Renderer::DebugDraw2D& dbg,
            HBE::Renderer::Renderer2D& r2d) const;
        void debugDrawSenses(HBE::Renderer::DebugDraw2D& dbg,
            HBE::Renderer::Renderer2D& r2d) const;
        void notifyGunshot(float sx, float sy);
        void setDifficulty(Difficulty d);
        Difficulty difficulty() const { return m_difficulty; }
        const DifficultyProfile& profile() const { return m_profile; }

        class EnemyBulletManager& enemyBullets();
        const class EnemyBulletManager& enemyBullets() const;

        float walkDustPeriod = 0.18f;
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

        Effects* m_effects = nullptr;

        Difficulty m_difficulty = Difficulty::Difficult;
        DifficultyProfile m_profile{};

        std::unique_ptr<class EnemyBulletManager> m_enemyBullets;

        std::vector<Enemy> m_enemies;
    };
}