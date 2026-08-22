# 04 — MegaX verification instrumentation

> **SCOPE NOTE.** This is the *only* MegaX file the Item 13
> tag `[HBE]` allows us to edit, and only for the narrow
> purpose of proving MegaX can consume the new profiler
> API (Item 13's "done" criterion #3 requires MegaX to
> time at least five nested systems). No gameplay logic
> changes here. Every edit is either an `HBE_PROFILE_SCOPE`
> macro or a single scope brace pair. If you delete every
> edit in this doc, MegaX still runs identically to Item 12.

**File touched:** `G:\Dev\HBE\MegaX\src\Game\GameLayer.cpp`
(only).

All new lines come from macros defined in
`HBE/Core/Profiler.h`, which is already visible through
`Game/GameLayer.h -> HBE/Core/Layer.h`? Let's not gamble on
transitive includes — doc §1 below adds an explicit include.

Sandbox and MapMaker are untouched, as required by
`[HBE]` scope. The engine already ships the profiler
regardless of these MegaX edits.

---

## 1. `GameLayer.cpp` — add the include

Open `G:\Dev\HBE\MegaX\src\Game\GameLayer.cpp`.

The current include block reads (lines 1-12):

```cpp
#include "Game/GameLayer.h"
#include "Game/EnemyBullet.h"

#include "HBE/Core/Application.h"
#include "HBE/Core/AssetPaths.h"
#include "HBE/Core/Log.h"

#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/RenderPass.h"

#include "HBE/Platform/Input.h"
```

Insert `HBE/Core/Profiler.h` **right after** the line
`#include "HBE/Core/Log.h"` (line 6):

```cpp
#include "HBE/Core/Profiler.h"
```

Final include block (relevant portion):

```cpp
#include "HBE/Core/Application.h"
#include "HBE/Core/AssetPaths.h"
#include "HBE/Core/Log.h"
#include "HBE/Core/Profiler.h"

#include "HBE/Renderer/Renderer2D.h"
```

> **WHY EXPLICITLY?** Even though `Application.h` transitively
> includes `Profiler.h` (doc 03 §1), being explicit here means
> a future refactor of the engine include chain can't
> silently break MegaX's profiling.

---

## 2. `GameLayer::onUpdate` — wrap the scene body

The current signature and prelude (lines 91-93) are:

```cpp
	void GameLayer::onUpdate(float dt) {
        m_watcher.poll(dt);
```

We are going to insert **one** `HBE_PROFILE_SCOPE("SceneUpdate")`
on the very first line of the function body, so the whole
`onUpdate` counts as `SceneUpdate` and every inner scope
becomes a child of it in the profiler tree.

### 2.1  Insert `SceneUpdate`

Change:

```cpp
	void GameLayer::onUpdate(float dt) {
        m_watcher.poll(dt);
```

to:

```cpp
	void GameLayer::onUpdate(float dt) {
        HBE_PROFILE_SCOPE("SceneUpdate");
        m_watcher.poll(dt);
```

That single line is enough for `SceneUpdate` to bracket
everything until the closing `}` on line 240 (the current
line that reads `}` after `m_effects.update(dt);`).

### 2.2  Insert `Physics`, `AI`, `Combat`, `Particles`

We now add **four** child scopes around specific blocks in
`onUpdate`. Each is a plain `{ ... }` block with a
`HBE_PROFILE_SCOPE(...)` on its first line.

#### Physics — around player + bullet + world movement

Currently lines 165-183 read:

```cpp
        m_world.update(dt);
		m_player.setMoveInput(ix, iy);
		m_player.setJumpInput(jumpPressed, jumpHeld);
		m_player.setCrouchInput(crouchHeld);
		m_player.setFireInput(firePressed, fireHeld);
		m_player.update(dt);

		float bx, by; int bdir;
		if (m_player.consumeShot(bx, by, bdir)) {
			m_bullets.spawn(bx, by, bdir);

            m_enemies.notifyGunshot(bx, by);

            m_effects.spawnMuzzleFlash(bx, by, bdir);
            const float casingX = m_player.x() + static_cast<float>(bdir) * 5.0f;
            m_effects.spawnCasing(casingX, by, bdir);
		}
		m_bullets.update(dt, &m_world.map(), m_ground, m_camera.camera());
```

Wrap this whole block with a `Physics` scope. Change the
block so it becomes:

```cpp
        {
            HBE_PROFILE_SCOPE("Physics");
            m_world.update(dt);
            m_player.setMoveInput(ix, iy);
            m_player.setJumpInput(jumpPressed, jumpHeld);
            m_player.setCrouchInput(crouchHeld);
            m_player.setFireInput(firePressed, fireHeld);
            m_player.update(dt);

            float bx, by; int bdir;
            if (m_player.consumeShot(bx, by, bdir)) {
                m_bullets.spawn(bx, by, bdir);

                m_enemies.notifyGunshot(bx, by);

                m_effects.spawnMuzzleFlash(bx, by, bdir);
                const float casingX = m_player.x() + static_cast<float>(bdir) * 5.0f;
                m_effects.spawnCasing(casingX, by, bdir);
            }
            m_bullets.update(dt, &m_world.map(), m_ground, m_camera.camera());
        }
```

Yes, the indentation shifts by one level. That's fine — it
matches how the existing `{ std::vector<...> impacts; ... }`
blocks below are formatted.

#### Combat — around the "check bullet hits" + impact drain block

Currently lines 185-194 read:

```cpp
        m_enemies.checkBulletHits(m_bullets, &m_effects, 1);

        {
            std::vector<BulletManager::Impact> impacts;
            if (m_bullets.consumeImpacts(impacts)) {
                for (const auto& imp : impacts) {
                    m_effects.spawnBulletImpact(imp.x, imp.y, imp.tileId);
                }
            }
        }
```

Wrap **both** statements in a single `Combat` scope:

```cpp
        {
            HBE_PROFILE_SCOPE("Combat");
            m_enemies.checkBulletHits(m_bullets, &m_effects, 1);

            {
                std::vector<BulletManager::Impact> impacts;
                if (m_bullets.consumeImpacts(impacts)) {
                    for (const auto& imp : impacts) {
                        m_effects.spawnBulletImpact(imp.x, imp.y, imp.tileId);
                    }
                }
            }
        }
```

#### AI — around `m_enemies.update(dt)` + enemy-bullet flow + player-hurtbox check

Currently lines 210-237 read (the block starting with
`m_enemies.update(dt);` through the closing brace of the
player-hurtbox `for (auto& b : ebm.bullets())` block):

```cpp
        m_enemies.update(dt);

        auto& ebm = m_enemies.enemyBullets();
        ebm.update(dt, &m_world.map(), m_ground, m_camera.camera());

        {
            std::vector<EnemyBulletManager::Impact> impacts;
            if (ebm.consumeImpacts(impacts)) {
                for (const auto& imp : impacts) {
                    m_effects.spawnEnemyBulletImpact(imp.x, imp.y, imp.tileId);
                }
            }
        }

        {
            const AABB pb = m_player.hurtbox();
            for (auto& b : ebm.bullets()) {
                if (!b.alive) continue;
                if (std::fabs(b.x - pb.cx) > pb.w * 0.5f) continue;
                if (std::fabs(b.y - pb.cy) > pb.h * 0.5f) continue;

                const int kbDir = (b.vx >= 0.0f) ? +1 : -1;
                if (m_player.takeDamage(b.damage, kbDir)) {
                    m_effects.spawnBloodSplatter(b.x, b.y, kbDir);
                    b.alive = false;
                }
            }
        }
```

Wrap the entire span in an `AI` scope:

```cpp
        {
            HBE_PROFILE_SCOPE("AI");
            m_enemies.update(dt);

            auto& ebm = m_enemies.enemyBullets();
            ebm.update(dt, &m_world.map(), m_ground, m_camera.camera());

            {
                std::vector<EnemyBulletManager::Impact> impacts;
                if (ebm.consumeImpacts(impacts)) {
                    for (const auto& imp : impacts) {
                        m_effects.spawnEnemyBulletImpact(imp.x, imp.y, imp.tileId);
                    }
                }
            }

            {
                const AABB pb = m_player.hurtbox();
                for (auto& b : ebm.bullets()) {
                    if (!b.alive) continue;
                    if (std::fabs(b.x - pb.cx) > pb.w * 0.5f) continue;
                    if (std::fabs(b.y - pb.cy) > pb.h * 0.5f) continue;

                    const int kbDir = (b.vx >= 0.0f) ? +1 : -1;
                    if (m_player.takeDamage(b.damage, kbDir)) {
                        m_effects.spawnBloodSplatter(b.x, b.y, kbDir);
                        b.alive = false;
                    }
                }
            }
        }
```

> **NAMING NOTE.** "AI" is the section-name from the work
> item. In practice it covers enemy tick + enemy-bullet
> physics + player-hurtbox collision. That's fine —
> "SystemName" for Item 13 is a label the profiler treats as
> pointer identity, not a semantic promise.

#### Particles — around `m_effects.update(dt);`

Currently line 239 reads:

```cpp
        m_effects.update(dt);
```

Replace with:

```cpp
        {
            HBE_PROFILE_SCOPE("Particles");
            m_effects.update(dt);
        }
```

### 2.3  Everything not wrapped (intentional)

The following pieces of `onUpdate` are **intentionally left
outside** any child scope:

* Input polling (lines 94-108) — sub-microsecond, noise.
* Hotkey handling (lines 112-164) — user input branches
  that only occasionally fire.
* Player landing dust + walk dust burst (lines 195-208) —
  cheap enough that a nested scope is more expensive than
  the code it wraps. If a future perf spike changes that,
  add a `PlayerFXTick` scope then.
* Camera update (lines 205-208) — sub-microsecond.

Only work that will show up in the profiler tree needs a
scope. Everything else contributes to the outer
`SceneUpdate` currentMs so its ~= `Physics + Combat + AI + Particles`
plus a small "everything else" residual — a useful sanity
signal.

---

## 3. `GameLayer::onRender` — wrap tile + sprite rendering

The current body of `onRender` (lines 242-266) reads:

```cpp
	void GameLayer::onRender() {
		Renderer2D& r2d = m_app->renderer2D();

		r2d.beginScene(m_camera.camera(), RenderPass::World);
        m_world.render(r2d);
        m_enemies.render(r2d);
        m_enemies.renderBubbles(m_debug, r2d);
		m_player.render(r2d);
		m_bullets.render(r2d);
        m_enemies.enemyBullets().render(r2d);

        m_effects.render(r2d);

        if (m_showHitBoxes) {
            m_enemies.debugDrawBoxes(m_debug, r2d);
            m_enemies.debugDrawSenses(m_debug, r2d);
            const HBE::Renderer::AABB pb = m_player.hurtbox();
            m_debug.rect(r2d, pb.cx, pb.cy, pb.w, pb.h, 0.35f, 0.55f, 1.0f, 1.0f, false);
            m_enemies.debugDrawBoxes(m_debug, r2d);
        }

        drawHud(r2d);

		r2d.endScene();
	}
```

We're going to add **two** scopes: `TileRendering` around
`m_world.render(r2d)` (which draws the tile map), and
`SpriteRendering` around the four sprite-render calls
(enemies + bubbles + player + bullets + enemy bullets +
effects). The `if (m_showHitBoxes) { ... }` block and
`drawHud(r2d)` stay outside — they're debug overlays whose
cost is not part of "gameplay rendering."

Change the block above to:

```cpp
	void GameLayer::onRender() {
		Renderer2D& r2d = m_app->renderer2D();

		r2d.beginScene(m_camera.camera(), RenderPass::World);

        {
            HBE_PROFILE_SCOPE("TileRendering");
            m_world.render(r2d);
        }

        {
            HBE_PROFILE_SCOPE("SpriteRendering");
            m_enemies.render(r2d);
            m_enemies.renderBubbles(m_debug, r2d);
            m_player.render(r2d);
            m_bullets.render(r2d);
            m_enemies.enemyBullets().render(r2d);
            m_effects.render(r2d);
        }

        if (m_showHitBoxes) {
            m_enemies.debugDrawBoxes(m_debug, r2d);
            m_enemies.debugDrawSenses(m_debug, r2d);
            const HBE::Renderer::AABB pb = m_player.hurtbox();
            m_debug.rect(r2d, pb.cx, pb.cy, pb.w, pb.h, 0.35f, 0.55f, 1.0f, 1.0f, false);
            m_enemies.debugDrawBoxes(m_debug, r2d);
        }

        drawHud(r2d);

		r2d.endScene();
	}
```

> **NOTE ON ORDERING.** `Renderer2D` batches draws until
> `endScene()`. That means `TileRendering` mostly measures
> the *submit* cost, not the GPU dispatch cost. That's fine
> for CPU-time profiling — Item 14 (GPU timers) will pick
> up the actual GPU cost. Item 13's "done" criterion is
> "millisecond values after every frame", which the CPU-side
> submit time delivers on.

---

## 4. Nested-scope inventory (for verification)

After all edits, running MegaX should produce a scope tree
with **exactly** the following named sections per frame:

| Section              | Depth | Parent            | Source of the scope             |
|----------------------|-------|-------------------|---------------------------------|
| `ApplicationUpdate`  | 0     | (top level)       | `Application::run` (doc 03)     |
| `SceneUpdate`        | 1     | ApplicationUpdate | `GameLayer::onUpdate` (§2.1)    |
| `Physics`            | 2     | SceneUpdate       | onUpdate wrap (§2.2)            |
| `Combat`             | 2     | SceneUpdate       | onUpdate wrap (§2.2)            |
| `AI`                 | 2     | SceneUpdate       | onUpdate wrap (§2.2)            |
| `Particles`          | 2     | SceneUpdate       | onUpdate wrap (§2.2)            |
| `Audio`              | 0     | (top level)       | `Application::run` (doc 03)     |
| `TileRendering`      | 0     | (top level)       | `GameLayer::onRender` (§3)      |
| `SpriteRendering`    | 0     | (top level)       | `GameLayer::onRender` (§3)      |

That's **9 named sections**, well above the "at least five
nested systems" bar. Four of them (`Physics`, `Combat`,
`AI`, `Particles`) are nested at depth 2 under `SceneUpdate`
which is itself nested at depth 1 under `ApplicationUpdate`
— the strongest possible read of "nested".

If you count sections and see fewer than nine in doc `05`'s
console output:

* Missing `Physics/Combat/AI/Particles` -> you forgot one of
  the four `{ HBE_PROFILE_SCOPE(...); ... }` wraps in §2.2.
* Missing `SceneUpdate` -> the top-of-function line in §2.1
  didn't take.
* Missing `TileRendering/SpriteRendering` -> `onRender`
  edits from §3 didn't take.
* Missing `ApplicationUpdate` or `Audio` -> doc 03 §2.2
  didn't take.

---

## 5. What NOT to touch

* Do **not** add a scope inside `m_effects.tickWalkDust` or
  `spawn...` calls. Those live in `Effects.cpp` and are
  covered by the outer `SceneUpdate` measurement.
* Do **not** add a `Render` root scope. Doc 03 §4 explains
  the "no root render scope in HBE" policy — the game names
  its own render sections.
* Do **not** wrap `drawHud(r2d)` in a scope. HUD cost is
  intentionally left as "unattributed" residual.
* Do **not** wrap `r2d.beginScene(...)` / `r2d.endScene()`
  in profiler scopes. The batching driver call cost is
  cross-cutting and would double-count into
  `SpriteRendering`.
* Do **not** touch `GameLayer.h`. No new members, no new
  methods.
* Do **not** touch the vcxproj — no new source files.

---

## 6. Sanity check

```powershell
Select-String -Path G:\Dev\HBE\MegaX\src\Game\GameLayer.cpp -Pattern "HBE_PROFILE_SCOPE"
```

Should return **exactly 7** matches:

```
1x SceneUpdate
1x Physics
1x Combat
1x AI
1x Particles
1x TileRendering
1x SpriteRendering
```

If you see fewer, re-run doc §2 and §3.

Next: `05_build_run_and_verify.md`.
