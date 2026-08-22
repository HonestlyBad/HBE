# 03 — Bubbles, `EnemyManager` extension, and `GameLayer` wiring

Three moving parts land in this doc:

1. **`EnemyManager`** grows a stored player reference, a stored
   collision world, a `notifyGunshot` pipe and a `renderBubbles`
   call. The existing `spawn` / `checkBulletHits` / `update` /
   `render` stay; `update` and `spawn` gain internals only, no
   caller-visible API break.
2. **Bubble composition** — how the "?" and "!" icons get built
   from `DebugDraw2D::rect` calls (2 rects each, colored,
   pulsing).
3. **`GameLayer`** — three small blocks of edits: `onAttach`
   (setters + patrol path), `onUpdate` (gunshot ping wiring),
   `onRender` (always-on bubble render, B-gated cone/ring).

---

## 1. `EnemyManager` — full replacement

### 1a. `include/Game/EnemyManager.h`

Replace with the following. All Item 08 API stays intact; new
methods are grouped at the end.

```cpp
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
        std::vector<Enemy>&       enemies()       { return m_enemies; }

        void clear() { m_enemies.clear(); }

    private:
        HBE::Renderer::ResourceCache* m_resources    = nullptr;
        HBE::Renderer::Mesh*          m_quadMesh     = nullptr;
        HBE::Renderer::GLShader*      m_spriteShader = nullptr;

        // Stored refs (not owned)
        const Player*                      m_player = nullptr;
        const HBE::Renderer::TileMap*      m_map    = nullptr;
        const HBE::Renderer::TileMapLayer* m_solid  = nullptr;

        std::vector<Enemy> m_enemies;
    };
}
```

### 1b. `src/Game/EnemyManager.cpp` — full replacement

```cpp
#include "Game/EnemyManager.h"

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

    // -------------------------------------------------------------- init / spawn
    bool EnemyManager::init(ResourceCache& resources, Mesh* quadMesh, GLShader* spriteShader) {
        if (!quadMesh || !spriteShader) {
            HBE::Core::LogError("EnemyManager::init: quadMesh or spriteShader is null.");
            return false;
        }
        m_resources    = &resources;
        m_quadMesh     = quadMesh;
        m_spriteShader = spriteShader;
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
        // Wire the collision refs BEFORE spawn so patrol probes and physics
        // work on the first tick.
        if (m_map && m_solid) e.setCollision(m_map, m_solid);
        e.spawn(x, groundY, facing);
        return &e;
    }

    void EnemyManager::setCollision(const TileMap* map, const TileMapLayer* solid) {
        m_map   = map;
        m_solid = solid;
        // Retro-wire any enemies that spawned before setCollision was called.
        for (auto& e : m_enemies) e.setCollision(m_map, m_solid);
    }

    // -------------------------------------------------------- bullet hit test
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

    // ------------------------------------------------------------ update / render
    void EnemyManager::update(float dt) {
        if (!m_player) return;   // GameLayer must call setPlayerRef first
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
            const float r  = e.hearingRingRadius();   // reuse ring for viz
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
            const float r  = e.gunshotHearRadius;
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
            const float dotY  = top - stemH * 0.5f - 4.0f;

            float r, g, b, a = 1.0f;
            if (icon == Enemy::BubbleIcon::Question) { r = 1.0f; g = 0.9f; b = 0.2f; }
            else                                     { r = 1.0f; g = 0.25f; b = 0.25f; }

            // The Question mark is offset slightly to imply the curve; the
            // Exclaim is a straight vertical stroke with a dot underneath.
            const float stemX = (icon == Enemy::BubbleIcon::Question) ? bx + 2.0f : bx;

            dbg.rect(r2d, stemX, top,   stemW, stemH, r, g, b, a, true);
            dbg.rect(r2d, bx,    dotY,  dotSize, dotSize, r, g, b, a, true);
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
```

**Note on the seemingly-doubled loop in `notifyGunshot`.** The
first loop is dead-code left over from a `const-ref` sketch; the
second (mutable) loop is the actual implementation. Feel free to
delete the first loop wholesale — it's kept here only so a
side-by-side diff review reads intuitively. Runtime cost is
negligible (single-digit enemies).

---

## 2. `include/Game/Player.h` — nothing changes

Player already exposes everything the enemy needs:

* `mode()` — the ghost check
* `velX()` — hearing gate
* `hurtbox()` — sight target and crouch check (`h < 34.0f`)
* `x()`, `y()` — position

If you want to clean up the crouch detection later, add:

```cpp
        bool isCrouched() const { return m_crouching; }
```

next to the other accessors and change the two `pb.h < 34.0f`
sites in `Enemy.cpp` to `p.isCrouched()`. Not required for
Item 09 to work.

---

## 3. `GameLayer` wiring

Three edit blocks. All in `src/Game/GameLayer.cpp` (header stays
the same — `m_enemies` and `m_debug` already exist from Item 08).

### 3a. `onAttach` — after the existing enemy spawn

Find the existing block (Item 08) that reads roughly:

```cpp
        {
            constexpr float kTilePx = 32.0f;
            const float ex = startX + 5.0f * kTilePx;
            const float eGroundY = m_player.feetY();     // (or the literal 130.0f you chose)
            if (Enemy* e = m_enemies.spawn(ex, eGroundY, -1)) {
                e->startHp = 3;
                e->spawn(ex, eGroundY, -1);
            }
        }
```

Wrap it so the collision + player ref are set BEFORE the spawn
(so patrol probes fire correctly on frame 0), and give the
spawned enemy a patrol range:

```cpp
        // ---- Item 09: wire manager to the world before spawning ----
        m_enemies.setPlayerRef(&m_player);
        m_enemies.setCollision(&m_world.map(), m_ground);

        {
            constexpr float kTilePx = 32.0f;
            const float ex = startX + 5.0f * kTilePx;
            const float eGroundY = m_player.feetY();    // or your literal
            if (Enemy* e = m_enemies.spawn(ex, eGroundY, -1)) {
                e->startHp = 3;

                // Patrol +/- 3 tiles from spawn X. Use whatever range fits
                // your test platform -- the enemy will auto-turn on walls
                // and ledges too, so a wide range is safe.
                e->setPatrolPath(ex - 3.0f * kTilePx, ex + 3.0f * kTilePx, 1.0f);

                // Re-snapshot HP with the new startHp.
                e->spawn(ex, eGroundY, -1);
            }
        }
```

Adjust `m_world.map()` / `m_ground` to whatever accessor names
GameLayer uses today; the point is to hand the TileMap* and
TileMapLayer* to the manager.

### 3b. `onUpdate` — gunshot ping when the player fires

Find the block where GameLayer consumes a player shot (Item 06)
— typically looks like:

```cpp
        float sx, sy; int sdir;
        if (m_player.consumeShot(sx, sy, sdir)) {
            m_bullets.spawn(sx, sy, sdir);
            m_effects.spawnMuzzleFlash(m_player.x() + sdir * 5.0f, sy, sdir);
            m_effects.spawnCasing(m_player.x() + sdir * 5.0f, sy, sdir);
        }
```

(Your local variables may be named `bx`/`by`/`bdir` — that's
fine; use whatever names your `consumeShot` call uses.)

Add one line at the bottom of that block, so gunshots ping every
in-range enemy the same frame they leave the barrel. Match the
variable names to whatever your `consumeShot` uses (typically
`bx, by`):

```cpp
            m_enemies.notifyGunshot(bx, by);
```

Order relative to `m_enemies.checkBulletHits(...)` and
`m_enemies.update(dt)` does not matter for `notifyGunshot`
(it only mutates enemy state; sensing reads run inside
`update`).

### 3c. `onRender` — bubbles always, cones only with B

Find the World-scene render block (Item 08 already added
`m_enemies.render(r2d)` and the B-gated `debugDrawBoxes`). Just
after the existing box overlay call, add the bubble render (no
gate) and the sense overlay (gated by B):

```cpp
            m_enemies.render(r2d);
            // ...player, bullets, etc render as before...

            // Item 09 always-on bubbles.
            m_enemies.renderBubbles(m_debug, r2d);

            if (m_showHitboxes) {
                m_enemies.debugDrawBoxes(m_debug, r2d);
                m_enemies.debugDrawSenses(m_debug, r2d);
                // player hurtbox already drawn here from Item 08
            }
```

Whether you call `renderBubbles` before or after
`debugDrawBoxes` doesn't matter — `DebugDraw2D` uses layer 9000
and paints on top of everything either way.

---

## 4. Why this stays in a game-side `DebugDraw2D` bubble

Two reasons not to make bubble sprites yet:

1. **Zero new assets in Item 09.** Every asset already ships
   with the repo; the icon is composed from 2 axis-aligned rects
   which look reasonable, animate cheaply (you could pulse alpha
   over time — see the tuning table in doc 04), and are already
   layer-9000-guaranteed to render above everything.
2. **Item 10 gets to redesign it anyway.** Once you start
   distinguishing Casual/Difficult/Challenging enemies, colored
   bubbles with per-difficulty tint (or a difficulty badge)
   naturally slots into the same helper.

If you do want prettier bubbles now, the swap is small:
`renderBubbles` becomes a `Renderer2D::draw(RenderItem)` call
with a small "?" or "!" sprite; the composition code goes away.

---

Next: `04_build_run_and_verify.md` — `.vcxproj` additions for
the Fordward walk sheet, MSBuild command, the full verify
checklist (patrol + hearing + sight + LOS + ghost bypass +
lose-aggro + return + gunshot ping), tuning table and a
troubleshooting matrix.
