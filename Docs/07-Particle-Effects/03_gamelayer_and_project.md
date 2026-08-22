# Item 07 · Doc 03 — GameLayer Wiring & Project Registration

Wire the new `MegaX::Effects` into the game layer and register the two new
files in the project. All changes here are game‑side — no engine edits.

---

## 1. `include/Game/GameLayer.h`

Add the include and the member. Nothing else changes.

```cpp
#include "Game/Effects.h"
...
        BulletManager m_bullets{};
        Effects       m_effects{};                                   // NEW
        const HBE::Renderer::TileMapLayer* m_ground = nullptr;
```

We deliberately place `m_effects` **after** `m_bullets` so its destructor
runs before the bullet manager's — nothing depends on that order today,
but it keeps the "world → player → bullets → effects" narrative in the
file as well.

---

## 2. `src/Game/GameLayer.cpp`

### `onAttach` — initialize Effects **after** the world is loaded

The particle system's tile-top colour cache needs the world's map, and
the casing simulator (see `01_effects_configs_and_class.md`) needs the
solid layer so casings can bounce off tiles. Init order is:

```
buildSpritePipeline  ->  m_world.load  ->  m_player.init
->  m_bullets.init  ->  m_effects.init
```

Add the init call right after `m_bullets.init`:

```cpp
        if (!m_bullets.init(app.resources(), m_quadMesh, m_spriteShader)) {
            LogError("MegaX GameLayer: bullet manager init failed.");
        }

        if (!m_effects.init(app.resources(), m_quadMesh, m_world.map(), m_ground)) {
            LogError("MegaX GameLayer: effects init failed.");
        }
```

`m_ground` is the same `TileMapLayer*` you already resolved via
`m_world.map().findLayer("Ground")` for player + bullet collisions.

The `ParticleSystem` inside `Effects` will look the sprite shader up out
of `ResourceCache` by the well‑known name `"sprite"` (already registered
in `buildSpritePipeline`) and build its own 1×1 white texture. We do
**not** need to hand it the shader pointer.

### `onUpdate` — spawn everything, then advance the system

Insert the effect spawns right after the existing shot / bullet block,
and add a single `m_effects.update(dt)` at the end of the function.

```cpp
        // spawn any bullet the player fired this frame, then advance bullets
        float bx, by; int bdir;
        if (m_player.consumeShot(bx, by, bdir)) {
            m_bullets.spawn(bx, by, bdir);

            // item 07: firing effects use the exact muzzle coords the
            // player published, so they can't drift from the bullet.
            m_effects.spawnMuzzleFlash(bx, by, bdir);
            // Casings eject from the ejection port near the gun body, not the
            // barrel tip: ~5 px in front of the player center, at gun height.
            const float casingX = m_player.x() + static_cast<float>(bdir) * 5.0f;
            m_effects.spawnCasing    (casingX, by, bdir);
        }
        m_bullets.update(dt, &m_world.map(), m_ground, m_camera.camera());

        // item 07: impact bursts for every bullet that died on a tile
        {
            std::vector<BulletManager::Impact> impacts;
            if (m_bullets.consumeImpacts(impacts)) {
                for (const auto& imp : impacts) {
                    m_effects.spawnBulletImpact(imp.x, imp.y, imp.tileId);
                }
            }
        }

        // item 07: landing burst on the frame the player touched down
        if (m_player.landedThisFrame()) {
            m_effects.spawnLandingDust(m_player.x(),
                                       m_player.feetY(),
                                       m_player.groundTileId());
        }

        // item 07: continuous walk dust — only ticks while grounded + moving
        {
            const bool moving = (ix != 0.0f);
            const bool grounded = (m_player.velY() == 0.0f) &&   // cheap heuristic
                                  (m_player.groundTileId() != 0);
            m_effects.tickWalkDust(dt,
                                   m_player.x(), m_player.feetY(),
                                   m_player.groundTileId(),
                                   moving, grounded);
        }

        // camera follow (existing) ------------------------------------
        m_camera.setFollowTarget(m_player.x(), m_player.y());
        m_camera.setFollowVelocity(m_player.velX(), m_player.velY());
        m_camera.update(dt);
        m_app->gl().setCamera(m_camera.camera());

        // item 07: advance every emitter, then we're done for the frame
        m_effects.update(dt);
```

Two notes:

- The **grounded heuristic** above uses `groundTileId() != 0` because
  `Player` doesn't expose `m_grounded` today, and Ghost mode always
  reports `groundTileId() == 0` (it never samples). If you want a
  first‑class `grounded()` accessor on `Player`, add it in the same
  edit — it's a one‑liner:

  ```cpp
  // include/Game/Player.h  (public)
  bool grounded() const { return m_grounded; }
  ```

  and replace the heuristic with `m_player.grounded()`.
- Ghost mode never fires bullets (item 06 already blocked shooting in
  `updatePlay`), so muzzle / casing / impact effects can't happen there.
  Ghost mode also never sets `landedThisFrame`, since `updateGhost` does
  not go through the transition. Walk dust likewise gets suppressed
  because `groundTileId()` is 0 in Ghost. All good.

### `onRender` — draw effects last inside the world scene

```cpp
    void GameLayer::onRender() {
        Renderer2D& r2d = m_app->renderer2D();

        r2d.beginScene(m_camera.camera(), RenderPass::World);
        m_world.render(r2d);
        m_player.render(r2d);
        m_bullets.render(r2d);
        m_effects.render(r2d);          // NEW
        r2d.endScene();
    }
```

Effects sort by `EmitterConfig::sortLayer` (walk/land dust at 95 sit
behind the player at 100; muzzle/casing/impact at 102 sit in front of
bullets at 101). Drawing effects last inside the same scene lets the
engine's layer sort do the right thing without reshuffling submits.

---

## 3. `MegaX.vcxproj` — register the new files

Add the header to the existing `ClInclude` group and the source to the
existing `ClCompile` group. Match the ordering / style of the neighbouring
`Bullet.h` / `Bullet.cpp` entries.

Header section (around the existing `Bullet.h` line):

```xml
    <ClInclude Include="include\Game\GameLayer.h" />
    <ClInclude Include="include\Game\Player.h" />
    <ClInclude Include="include\Game\Bullet.h" />
    <ClInclude Include="include\Game\Effects.h" />                <!-- NEW -->
    <ClInclude Include="include\World\AnimatedTile.h" />
    <ClInclude Include="include\World\World.h" />
```

Source section (around the existing `Bullet.cpp` line):

```xml
    <ClCompile Include="src\Game\GameLayer.cpp" />
    <ClCompile Include="src\Game\Player.cpp" />
    <ClCompile Include="src\Game\Bullet.cpp" />
    <ClCompile Include="src\Game\Effects.cpp" />                  <!-- NEW -->
    <ClCompile Include="src\main.cpp" />
    <ClCompile Include="src\World\World.cpp" />
```

Without those two entries you'll get unresolved `Effects` symbols at link
time.

## 4. `MegaX.vcxproj.filters` — mirror the new entries

The `.filters` file (used for Solution Explorer grouping) currently lists
the same files but is missing `Bullet.h` / `Bullet.cpp` too (see
`06-Player-Shooting-And-Bullets` if you haven't already fixed it). Add
`Effects.h` and `Effects.cpp` next to the other `Game/` entries:

```xml
    <ClInclude Include="include\Game\GameLayer.h" />
    <ClInclude Include="include\Game\Player.h" />
    <ClInclude Include="include\Game\Bullet.h" />          <!-- add if missing -->
    <ClInclude Include="include\Game\Effects.h" />         <!-- NEW -->
    <ClInclude Include="include\World\AnimatedTile.h" />
    <ClInclude Include="include\World\World.h" />
```

```xml
    <ClCompile Include="src\Game\GameLayer.cpp" />
    <ClCompile Include="src\main.cpp" />
    <ClCompile Include="src\Game\Player.cpp" />
    <ClCompile Include="src\Game\Bullet.cpp" />            <!-- add if missing -->
    <ClCompile Include="src\Game\Effects.cpp" />           <!-- NEW -->
    <ClCompile Include="src\World\World.cpp" />
```

(Purely cosmetic — Visual Studio uses this to place files in the right
solution-explorer folder. The build still succeeds without it.)

---

## 5. Full input map (unchanged)

| Key | Action |
|-----|--------|
| A / D | Move left / right |
| Space | Jump (Play) / fly up (Ghost) |
| S | Crouch (Play) / fly down (Ghost) |
| E | **Fire** (tap = one shot, hold = auto-fire) |
| H | Toggle helmet |
| G | Toggle Play ↔ Ghost |

> Note: the current codebase uses **`SDL_SCANCODE_E`** for fire (not `J`
> as an earlier doc mentioned). Item 07 doesn't change key bindings —
> whatever the player fires with is what generates the muzzle / casing
> effects.

---

Next: `04_build_run_and_verify.md` — build, run, verify checklist,
tuning cheat‑sheet, and troubleshooting.
