# 03 — GameLayer wiring & the B debug toggle

This step wires the new `EnemyManager` into the existing
`GameLayer`, drops a single Robot Soldier into the world at a
hard-coded spawn point, and adds a `DebugDraw2D`-based hit / hurt
box overlay controlled by the `B` key.

All changes are inside `G:\Dev\HBE\MegaX\`. No engine edits.

---

## 1. Header — `include/Game/GameLayer.h`

Open the file and:

1. Add three new includes near the top with the other game headers.
2. Add three new members to the class.

The final relevant sections should look like this (unchanged bits
elided):

```cpp
#pragma once

#include "HBE/Core/Layer.h"
#include "HBE/Renderer/CameraController.h"
#include "HBE/Renderer/DebugDraw2D.h"          // NEW: hit/hurt box overlay

#include "Game/Player.h"
#include "Game/Bullet.h"
#include "Game/Effects.h"
#include "Game/EnemyManager.h"                 // NEW: enemies
#include "World/World.h"

namespace HBE::Core { class Application; }

namespace MegaX {

    class GameLayer : public HBE::Core::Layer {
    public:
        void onAttach(HBE::Core::Application& app) override;
        void onUpdate(float dt) override;
        void onRender() override;

    private:
        void buildSpritePipeline();

        HBE::Core::Application* m_app = nullptr;

        HBE::Renderer::Mesh*     m_quadMesh = nullptr;
        HBE::Renderer::GLShader* m_spriteShader = nullptr;

        HBE::Renderer::CameraController m_camera{};
        Player        m_player{};
        World         m_world{};
        BulletManager m_bullets{};
        Effects       m_effects{};
        EnemyManager  m_enemies{};                     // NEW
        const HBE::Renderer::TileMapLayer* m_ground = nullptr;

        // ---- item 08 debug overlay -------------------------------------
        HBE::Renderer::DebugDraw2D m_debug{};          // NEW: outlined rects
        bool                       m_showHitboxes = false; // toggled by B
    };
}
```

That is the entire header change. `DebugDraw2D`'s definition
(`HBE/Renderer/DebugDraw2D.h`) only pulls in `RenderItem.h`,
`Material.h`, `Color.h` — none of which drag in the second
`SpriteRenderer2D` header, so no pimpl workaround is needed.

---

## 2. Implementation — `src/Game/GameLayer.cpp`

There are five edit blocks inside this file. Do them in order.

### 2a. Extra include at the top

The other includes near the top of the file already cover most of
what we need; just make sure the `DebugDraw2D` header is available
(the transitive include from `GameLayer.h` is enough, so **no
extra `#include` is required** in the `.cpp`).

### 2b. `onAttach` — init the debug drawer and spawn the enemy

Find the block that reads:

```cpp
        if (!m_effects.init(app.resources(), m_quadMesh, m_world.map(), m_ground)) {
            LogError("MegaX GameLayer: effects init failed.");
        }

        const float startX = m_world.pixelWidth() * 0.5f;
        const float startY = m_world.pixelHeight() * 0.5f;
        m_player.setPosition(startX, startY);
        m_camera.snapTo(startX, startY);
        app.gl().setCamera(m_camera.camera());

        LogInfo("MegaX GameLayer attached (Play mode; press G for Ghost).");
```

Insert three additions:

```cpp
        if (!m_effects.init(app.resources(), m_quadMesh, m_world.map(), m_ground)) {
            LogError("MegaX GameLayer: effects init failed.");
        }

        // Item 08 -----------------------------------------------------------
        if (!m_enemies.init(app.resources(), m_quadMesh, m_spriteShader)) {
            LogError("MegaX GameLayer: enemy manager init failed.");
        }
        if (!m_debug.initialize(app.resources(), m_quadMesh)) {
            LogError("MegaX GameLayer: debug draw init failed (B overlay disabled).");
        }
        // -------------------------------------------------------------------

        const float startX = m_world.pixelWidth() * 0.5f;
        const float startY = m_world.pixelHeight() * 0.5f;
        m_player.setPosition(startX, startY);
        m_camera.snapTo(startX, startY);
        app.gl().setCamera(m_camera.camera());

        // Item 08: hard-coded enemy spawn (data-driven spawns are a
        // future work item). Drop the Robot Soldier ~5 tiles to the
        // right of the player, at the same ground height.
        {
            constexpr float kTilePx = 32.0f;
            const float ex = startX + 5.0f * kTilePx;
            const float eGroundY = m_player.feetY();
            if (Enemy* e = m_enemies.spawn(ex, eGroundY, /*facing left*/ -1)) {
                // Optional per-spawn HP tweak. Because Enemy::spawn re-reads
                // startHp each call, we override then re-spawn so the new
                // value takes effect on this instance. Skip both lines if
                // the default startHp (3) is fine.
                e->startHp = 3;
                e->spawn(ex, eGroundY, -1);
            }
        }

        LogInfo("MegaX GameLayer attached (Play mode; press G for Ghost).");
```

Why the second `e->spawn(...)`?  `EnemyManager::spawn` already
called `Enemy::spawn`, which snapshotted `m_maxHp` from `startHp`.
Bumping `startHp` after the fact doesn't retroactively change the
current HP, so we re-spawn to re-snapshot. If you leave `startHp`
alone you can delete both lines and just use the pointer for
future tuning (hurtbox size, hitOffsetY, etc.).

**Ground height caveat:** `m_player.feetY()` is only correct if
the player has already moved to `startX, startY` and had one
update tick to settle onto the ground. On the frame the level
first loads, `m_player.feetY()` equals the *initial* box bottom
which may be a few pixels above the actual floor. If your enemy
spawns slightly floating, either wait a frame and re-anchor, or
pass a hand-tuned world Y directly (e.g.
`eGroundY = startY + 20.0f;` if that visually lands on the floor
in your test map).

### 2c. `onUpdate` — B toggle + drive enemies + hit test

Find the block that reads (in the input section):

```cpp
        // G toggles Play <-> Ghost (fly, no gravity/collision — for map building)
        if (Input::IsKeyPressed(SDL_SCANCODE_G)) {
            m_player.toggleMode();
            LogInfo(m_player.mode() == Player::Mode::Ghost
                ? "MegaX: Ghost mode (fly, no collision)."
                : "MegaX: Play mode (gravity + collision).");
        }
```

Add the `B` toggle right after it:

```cpp
        if (Input::IsKeyPressed(SDL_SCANCODE_G)) {
            /* ... unchanged ... */
        }

        // Item 08: B toggles the hit / hurt box debug overlay.
        if (Input::IsKeyPressed(SDL_SCANCODE_B)) {
            m_showHitboxes = !m_showHitboxes;
            LogInfo(m_showHitboxes
                ? "MegaX: hit/hurt box overlay ON."
                : "MegaX: hit/hurt box overlay OFF.");
        }
```

Next, find the block that reads:

```cpp
        m_bullets.update(dt, &m_world.map(), m_ground, m_camera.camera());

        {
            std::vector<BulletManager::Impact> impacts;
            if (m_bullets.consumeImpacts(impacts)) {
                for (const auto& imp : impacts) {
                    m_effects.spawnBulletImpact(imp.x, imp.y, imp.tileId);
                }
            }
        }
```

Insert the enemy hit test **between** `m_bullets.update(...)` and
the tile-impact drain block. The order matters: bullets that hit
an enemy this frame should be consumed before the tile-impact
drainer runs, otherwise a bullet could produce both an enemy-hit
burst AND a tile-impact burst in the same frame.

```cpp
        m_bullets.update(dt, &m_world.map(), m_ground, m_camera.camera());

        // Item 08: convert alive bullets into HP loss on enemies.
        m_enemies.checkBulletHits(m_bullets, &m_effects, /*damagePerBullet*/ 1);

        {
            std::vector<BulletManager::Impact> impacts;
            if (m_bullets.consumeImpacts(impacts)) {
                for (const auto& imp : impacts) {
                    m_effects.spawnBulletImpact(imp.x, imp.y, imp.tileId);
                }
            }
        }
```

Finally, add `m_enemies.update(dt)` alongside the other end-of-
frame updates. Find:

```cpp
        m_camera.setFollowTarget(m_player.x(), m_player.y());
        m_camera.setFollowVelocity(m_player.velX(), m_player.velY());
        m_camera.update(dt);
        m_app->gl().setCamera(m_camera.camera());

        m_effects.update(dt);
```

Add one line:

```cpp
        m_camera.setFollowTarget(m_player.x(), m_player.y());
        m_camera.setFollowVelocity(m_player.velX(), m_player.velY());
        m_camera.update(dt);
        m_app->gl().setCamera(m_camera.camera());

        m_enemies.update(dt);            // NEW: ticks anim, timers, compaction
        m_effects.update(dt);
```

Order matters: `m_enemies.update(dt)` runs before `m_effects.update(dt)`
so that any bullet-impact bursts the hit-test spawned this frame
are advanced by the same `dt` as everything else.

### 2d. `onRender` — draw enemies and the B overlay

Find the current render body:

```cpp
    void GameLayer::onRender() {
        Renderer2D& r2d = m_app->renderer2D();

        r2d.beginScene(m_camera.camera(), RenderPass::World);
        m_world.render(r2d);
        m_player.render(r2d);
        m_bullets.render(r2d);
        m_effects.render(r2d);
        r2d.endScene();
    }
```

Replace it with:

```cpp
    void GameLayer::onRender() {
        Renderer2D& r2d = m_app->renderer2D();

        r2d.beginScene(m_camera.camera(), RenderPass::World);
        m_world.render(r2d);
        m_enemies.render(r2d);       // NEW: enemies behind the player
        m_player.render(r2d);
        m_bullets.render(r2d);
        m_effects.render(r2d);

        // Item 08: hit/hurt box overlay. Drawn INSIDE the World scene so
        // the outlines share the world-space camera transform. DebugDraw2D
        // uses layer 9000, so it always appears above the sprites.
        if (m_showHitboxes) {
            const HBE::Renderer::AABB pb = m_player.hurtbox();
            m_debug.rect(r2d, pb.cx, pb.cy, pb.w, pb.h,
                         0.35f, 0.55f, 1.0f, 1.0f, /*filled*/ false);
            m_enemies.debugDrawBoxes(m_debug, r2d);
        }
        r2d.endScene();
    }
```

Enemy draw goes **before** the player so, when the player is
standing on top of the enemy, the player's silhouette wins.

---

## 3. Sanity check — what your final `GameLayer.cpp` should touch

If you're diff-checking your work, exactly four semantic changes
should show up:

1. New `#include "HBE/Renderer/DebugDraw2D.h"` transitively via
   `GameLayer.h`; no explicit include in `.cpp`.
2. Three new members declared in `GameLayer.h`
   (`m_enemies`, `m_debug`, `m_showHitboxes`).
3. In `onAttach`: `m_enemies.init`, `m_debug.initialize`, and the
   spawn-point block.
4. In `onUpdate`: the `B` toggle, the `checkBulletHits` call, and
   the `m_enemies.update(dt)` line.
5. In `onRender`: `m_enemies.render(r2d)` and the `if
   (m_showHitboxes)` overlay block.

Everything else stays exactly as it is today.

---

Next: `04_build_run_and_verify.md` — vcxproj / filters updates,
the build command, the manual verify checklist, and a
troubleshooting reference.
