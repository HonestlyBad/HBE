# 02 — EnemyManager & bullet integration

`EnemyManager` owns every live enemy, drives their update / render,
and runs the per-frame bullet-vs-hurtbox test that turns a live
bullet into HP loss on an enemy. This mirrors how `BulletManager`
owns bullets and how `Effects` owns particles.

To make the hit test possible without any engine changes we make a
**tiny** additive tweak to `BulletManager`:

* Move the `Bullet` struct to the public API.
* Add a `bullets()` getter that returns a mutable reference to the
  underlying vector.
* Nothing else changes: `Bullet.cpp` is untouched, no existing
  callers need updates.

---

## 1. `BulletManager` patch — expose the bullet list

Open `include/Game/Bullet.h` and make three small edits.

### 1a. Move `struct Bullet` out of `private:` into `public:`

Cut the existing definition:

```cpp
    private:
        struct Bullet {
            float x = 0.0f, y = 0.0f;
            float vx = 0.0f;
            bool  alive = true;
        };
```

...and paste it in the `public:` section, right after the `Impact`
struct that is already public there. Also add a doc comment so
it's obvious the field can be mutated by outsiders on purpose:

```cpp
    public:
        // ...existing public API up through Impact + consumeImpacts...

        // Exposed so game code (e.g. EnemyManager) can iterate live bullets
        // and mark them as `alive = false` on hit. Do not resize this vector
        // from outside; only clear the `alive` flag.
        struct Bullet {
            float x = 0.0f, y = 0.0f;
            float vx = 0.0f;
            bool  alive = true;
        };

        std::vector<Bullet>&       bullets()       { return m_bullets; }
        const std::vector<Bullet>& bullets() const { return m_bullets; }
```

### 1b. Remove the now-duplicate declaration from `private:`

The `private:` section previously read:

```cpp
    private:
        struct Bullet { /* ... */ };

        bool pointInSolid(...) const;

        std::vector<Bullet> m_bullets;
        std::vector<Impact> m_impacts;
        // ...
    };
```

After the move it should read:

```cpp
    private:
        bool pointInSolid(...) const;

        std::vector<Bullet> m_bullets;
        std::vector<Impact> m_impacts;
        // ...
    };
```

That is the **only** file change to Bullet. `Bullet.cpp` compiles
as-is because `Bullet` was always used via the nested name
`BulletManager::Bullet` inside the class body.

### Why not a callback-style API?

A `BulletManager::forEachAliveBullet(std::function<...>)` would
avoid the direct exposure but would also drag `<functional>` in and
add an extra copy-per-frame per bullet. The vector getter is dead
simple, and every current caller (`GameLayer`, `EnemyManager`) is
inside the same project so encapsulation isn't providing much
insulation. Keep it flat.

---

## 2. Header — `include/Game/EnemyManager.h`

Create the file with exactly this content:

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
}

namespace MegaX {

    class BulletManager;
    class Effects;

    // Owns every live enemy. Manages spawn / update / render and runs the
    // per-frame bullet-vs-hurtbox test that converts alive bullets into HP
    // loss on the appropriate enemy.
    //
    // Item 08 only needs a single enemy on the map, but keeping the vector
    // means Item 09 (patrol) and Item 10 (multi-enemy combat) don't need
    // to rewrite this file.
    class EnemyManager {
    public:
        bool init(HBE::Renderer::ResourceCache& resources,
            HBE::Renderer::Mesh* quadMesh,
            HBE::Renderer::GLShader* spriteShader);

        // Spawn a new enemy. `facing`: -1 (left) or +1 (right). Returns a
        // pointer to the created enemy so the caller can override its
        // tunables (HP, hurtbox size, etc.) before the first update.
        Enemy* spawn(float x, float groundY, int facing);

        // Test every alive bullet in `bullets` against every alive enemy's
        // hurtbox. On hit:
        //   * calls `enemy.takeDamage(damagePerBullet)`,
        //   * marks the bullet as not alive (BulletManager compacts it on
        //     the next frame),
        //   * if `effects` != nullptr, spawns a bullet-impact burst at the
        //     hit point (tileId = 0 -> tan fallback tint).
        // Returns the total number of hits registered this frame.
        int checkBulletHits(BulletManager& bullets, Effects* effects, int damagePerBullet = 1);

        void update(float dt);
        void render(HBE::Renderer::Renderer2D& r2d);

        // Draw hurtbox/hitbox outlines for every enemy via the supplied
        // DebugDraw2D. Green = alive hurtbox, gray = fading, red = active
        // hitbox. Called by GameLayer only when the B toggle is on.
        void debugDrawBoxes(HBE::Renderer::DebugDraw2D& dbg,
                            HBE::Renderer::Renderer2D& r2d) const;

        // Read-only accessors (mostly for debug HUDs and future tests).
        int aliveCount() const;
        const std::vector<Enemy>& enemies() const { return m_enemies; }

        void clear() { m_enemies.clear(); }

    private:
        HBE::Renderer::ResourceCache* m_resources = nullptr;
        HBE::Renderer::Mesh*          m_quadMesh  = nullptr;
        HBE::Renderer::GLShader*      m_spriteShader = nullptr;

        std::vector<Enemy> m_enemies;
    };
}
```

Notes:

* `spawn()` returns a pointer so the caller can override tunables
  (HP, hurtbox dimensions) right at the spawn site. This keeps
  Item 08's setup readable without needing a builder pattern.
* `checkBulletHits` reports the number of hits so a future HUD /
  test can assert on it, but nothing in Item 08 uses the return
  value.
* The `Effects*` in `checkBulletHits` is a pointer (not a
  reference) so a headless unit test can pass `nullptr` without
  having to stand up a particle system.

---

## 3. Implementation — `src/Game/EnemyManager.cpp`

Create the file with exactly this content:

```cpp
#include "Game/EnemyManager.h"

#include "Game/Bullet.h"
#include "Game/Effects.h"

#include "HBE/Renderer/DebugDraw2D.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Core/Log.h"

#include <algorithm>
#include <cmath>

using namespace HBE::Renderer;

namespace MegaX {

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
        if (!m_resources || !m_quadMesh || !m_spriteShader) {
            HBE::Core::LogError("EnemyManager::spawn called before init.");
            return nullptr;
        }
        m_enemies.emplace_back();
        Enemy& e = m_enemies.back();
        if (!e.init(*m_resources, m_quadMesh, m_spriteShader)) {
            m_enemies.pop_back();
            return nullptr;
        }
        e.spawn(x, groundY, facing);
        return &e;
    }

    // --------------------------------------------------- bullet hit test loop
    int EnemyManager::checkBulletHits(BulletManager& bullets, Effects* effects, int damagePerBullet) {
        if (m_enemies.empty()) return 0;

        int hits = 0;
        auto& list = bullets.bullets();

        for (auto& b : list) {
            if (!b.alive) continue;

            // Bullet is a point in world space. Test it against every alive
            // hurtbox; on the first hit, kill the bullet and stop looking so
            // one bullet can't damage two enemies stacked on the same tile.
            for (auto& e : m_enemies) {
                if (!e.hurtboxActive()) continue;

                const AABB h = e.hurtbox();
                const float dx = std::fabs(b.x - h.cx);
                const float dy = std::fabs(b.y - h.cy);
                if (dx > h.w * 0.5f || dy > h.h * 0.5f) continue;

                if (e.takeDamage(damagePerBullet)) {
                    b.alive = false;
                    ++hits;
                    if (effects) {
                        // tileId = 0 -> Effects uses the fallback tan tint
                        // today. Item 12 will swap this for a spark burst
                        // more appropriate for a robot enemy.
                        effects->spawnBulletImpact(b.x, b.y, 0);
                    }
                    break;   // bullet is dead; next bullet
                }
            }
        }
        return hits;
    }

    // ------------------------------------------------------------------ update
    void EnemyManager::update(float dt) {
        for (auto& e : m_enemies) e.update(dt);

        // Reclaim slots for enemies that finished their fade-out.
        m_enemies.erase(
            std::remove_if(m_enemies.begin(), m_enemies.end(),
                [](const Enemy& e) { return e.isFinished(); }),
            m_enemies.end());
    }

    // ------------------------------------------------------------------ render
    void EnemyManager::render(Renderer2D& r2d) {
        for (auto& e : m_enemies) e.render(r2d);
    }

    // ---------------------------------------------------------- debug overlay
    void EnemyManager::debugDrawBoxes(DebugDraw2D& dbg, Renderer2D& r2d) const {
        for (const auto& e : m_enemies) {
            // Hurtbox: green when alive, gray while fading, skip if finished.
            if (e.isFinished()) continue;
            const AABB h = e.hurtbox();
            if (e.isDying()) {
                dbg.rect(r2d, h.cx, h.cy, h.w, h.h, 0.7f, 0.7f, 0.7f, 1.0f, false);
            } else {
                dbg.rect(r2d, h.cx, h.cy, h.w, h.h, 0.25f, 1.0f, 0.35f, 1.0f, false);
            }

            // Hitbox: only when active (i.e., attacking). In Item 08 this is
            // always off. Draw in red so future items make it obvious.
            if (e.hitboxActive()) {
                const AABB k = e.hitbox();
                dbg.rect(r2d, k.cx, k.cy, k.w, k.h, 1.0f, 0.35f, 0.35f, 1.0f, false);
            }
        }
    }

    int EnemyManager::aliveCount() const {
        int n = 0;
        for (const auto& e : m_enemies) if (e.isAlive()) ++n;
        return n;
    }
}
```

Notes:

* **AABB overlap test** — straightforward centered rectangle:
  `|bx - hcx| < hw/2 && |by - hcy| < hh/2`. Because a bullet is
  modelled as a point today (Item 06), no bullet extents enter
  the math. If Item 09 or later gives bullets a thicker collision
  shape, widen the check by adding the bullet's half-extent to
  `h.w/2` etc.
* **`break` after a hit** — a bullet can only kill itself once, so
  stopping at the first hit prevents a single bullet from damaging
  two enemies whose hurtboxes happen to overlap.
* **Compaction happens in `update()`** — not right after the hit
  loop. That way rendering and debug drawing on the frame of the
  killing hit still show the fading-out enemy correctly.
* **`Effects::spawnBulletImpact` with tileId = 0** — this reuses
  the fallback tan-tint path in
  `Effects::spawnBulletImpact` (see
  `07-Particle-Effects/01_effects_configs_and_class.md`). It's
  intentionally the same visual as a bullet hitting an unknown
  tile; Item 12 will branch on the target type and switch to a
  spark burst for robots.

---

Next: `03_gamelayer_and_debug.md` — wiring the manager into
`GameLayer`, the spawn point, the update / render order and the
`B` key that toggles the hit / hurt box overlay.
