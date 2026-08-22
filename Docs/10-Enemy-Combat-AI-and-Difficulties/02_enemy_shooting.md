# 02 — Enemy shooting: `EnemyBullet`, `tickShooting`, and the fire callback

This doc adds the actual weapon. When we're done here:

* A new `EnemyBulletManager` owns enemy bullets and looks
  structurally identical to `BulletManager` (Item 06) —
  simulates positions, culls off-screen bullets, kills on tile
  hits, exposes a `bullets()` view for hit-testing the player.
* `Enemy::tickChase` calls a new `tickShooting(...)` helper
  each frame. When the shot timer hits zero and the enemy has
  LOS + range on the player, it invokes the fire callback wired
  in by `EnemyManager`. The enemy never touches a vector.
* Challenging enemies use `leadFactor` to compute a predicted
  aim point using `player.velX()` and the flight time
  approximation `distance / bulletSpeed`.

Everything in this doc is game-side; no engine edits.

---

## 1. `include/Game/EnemyBullet.h` — new file

Create this file at `G:\Dev\HBE\MegaX\include\Game\EnemyBullet.h`:

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

    // Enemy-fired projectiles. Structurally close to BulletManager (Item
    // 06) but kept separate so hit tests, tints and future visuals don't
    // require branching on "who fired this".
    class EnemyBulletManager {
    public:
        struct Bullet {
            float x = 0.0f, y = 0.0f;
            float vx = 0.0f, vy = 0.0f;
            int   damage = 1;
            bool  alive = true;
        };

        bool init(HBE::Renderer::ResourceCache& resources,
                  HBE::Renderer::Mesh* quadMesh,
                  HBE::Renderer::GLShader* spriteShader);

        // Spawn a bullet at (sx, sy) aimed at (aimX, aimY) at `speed`.
        // Damage travels with the bullet so per-shot difficulty scaling
        // works even if difficulty changes mid-flight.
        void spawn(float sx, float sy,
                   float aimX, float aimY,
                   float speed, int damage);

        void update(float dt,
                    const HBE::Renderer::TileMap* map,
                    const HBE::Renderer::TileMapLayer* solidLayer,
                    const HBE::Renderer::Camera2D& cam);

        void render(HBE::Renderer::Renderer2D& r2d);

        void clear() { m_bullets.clear(); }
        int  count() const { return static_cast<int>(m_bullets.size()); }

        std::vector<Bullet>&       bullets()       { return m_bullets; }
        const std::vector<Bullet>& bullets() const { return m_bullets; }

        // Visual tuning
        float length          = 14.0f;   // world px
        float height          = 4.0f;
        float offscreenTiles  = 4.0f;    // despawn margin past view

    private:
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

**Design notes:**

* Both `vx` and `vy` are stored so we can support angled shots
  (Challenging leads horizontally today, but a later item might
  aim vertically at a jumping player without changing the
  struct).
* `damage` is carried by each bullet, not read off the
  originating enemy, so a bullet fired at `damage=2` still
  hurts for 2 even if you flick to Casual mid-flight.
* `length`/`height` mirror `BulletManager`'s visual knobs.
  Tint is baked to a bright red inside `init(...)` — this is
  the primary visual cue that this bullet is *incoming*.

---

## 2. `src/Game/EnemyBullet.cpp` — new file

Create at `G:\Dev\HBE\MegaX\src\Game\EnemyBullet.cpp`:

```cpp
#include "Game/EnemyBullet.h"

#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/Renderer2D.h"

#include <algorithm>
#include <cmath>

using namespace HBE::Renderer;

namespace MegaX {

    bool EnemyBulletManager::init(ResourceCache& resources, Mesh* quadMesh, GLShader* spriteShader) {
        if (!quadMesh || !spriteShader) return false;

        // 1x1 white texture, re-usable across visual variants.
        const unsigned char white[4] = { 255, 255, 255, 255 };
        Texture2D* tex = resources.getOrCreateTextureFromRGBA("megax_white1x1", 1, 1, white);
        if (!tex) return false;

        m_quad = quadMesh;
        m_material.shader  = spriteShader;
        m_material.texture = tex;

        m_item.mesh     = m_quad;
        m_item.material = &m_material;
        m_item.layer    = 101;                                    // above player
        m_item.pass     = RenderPass::World;
        m_item.tint     = Color4{ 1.0f, 0.35f, 0.35f, 1.0f };     // hot red
        return true;
    }

    void EnemyBulletManager::spawn(float sx, float sy, float aimX, float aimY, float speed, int damage) {
        float dx = aimX - sx;
        float dy = aimY - sy;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.0001f) {
            // Degenerate aim -- default to positive-X so we still see the shot.
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
        const float halfW = cam.viewportWidth  / (2.0f * zoom);
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
                b.alive = false;
                continue;
            }
            if (b.x < minX || b.x > maxX || b.y < minY || b.y > maxY) {
                b.alive = false;
            }
        }

        m_bullets.erase(
            std::remove_if(m_bullets.begin(), m_bullets.end(),
                [](const Bullet& b) { return !b.alive; }),
            m_bullets.end());
    }

    void EnemyBulletManager::render(Renderer2D& r2d) {
        for (auto& b : m_bullets) {
            if (!b.alive) continue;
            m_item.transform.posX = b.x;
            m_item.transform.posY = b.y;
            // Rotate the quad so its long side follows the bullet velocity.
            const float ang = std::atan2(b.vy, b.vx);
            m_item.transform.rotation = ang;
            m_item.transform.scaleX = length;
            m_item.transform.scaleY = height;
            r2d.draw(m_item);
        }
    }
}
```

**Design notes:**

* Rotation isn't strictly necessary for a horizontal shooter,
  but it costs one atan2 per bullet and lets Challenging's
  angled leads read clearly (the streak points at the aim
  point).
* We reuse `"megax_white1x1"` — the exact key `BulletManager`
  uses. The `ResourceCache` is name-keyed, so both managers
  share the texture (see the `resource cache` memory).
* No collision with enemies (friendly fire is off) — that check
  is deliberately absent.
* No collision with the player is done in this file. Doc 03
  wires that check in `GameLayer::onUpdate` where the player
  hurtbox already lives.

---

## 3. `Enemy.cpp` — plug shooting into `tickChase`

Item 09 gave `tickChase` a standoff-hold pattern. Item 10 keeps
that behavior and adds a shooting subroutine. Here's the full
replacement of `tickChase(...)`. Diff-wise, we're adding the
`m_fireCooldown` decrement at the top and the fire-check block
just before the crouch/hidden bookkeeping at the bottom.

```cpp
    void Enemy::tickChase(float dt, const Player& player) {
        if (player.mode() == Player::Mode::Ghost) {
            enter(AIState::Search);
            return;
        }

        faceX(player.x());

        // --- Item 09 standoff-hold: unchanged ---
        const float dx  = player.x() - m_x;
        const float adx = std::fabs(dx);
        const float standoff = shootingRange * standoffFrac;
        int moveDir = 0;
        if (adx > standoff + standoffDeadzone)      moveDir = m_facing;
        else if (adx < standoff - standoffDeadzone) moveDir = -m_facing;
        m_vx = static_cast<float>(moveDir) * chaseSpeed;

        if (m_grounded && m_jumpCooldown <= 0.0f) {
            const float dyToPlayer = player.y() - m_y;
            const bool advancing = (moveDir == m_facing) && moveDir != 0;
            const bool wantJump = (dyToPlayer > 24.0f) ||
                (advancing && (wallInFront() || ledgeInFront()));
            if (wantJump) {
                m_vy = jumpSpeed;
                m_grounded = false;
                m_jumpCooldown = chaseJumpCooldown;
            }
        }

        // --- Item 10 shooting ---
        tickShooting(dt, player);

        // --- Item 09 aggro/hidden bookkeeping (unchanged) ---
        const AABB pb = player.hurtbox();
        const bool crouching = pb.h < 34.0f;
        if (!m_lastSeen && crouching) {
            m_hiddenTimer += dt;
            if (m_hiddenTimer >= loseAggroDelay) { enter(AIState::Search); return; }
        }
        else {
            m_hiddenTimer = 0.0f;
        }
    }
```

Add the new `tickShooting(...)` helper. Placement: right below
the definition of `tickChase(...)` for readability.

```cpp
    void Enemy::tickShooting(float dt, const Player& player) {
        // Countdown always -- even if we can't shoot this frame.
        if (m_fireCooldown > 0.0f) m_fireCooldown -= dt;

        // Gate: manager must have wired a callback, LOS must be clear,
        // player must be within our shooting range.
        if (!m_fireFn) return;
        if (!m_lastSeen) return;

        const float dx = player.x() - m_x;
        const float dy = player.y() - m_y;
        if (dx * dx + dy * dy > shootingRange * shootingRange) return;

        if (m_fireCooldown > 0.0f) return;

        // -------- compute muzzle world position --------
        float mx, my;
        muzzleWorldPos(mx, my);

        // -------- compute aim (with optional lead) --------
        // Time-of-flight approximation: distance / bulletSpeed.
        // Multiplied by leadFactor (0 for Casual/Difficult, 1 for Challenging).
        float aimX = player.x();
        float aimY = player.y();
        if (leadFactor > 0.0001f && bulletSpeed > 0.0f) {
            const float toPlayerX = aimX - mx;
            const float toPlayerY = aimY - my;
            const float dist = std::sqrt(toPlayerX * toPlayerX + toPlayerY * toPlayerY);
            const float flight = dist / bulletSpeed;
            aimX = player.x() + player.velX() * flight * leadFactor;
            // Vertical lead is left off intentionally -- see doc 00 goals.
        }

        // -------- fire --------
        m_fireFn(m_fireCtx, mx, my, aimX, aimY, bulletSpeed, bulletDamage);
        m_fireCooldown = fireCooldownSec;
    }
```

Declare `tickShooting` in `Enemy.h` in the `private:` block near
the other tick helpers:

```cpp
        void tickShooting(float dt, const Player& player);
```

**Reminder:** `tickShooting` sits inside `tickChase`, meaning we
only fire while chasing. If you later want a "hold-and-shoot"
guard enemy that doesn't move, add a new `AIState::Hold` and
call `tickShooting(...)` from there too — the helper is
self-contained.

---

## 4. `EnemyManager` — wire the fire callback

The manager passes each enemy a raw C-style callback + `this`
as context. This keeps `Enemy` template-free and vector-free
(same reason `Effects.h` uses a pimpl-shaped `unique_ptr`).

### 4a. Update `EnemyManager::spawn` to wire the callback

Add three lines right after `e.applyDifficulty(m_profile);`:

```cpp
        e.setFireCallback(
            [](void* ctx, float sx, float sy, float aimX, float aimY,
               float speed, int damage) {
                static_cast<EnemyManager*>(ctx)->m_enemyBullets->spawn(
                    sx, sy, aimX, aimY, speed, damage);
            },
            this);
```

Because we're using a *capturing-free* lambda cast to
`FireFn`, the compiler will accept the conversion. If your
compiler flags the conversion, add `+` in front:

```cpp
        e.setFireCallback(
            +[](void* ctx, float sx, float sy, float aimX, float aimY,
                float speed, int damage) { ... },
            this);
```

### 4b. Have `EnemyManager::update(...)` advance enemy bullets and check the player hit

Replace the current `update(...)` with:

```cpp
    void EnemyManager::update(float dt) {
        if (!m_player) return;   // GameLayer must call setPlayerRef first

        // 1. Tick every enemy (drives sensing, chase, shooting).
        for (auto& e : m_enemies) e.tick(dt, *m_player);

        // 2. Advance enemy bullets. GameLayer forwarded the camera to us
        //    via the same accessor Player-side bullets use -- but since
        //    EnemyManager doesn't currently hold a camera ref, we route
        //    the tick + hit test through GameLayer instead. See doc 03.
        //
        //    (Nothing to do in this manager yet -- GameLayer owns the
        //    per-frame ordering for enemy bullets + player hit.)

        // 3. Reap finished enemies.
        m_enemies.erase(
            std::remove_if(m_enemies.begin(), m_enemies.end(),
                [](const Enemy& e) { return e.isFinished(); }),
            m_enemies.end());
    }
```

`EnemyManager` intentionally does NOT advance the bullets or
check the player hit — those need a `Camera2D&` (for cull
margins) and a `Player&` (mutable, for `takeDamage`). Both are
sitting in `GameLayer`, so GameLayer drives the enemy-bullet
update/hit-test in three lines (doc 03).

Alternatively, if you'd rather have the manager own the whole
loop, extend `setPlayerRef` to take a mutable `Player*` and
`setCollision` to also cache the camera. Both are fine; doc 03
uses the game-layer-drives-it flavor for parity with how player
bullets already work.

### 4c. Have `EnemyManager::render(...)` NOT render enemy bullets

Enemy bullets are drawn by GameLayer alongside player bullets so
render ordering stays obvious. `EnemyManager::render(...)` stays
unchanged — it still just draws the enemies themselves.

---

## 5. `Enemy::spawn` — reset the fire cooldown

At the bottom of `Enemy::spawn(...)`, add:

```cpp
        m_fireCooldown = 0.0f;
```

so a re-spawned enemy is *ready to shoot* the moment it acquires
line of sight. If you want them to hold fire for a beat after
spawn (feels less punishing on scene reload), set to
`fireCooldownSec * 0.5f` instead.

---

## 6. Sanity check

At the end of doc 02:

* `EnemyBullet.h` and `.cpp` exist and compile in isolation.
* `EnemyManager::spawn` wires the fire callback + applies the
  profile.
* `Enemy::tickChase` calls `tickShooting`; `tickShooting`
  invokes the callback when eligible.
* The project **still doesn't fully compile** because
  `GameLayer` doesn't yet know about the enemy bullets and the
  player HP doesn't exist. Doc 03 finishes the wiring and gets
  us back to green.

---

## 7. Common mistakes

| Symptom | Likely cause | Fix |
|---|---|---|
| `error C2664: cannot convert lambda to FireFn` | Non-capturing lambda still fails on some MSVC releases. | Prefix with `+`: `+[](...){...}`. |
| Enemy shoots immediately every frame | `m_fireCooldown` was reset to `0` inside `tickShooting` instead of after firing. | The line `m_fireCooldown = fireCooldownSec;` must be the last thing in the branch after `m_fireFn(...)`. |
| Bullets spawn from the wrong side of the enemy | `m_facing` is `-1`/`+1` but you scaled the muzzle by facing twice. | Only `muzzleWorldPos` multiplies by `m_facing`. |
| Bullets tunnel through thin walls at high speed | Per-frame step > tile width; `pointInSolid` only samples once per frame. | Either lower `bulletSpeed` or add a mid-step sample in `EnemyBulletManager::update` (kept out of Item 10 by design). |
| Bullet spawn callback dereferences a dangling manager | `EnemyManager` was moved or reallocated after wiring the callback. | Never move the manager after `init(...)`. It's owned by GameLayer for the entire scene lifetime. |
| Lead prediction sends bullets miles past the player | You accidentally used raw `player.velX()` without multiplying by flight time. | Formula is `player.velX() * (dist / bulletSpeed) * leadFactor`. |
| Enemy fires while the player is out of sight | Forgot the `m_lastSeen` guard in `tickShooting`. | It's the second line after the cooldown decrement. |
| Enemy still shoots after death | Sensing sets `m_lastSeen`/`m_lastHeard` regardless of `m_dead`. | `Enemy::tick` already early-outs when `m_dead` — verify you didn't remove that block from Item 09. |

---

Next: `03_player_damage_and_wiring.md` — Player HP + i-frames +
knockback + hurt flash, GameLayer wiring for hotkeys / HUD /
enemy-bullet update-and-hit-test.
