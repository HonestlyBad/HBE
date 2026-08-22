# 01 — Enemy class

The `Enemy` is a single robot soldier: standalone class (no ECS),
mirrors how `Player` is structured, includes an idle-only
`SpriteAnimation`, a health counter, a hurtbox for taking damage,
a placeholder hitbox for later, a small red-flash on hit, and a
fade-out timer for death.

Everything below is game-side code that lives in
`G:\Dev\HBE\MegaX\`. No engine files are touched.

---

## 1. Header — `include/Game/Enemy.h`

Create the file with exactly this content:

```cpp
#pragma once

#include "HBE/Renderer/Sprite2D.h"          // SpriteSheet, SpriteAnimation, SpriteRenderer2D
#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/TileCollision.h"     // AABB

// Do NOT include ParticleSystem.h, Scene2D.h or CombatSystem.h here --
// each transitively pulls in a second definition of SpriteRenderer2D
// (see G:\Dev\Temp\MegaX\07-Particle-Effects\01_effects_configs_and_class.md
// header note).

namespace HBE::Renderer {
    class ResourceCache;
    class Mesh;
    class GLShader;
    class Renderer2D;
}

namespace MegaX {

    // A single robot-soldier enemy. In Item 08 the enemy stands idle,
    // takes damage from player bullets, and fades out on death. Walking,
    // shooting and hitting the player are added in later items.
    class Enemy {
    public:
        // Load animations / materials. Safe to re-call for pooling later.
        bool init(HBE::Renderer::ResourceCache& resources,
            HBE::Renderer::Mesh* quadMesh,
            HBE::Renderer::GLShader* spriteShader);

        // Place the enemy so that the sprite's feet sit at world y == groundY,
        // at world x == x. facing = -1 (left, default) or +1 (right).
        void spawn(float x, float groundY, int facing);

        void update(float dt);
        void render(HBE::Renderer::Renderer2D& r2d);

        // Damage the enemy by `amount`. Returns true if the hit actually
        // landed (respects the post-hit invulnerability window and the
        // enemy being alive). On death, sets `deathTimer = deathFadeTime`
        // so the fade-out plays; the enemy stays around until the fade
        // completes and then reports `isFinished() == true`.
        bool takeDamage(int amount);

        // World-space AABB of the hurtbox (where bullets can hit us).
        // Center is the enemy anchor + (offsetX, offsetY), possibly
        // mirrored on X by the facing. Inactive while dead.
        HBE::Renderer::AABB hurtbox() const;

        // World-space AABB of the hitbox (where we deal damage). In Item
        // 08 hitboxActive stays false, so this exists for the debug
        // overlay + future items.
        HBE::Renderer::AABB hitbox() const;

        bool hurtboxActive() const { return !m_dead; }
        bool hitboxActive()  const { return m_hitboxActive && !m_dead; }

        bool isAlive()    const { return !m_dead; }
        bool isDying()    const { return m_dead && m_deathTimer > 0.0f; }
        bool isFinished() const { return m_dead && m_deathTimer <= 0.0f; }

        int  hp()    const { return m_hp; }
        int  maxHp() const { return m_maxHp; }
        int  facing() const { return m_facing; }
        float x() const { return m_x; }
        float y() const { return m_y; }
        float feetY() const { return m_feetY; }

        // ---- tunables (safe to edit at runtime) ----------------------------
        int   startHp        = 3;      // HP each spawn begins with; also sets maxHp
        int   damagePerHit   = 1;      // reference value; the caller passes damage
        float invulnAfterHit = 0.08f;  // seconds between accepted hits (i-frames)
        float hitFlashTime   = 0.10f;  // seconds the sprite tints red on hit
        float deathFadeTime  = 0.60f;  // seconds the fade-out takes

        // Hurtbox (world size, in pixels) -- centered on the enemy anchor,
        // with an optional offset. Offset X is mirrored by facing so the
        // hurtbox stays on the visible sprite side when the enemy faces
        // right.
        float hurtHalfW      = 11.0f;
        float hurtHalfH      = 18.0f;
        float hurtOffsetX    =  0.0f;
        float hurtOffsetY    =  0.0f;

        // Hitbox (where the enemy will hurt the player, once Item 10 turns
        // it on). Same conventions as hurtbox.
        float hitHalfW       = 20.0f;
        float hitHalfH       = 14.0f;
        float hitOffsetX     = 20.0f;   // in front of the enemy
        float hitOffsetY     =  0.0f;
        int   hitDamage      = 1;

    private:
        void applyAnimFrameToRenderItem();

        // Position: `m_x` / `m_y` are the sprite CENTER in world space; the
        // sprite is drawn as a 64x64 quad centered on this point. `m_feetY`
        // is captured at spawn (equals groundY) and is used for the hurtbox
        // math so the box hugs the ground regardless of the sprite offset.
        float m_x = 0.0f;
        float m_y = 0.0f;
        float m_feetY = 0.0f;

        int   m_facing = -1;   // -1 left (default), +1 right

        int   m_hp    = 3;
        int   m_maxHp = 3;

        float m_invulnTimer = 0.0f;
        float m_flashTimer  = 0.0f;
        float m_deathTimer  = 0.0f;
        bool  m_dead        = false;

        bool  m_hitboxActive = false;   // Item 10 sets this to true

        // Animation
        HBE::Renderer::SpriteSheet    m_idleSheet{};
        HBE::Renderer::SpriteAnimation m_idleAnim;

        HBE::Renderer::Material   m_material{};
        HBE::Renderer::RenderItem m_item{};
    };
}
```

Why these tunables live in `public` fields: MegaX has been treating
Player / Bullet tuning the same way (`speed`, `moveSpeed`, `jumpSpeed`)
so callers can override defaults per-spawn or from a future
JSON-driven spawner. Keep the pattern for consistency.

---

## 2. Implementation — `src/Game/Enemy.cpp`

Create the file with exactly this content:

```cpp
#include "Game/Enemy.h"

#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Core/AssetPaths.h"
#include "HBE/Core/Log.h"

#include <algorithm>

using namespace HBE::Renderer;

namespace MegaX {

    // ---- sprite grid (Robot Soldier Idle) ---------------------------------
    // 4 cols x 2 rows, 64 x 64 px cells. SpriteAnimation only supports a
    // single row, so Item 08 plays row 0 cols 0..3 (4 frames) at 6 fps.
    static constexpr int kFrameW = 64;
    static constexpr int kFrameH = 64;
    static constexpr float kPixelScale = 1.0f;

    static constexpr int   kIdleRow  = 0;
    static constexpr int   kIdleCol0 = 0;
    static constexpr int   kIdleCol1 = 3;
    static constexpr float kIdleFps  = 6.0f;

    // Sprite pixels occupy the LOWER ~35 rows of each 64-tall cell; the
    // upper half is transparent headroom. Player's convention says draw the
    // 64x64 quad centered at (posX, posY) where posY = feetY + kFrameH/2.
    // That puts the frame-bottom row (i.e. the visible feet) exactly at
    // feetY, matching how Player anchors its 75x48 sprite to its collision
    // box bottom.
    static float posYForFeet(float feetY) {
        return feetY + static_cast<float>(kFrameH) * kPixelScale * 0.5f;
    }

    // ------------------------------------------------------------------ init
    bool Enemy::init(ResourceCache& resources, Mesh* quadMesh, GLShader* spriteShader) {
        if (!quadMesh || !spriteShader) {
            HBE::Core::LogError("Enemy::init: quadMesh or spriteShader is null.");
            return false;
        }

        m_idleSheet = SpriteRenderer2D::DeclareSpriteSheet(
            resources, "robot_soldier_idle",
            HBE::Core::AssetPaths::Resolve("sprites/Enemies/RobotSoldier/SoldierIdle_Spritesheet.png"),
            kFrameW, kFrameH);

        if (!m_idleSheet.isValid()) {
            HBE::Core::LogError("Enemy::init: failed to load Robot Soldier Idle sheet at "
                + HBE::Core::AssetPaths::AssetRootString());
            return false;
        }

        m_idleAnim = SpriteAnimation(&m_idleSheet, kIdleCol0, kIdleCol1, kIdleRow, kIdleFps, true);
        m_idleAnim.play(true);

        m_material.shader  = spriteShader;
        m_material.texture = m_idleSheet.texture;

        m_item.mesh = quadMesh;
        m_item.material = &m_material;
        m_item.layer = 99;                        // just behind the player (100)
        m_item.pass  = RenderPass::World;
        m_item.transform.scaleX = kFrameW * kPixelScale;
        m_item.transform.scaleY = kFrameH * kPixelScale;
        m_item.tint = Color4{ 1.0f, 1.0f, 1.0f, 1.0f };

        m_maxHp = startHp;
        m_hp    = m_maxHp;

        SpriteRenderer2D::SetStaticSpriteFrame(m_item, m_idleSheet, kIdleCol0, kIdleRow);
        return true;
    }

    // ----------------------------------------------------------------- spawn
    void Enemy::spawn(float x, float groundY, int facing) {
        m_x     = x;
        m_feetY = groundY;
        m_y     = posYForFeet(groundY);
        m_facing = (facing >= 0) ? +1 : -1;

        // Re-read startHp every spawn so callers can bump it between spawns
        // (e.g. tougher enemies later in the level) without touching Enemy.cpp.
        m_maxHp = startHp;
        m_hp    = m_maxHp;
        m_invulnTimer = 0.0f;
        m_flashTimer  = 0.0f;
        m_deathTimer  = 0.0f;
        m_dead        = false;
    }

    // ---------------------------------------------------------------- update
    void Enemy::update(float dt) {
        // Timers first so a fresh hit is respected in the same frame.
        if (m_invulnTimer > 0.0f) m_invulnTimer = std::max(0.0f, m_invulnTimer - dt);
        if (m_flashTimer  > 0.0f) m_flashTimer  = std::max(0.0f, m_flashTimer  - dt);

        if (m_dead) {
            if (m_deathTimer > 0.0f) m_deathTimer = std::max(0.0f, m_deathTimer - dt);
        } else {
            m_idleAnim.update(dt);
        }
        applyAnimFrameToRenderItem();
    }

    // ---------------------------------------------------------------- render
    void Enemy::render(Renderer2D& r2d) {
        if (isFinished()) return;                 // fade complete: fully invisible

        m_item.transform.posX = m_x;
        m_item.transform.posY = m_y;
        // scaleX sign flip = horizontal mirror (matches Player.cpp).
        m_item.transform.scaleX = kFrameW * kPixelScale * static_cast<float>(m_facing);
        m_item.transform.scaleY = kFrameH * kPixelScale;

        // Tint / alpha priority: death fade > hit flash > normal.
        if (m_dead) {
            const float t = (deathFadeTime > 0.0f) ? (m_deathTimer / deathFadeTime) : 0.0f;
            m_item.tint = Color4{ 1.0f, 1.0f, 1.0f, std::clamp(t, 0.0f, 1.0f) };
        } else if (m_flashTimer > 0.0f) {
            m_item.tint = Color4{ 1.0f, 0.4f, 0.4f, 1.0f };
        } else {
            m_item.tint = Color4{ 1.0f, 1.0f, 1.0f, 1.0f };
        }
        r2d.draw(m_item);
    }

    // ---------------------------------------------------------- takeDamage
    bool Enemy::takeDamage(int amount) {
        if (m_dead) return false;
        if (m_invulnTimer > 0.0f) return false;
        if (amount <= 0) return false;

        m_hp -= amount;
        m_invulnTimer = invulnAfterHit;
        m_flashTimer  = hitFlashTime;

        if (m_hp <= 0) {
            m_hp = 0;
            m_dead = true;
            m_hitboxActive = false;                // stop any pending attacks
            m_deathTimer = deathFadeTime;
        }
        return true;
    }

    // ---------------------------------------------------------- hurtbox / hitbox
    // Hurtbox is anchored to the FEET so the box hugs the ground regardless
    // of how much transparent headroom the sprite frame has. Center Y sits
    // hurtHalfH above the feet, plus any hurtOffsetY tweak.
    AABB Enemy::hurtbox() const {
        AABB b{};
        const float fx = static_cast<float>(m_facing);
        b.cx = m_x    + hurtOffsetX * fx;
        b.cy = m_feetY + hurtHalfH  + hurtOffsetY;
        b.w  = hurtHalfW * 2.0f;
        b.h  = hurtHalfH * 2.0f;
        return b;
    }

    AABB Enemy::hitbox() const {
        AABB b{};
        const float fx = static_cast<float>(m_facing);
        b.cx = m_x    + hitOffsetX * fx;
        b.cy = m_feetY + hitHalfH  + hitOffsetY;
        b.w  = hitHalfW * 2.0f;
        b.h  = hitHalfH * 2.0f;
        return b;
    }

    // -------------------------------------------------------------- internals
    void Enemy::applyAnimFrameToRenderItem() {
        // Even while dead we keep the last frame -- the fade-out drives
        // opacity, not the animation. This matches Player's approach of
        // freezing on the last landed frame.
        m_idleAnim.apply(m_item);
    }
}
```

Key implementation notes (things the compiler cannot enforce but a
future reader needs to know):

* **Position anchoring** — `m_feetY` is captured at spawn and never
  changes. If Item 09 gives the enemy walking, feed the tile-solver
  result back into `m_feetY = m_box.cy - m_box.h * 0.5f` the same
  way Player does in `syncRenderFromBox`. The hurtbox math already
  keys off `m_feetY`, so ground-hugging just works.
* **Facing mirror** — negating `scaleX` is the exact same trick
  Player uses (`Player.cpp:212`). No cell-flip in the shader is
  needed; the quad's UVs stay put and OpenGL flips the geometry.
* **Sort layer 99** — puts the enemy behind the player (layer 100)
  and behind bullets (101). This makes the "player shot a bullet
  that hit the enemy" moment read clearly even when the player is
  standing right next to the enemy.
* **`isFinished()` short-circuits render** — as soon as the death
  fade completes the sprite is invisible. `EnemyManager` uses the
  same flag to compact the vector.
* **Invulnerability window** is tiny (0.08 s). It's just there to
  prevent a single 60 fps frame from counting one bullet as two
  hits when the bullet's step-size is comparable to the hurtbox
  size. If you find enemies dying in a single trigger-pull, bump
  it to 0.15 s.
* **Hitbox stays inactive in Item 08** — `m_hitboxActive` is
  `false`, so `hitboxActive()` returns `false` and the B overlay
  won't draw the red rectangle yet.

---

## 3. Player hurtbox accessor

Player already has an AABB (its collision box). Expose it so the B
overlay can outline it. Open `include/Game/Player.h` and add one
inline getter next to the other geometry accessors (right after
`feetY()`):

```cpp
        float feetY() const { return m_box.cy - m_box.h * 0.5f; }

        // Player's collision box doubles as its hurtbox for now (used by
        // the item-08 debug overlay; Item 10 introduces the enemy hitbox
        // check that reads this).
        HBE::Renderer::AABB hurtbox() const { return m_box; }
```

That is the only Player change in Item 08. No new fields, no new
`.cpp` code.

---

Next: `02_enemy_manager_and_bullet_integration.md` — the manager
that owns enemies, the tiny `BulletManager` patch that exposes the
alive-bullet list, and the hit-test loop that ties the two
together.
