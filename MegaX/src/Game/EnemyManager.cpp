#include "Game/EnemyManager.h"
#include "Game/EnemyBullet.h"

#include "Game/Bullet.h"
#include "Game/Effects.h"
#include "Game/Player.h"

#include "HBE/Renderer/DebugDraw2D.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Core/Log.h"

#include <algorithm>
#include <cmath>

using namespace HBE::Renderer;



namespace MegaX {

    DifficultyProfile MakeProfile(Difficulty d) {
        DifficultyProfile p{};
        switch (d) {
        case Difficulty::Casual:
            p.chaseSpeedMul = 0.50f;
            p.sightRangeMul = 0.75f;
            p.hearingRadiusMul = 0.75f;
            p.loseAggroDelayMul = 0.55f;
            p.startHpMul = 0.66f;
            p.bulletDamage = 1;
            p.fireCooldownSec = 1.60f;
            p.bulletSpeed = 380.0f;
            p.leadFactor = 0.0f;
            p.label = "Casual";
            p.labelR = 0.35f; p.labelG = 1.0f; p.labelB = 0.35f; // green
            break;
        case Difficulty::Difficult:
            p.chaseSpeedMul = 1.35f;
            p.sightRangeMul = 1.10f;
            p.hearingRadiusMul = 1.10f;
            p.loseAggroDelayMul = 1.20f;
            p.startHpMul = 1.00f;
            p.bulletDamage = 1;
            p.fireCooldownSec = 0.90f;
            p.bulletSpeed = 480.0f;
            p.leadFactor = 0.0f;
            p.label = "Difficult";
            p.labelR = 1.0f; p.labelG = 0.9f; p.labelB = 0.25f; // yellow
            break;
        case Difficulty::Challenging:
            p.chaseSpeedMul = 1.85f;
            p.sightRangeMul = 1.35f;
            p.hearingRadiusMul = 1.25f;
            p.loseAggroDelayMul = 2.00f;
            p.startHpMul = 1.66f;
            p.bulletDamage = 2;
            p.fireCooldownSec = 0.55f;
            p.bulletSpeed = 560.0f;
            p.leadFactor = 1.0f;
            p.label = "Challenging";
            p.labelR = 1.0f; p.labelG = 0.3f; p.labelB = 0.3f; // red
            break;
        }
        return p;
    }

    EnemyManager::EnemyManager() = default;
    EnemyManager::~EnemyManager() = default;

    EnemyBulletManager& EnemyManager::enemyBullets() { return *m_enemyBullets; }
    const EnemyBulletManager& EnemyManager::enemyBullets() const { return *m_enemyBullets; }

    bool EnemyManager::init(ResourceCache& resources, Mesh* quadMesh, GLShader* spriteShader) {
        if (!quadMesh || !spriteShader) {
            HBE::Core::LogError("EnemyManager::init: quadMesh or spriteShader is null.");
            return false;
        }
        m_resources = &resources;
        m_quadMesh = quadMesh;
        m_spriteShader = spriteShader;

        m_enemyBullets = std::make_unique<EnemyBulletManager>();
        if (!m_enemyBullets->init(resources, quadMesh, spriteShader)) {
            HBE::Core::LogError("EnemyManager::init: enemy bullet manager init failed.");
            return false;
        }

        m_difficulty = Difficulty::Difficult;
        m_profile    = MakeProfile(m_difficulty);
        return true;
    }

    Enemy* EnemyManager::spawn(float x, float groundY, int facing) {
        if (!m_resources || !m_quadMesh || !m_spriteShader) return nullptr;
        m_enemies.emplace_back();
        Enemy& e = m_enemies.back();
        if (!e.init(*m_resources, m_quadMesh, m_spriteShader)) {
            m_enemies.pop_back();
            return nullptr;
        }
        if (m_map && m_solid) e.setCollision(m_map, m_solid);
        e.spawn(x, groundY, facing);

        e.snapshotBaseStats();
        e.applyDifficulty(m_profile);
        e.setFireCallback(
            [](void* ctx, float sx, float sy, float aimX, float aimY, float speed, int damage) {
                static_cast<EnemyManager*>(ctx)->m_enemyBullets->spawn(sx, sy, aimX, aimY, speed, damage);
            },
            this);
        return &e;
    }

    void EnemyManager::setDifficulty(Difficulty d) {
        m_difficulty = d;
        m_profile    = MakeProfile(d);
        for (auto& e : m_enemies) {
            if (!e.isAlive()) continue;
            e.applyDifficulty(m_profile);
        }
    }

    void EnemyManager::setCollision(const TileMap* map, const TileMapLayer* solid) {
        m_map = map;
        m_solid = solid;
        for (auto& e : m_enemies) e.setCollision(m_map, m_solid);
    }
    int EnemyManager::checkBulletHits(BulletManager& bullets, Effects* effects, int damagePerBullet) {
        if (m_enemies.empty()) return 0;
        int hits = 0;
        auto& list = bullets.bullets();
        for (auto& b : list) {
            if (!b.alive) continue;
            for (auto& e : m_enemies) {
                if (!e.hurtboxActive()) continue;
                const AABB h = e.hurtbox();
                if (std::fabs(b.x - h.cx) > h.w * 0.5f) continue;
                if (std::fabs(b.y - h.cy) > h.h * 0.5f) continue;
                if (e.takeDamage(damagePerBullet)) {
                    b.alive = false;
                    ++hits;
                    if (effects) effects->spawnBulletImpact(b.x, b.y, 0);
                    break;
                }
            }
        }
        return hits;
    }

    void EnemyManager::update(float dt) {
        if (!m_player) return;

        for (auto& e : m_enemies) e.tick(dt, *m_player);
        m_enemies.erase(
            std::remove_if(m_enemies.begin(), m_enemies.end(),
                [](const Enemy& e) { return e.isFinished(); }),
            m_enemies.end());
    }

    void EnemyManager::render(Renderer2D& r2d) {
        for (auto& e : m_enemies) e.render(r2d);
    }

    // --------------------------------------------------- gunshot broadcast
    void EnemyManager::notifyGunshot(float sx, float sy) {
        for (auto& e : m_enemies) {
            if (!e.isAlive()) continue;
            const float dx = sx - e.x();
            const float dy = sy - e.y();
            const float r = e.hearingRingRadius();   // reuse ring for viz
            // Use each enemy's own gunshotHearRadius as the trigger check.
            // We can't read that tunable through a const-ref -- it's public,
            // so query via the mutable vector cast below.
            (void)r; (void)dx; (void)dy;
        }
        // Simplify: iterate mutable and use the public field.
        for (auto& e : m_enemies) {
            if (!e.isAlive()) continue;
            const float dx = sx - e.x();
            const float dy = sy - e.y();
            const float r = e.gunshotHearRadius;
            if (dx * dx + dy * dy <= r * r) e.onHeardGunshot(sx, sy);
        }
    }

    int EnemyManager::aliveCount() const {
        int n = 0; for (auto& e : m_enemies) if (e.isAlive()) ++n; return n;
    }

    // ------------------------------------------------------ debug boxes (Item 08)
    void EnemyManager::debugDrawBoxes(DebugDraw2D& dbg, Renderer2D& r2d) const {
        for (const auto& e : m_enemies) {
            if (e.isFinished()) continue;
            const AABB h = e.hurtbox();
            if (e.isDying())
                dbg.rect(r2d, h.cx, h.cy, h.w, h.h, 0.7f, 0.7f, 0.7f, 1.0f, false);
            else
                dbg.rect(r2d, h.cx, h.cy, h.w, h.h, 0.25f, 1.0f, 0.35f, 1.0f, false);

            if (e.hitboxActive()) {
                const AABB k = e.hitbox();
                dbg.rect(r2d, k.cx, k.cy, k.w, k.h, 1.0f, 0.35f, 0.35f, 1.0f, false);
            }
        }
    }

    // ------------------------------------------------------ bubbles (Item 09)
    // DebugDraw2D only draws axis-aligned rects, so both "?" and "!" are
    // composed from 2 rects: a stem + a dot below. Colors:
    //   Question = yellow  (heard, unsure)
    //   Exclaim  = red     (has seen the player)
    void EnemyManager::renderBubbles(DebugDraw2D& dbg, Renderer2D& r2d) const {
        for (const auto& e : m_enemies) {
            if (e.isFinished()) continue;
            const Enemy::BubbleIcon icon = e.bubbleIcon();
            if (icon == Enemy::BubbleIcon::None) continue;

            const float bx = e.x();
            const float top = e.feetY() + 60.0f;   // 60 px above feet == above head
            const float stemH = 10.0f, stemW = 3.0f, dotSize = 3.0f;
            const float dotY = top - stemH * 0.5f - 4.0f;

            float r, g, b, a = 1.0f;
            if (icon == Enemy::BubbleIcon::Question) { r = 1.0f; g = 0.9f; b = 0.2f; }
            else { r = 1.0f; g = 0.25f; b = 0.25f; }

            // The Question mark is offset slightly to imply the curve; the
            // Exclaim is a straight vertical stroke with a dot underneath.
            const float stemX = (icon == Enemy::BubbleIcon::Question) ? bx + 2.0f : bx;

            dbg.rect(r2d, stemX, top, stemW, stemH, r, g, b, a, true);
            dbg.rect(r2d, bx, dotY, dotSize, dotSize, r, g, b, a, true);
        }
    }

    // ------------------------------------------------------ sense overlay (Item 09)
    // Vision cone: sample the two cone edges as tiny dots along the line
    // from eye to cone-tip; also draw a short arc between them.
    // Hearing radius: sample the circle perimeter as tiny dots.
    static void debugDrawLine(DebugDraw2D& dbg, Renderer2D& r2d,
        float x0, float y0, float x1, float y1,
        float r, float g, float b, float a) {
        const float dx = x1 - x0, dy = y1 - y0;
        const float len = std::sqrt(dx * dx + dy * dy);
        const int steps = std::max(2, static_cast<int>(len / 6.0f));
        for (int i = 0; i <= steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            dbg.rect(r2d, x0 + dx * t, y0 + dy * t, 2.0f, 2.0f, r, g, b, a, true);
        }
    }
    static void debugDrawCircle(DebugDraw2D& dbg, Renderer2D& r2d,
        float cx, float cy, float radius,
        float r, float g, float b, float a) {
        constexpr int N = 32;
        for (int i = 0; i < N; ++i) {
            const float th = (i / static_cast<float>(N)) * 6.28318530f;
            dbg.rect(r2d, cx + std::cos(th) * radius, cy + std::sin(th) * radius,
                2.0f, 2.0f, r, g, b, a, true);
        }
    }
    static void debugDrawArc(DebugDraw2D& dbg, Renderer2D& r2d,
        float cx, float cy, float radius,
        float thStart, float thEnd,
        float r, float g, float b, float a) {
        constexpr int N = 20;
        for (int i = 0; i <= N; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(N);
            const float th = thStart + (thEnd - thStart) * t;
            dbg.rect(r2d, cx + std::cos(th) * radius, cy + std::sin(th) * radius,
                2.0f, 2.0f, r, g, b, a, true);
        }
    }

    void EnemyManager::debugDrawSenses(DebugDraw2D& dbg, Renderer2D& r2d) const {
        for (const auto& e : m_enemies) {
            if (!e.isAlive()) continue;

            // Hearing ring (blue, transparent) centered on enemy anchor.
            debugDrawCircle(dbg, r2d, e.x(), e.y(),
                e.hearingRingRadius(), 0.35f, 0.55f, 1.0f, 0.9f);

            // Vision cone (yellow) rooted at the eye.
            const float ex = e.eyeX();
            const float ey = e.eyeY();
            const float range = e.visionRange();
            const float halfDeg = e.visionHalfAngle();
            const float baseTh = (e.facing() >= 0) ? 0.0f : 3.14159265f;
            const float halfRad = halfDeg * 3.14159265f / 180.0f;
            const float th0 = baseTh - halfRad;
            const float th1 = baseTh + halfRad;

            debugDrawLine(dbg, r2d, ex, ey,
                ex + std::cos(th0) * range, ey + std::sin(th0) * range,
                1.0f, 0.9f, 0.2f, 1.0f);
            debugDrawLine(dbg, r2d, ex, ey,
                ex + std::cos(th1) * range, ey + std::sin(th1) * range,
                1.0f, 0.9f, 0.2f, 1.0f);
            debugDrawArc(dbg, r2d, ex, ey, range, th0, th1,
                1.0f, 0.9f, 0.2f, 1.0f);
        }
    }
}