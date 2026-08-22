# 01 — Small reset/clear helpers (Effects, World, Player)

Before we can add `reloadScene(...)` to `GameLayer`, we need
three surgical additions to the pieces the reload will call:

1. `Effects::clear()` — drops every live particle + casing.
2. `World::reload(...)` — re-loads the last logical map path
   in place, restoring the previous map on failure.
3. `Player::resetForRespawn()` — zeroes shot-state / anim-state
   fields that `setPosition(...)` does not touch, so a fresh
   spawn never inherits "was firing when reload happened"
   state.

Every edit here is additive. No existing behavior changes; the
Item 10 code paths keep working unchanged.

---

## 1. `include/Game/Effects.h` — add `clear()` declaration

Open `G:\Dev\HBE\MegaX\include\Game\Effects.h`.

Find the existing public method block that contains
`void shutdown();` (around line 45, right after `bool init(...)`).
Add a new declaration **immediately below** `shutdown();`:

```cpp
        void shutdown();

        // Item 11: drop every live particle, casing and walk-dust accumulator
        // so a scene reload doesn't leave stale FX behind. Safe to call at
        // any time; a no-op if !m_initialized.
        void clear();
```

That is the only change to `Effects.h`. **Do not** add
`#include "HBE/Renderer/ParticleSystem.h"` — the pimpl comment
at the top of `Effects.h` is still in force.

---

## 2. `src/Game/Effects.cpp` — implement `Effects::clear()`

Open `G:\Dev\HBE\MegaX\src\Game\Effects.cpp`.

Find the existing definition of `Effects::shutdown()`. Add the
new function **immediately below** `shutdown()`'s closing brace,
still inside `namespace MegaX {`:

```cpp
    void Effects::clear() {
        if (!m_initialized) return;

        // Particles: engine ParticleSystem has a clear() that nukes every
        // live particle across every registered effect without touching
        // the registrations themselves. We keep the "walk_dust",
        // "muzzle_flash", "bullet_impact", "landing_dust" registrations
        // so subsequent spawnXxx() calls still work post-reload.
        if (m_ps) m_ps->clear();

        // Casings live in a plain vector we simulate ourselves.
        m_casings.clear();

        // Reset the walk-dust cadence so the first grounded frame after a
        // reload doesn't emit a dust puff instantly.
        m_walkAccum = 0.0f;
    }
```

If your ParticleSystem member is named `m_particles` instead of
`m_ps` (older revisions), match the actual field name in your
copy of `Effects.h` — but per the header quoted in Item 10's
build doc, MegaX uses `m_ps`. Grep `m_ps` in `Effects.cpp` to
confirm.

---

## 3. `include/World/World.h` — add `reload()` + path cache

Open `G:\Dev\HBE\MegaX\include\World\World.h`.

### 3a. Add the `reload(...)` public method

Find the existing `bool load(...)` declaration (around line
22). Add the new declaration **immediately below** the closing
`)` of `load(...)`:

```cpp
        bool load(HBE::Renderer::Renderer2D& r2d,
            HBE::Renderer::ResourceCache& resources,
            HBE::Renderer::GLShader* spriteShader,
            HBE::Renderer::Mesh* quadMesh,
            const std::string& logicalMapPath);

        // Item 11: re-load the LAST logical path we were successfully loaded
        // with. Uses the shader + quadMesh cached from the last successful
        // load(). Returns false if the last load hasn't succeeded yet, or
        // if the reload itself fails (the previous map stays in place on
        // failure -- see World.cpp).
        bool reload(HBE::Renderer::Renderer2D& r2d,
            HBE::Renderer::ResourceCache& resources);

        // Item 11: exposed so GameLayer / FileWatcher can Resolve() the same
        // absolute path the world was loaded from.
        const std::string& logicalMapPath() const { return m_logicalMapPath; }
```

### 3b. Add a cached path field

Find the private section (near the bottom, alongside
`m_animClock` and `m_loaded`). Add these two fields **below**
the existing `m_loaded` line:

```cpp
        bool m_loaded = false;

        // Item 11: remember what path we were loaded from so reload() can
        // find it again without the caller re-passing it.
        std::string m_logicalMapPath{};
```

`<string>` is already included at the top of `World.h` for
`std::string`, so no new include needed.

---

## 4. `src/World/World.cpp` — cache the path, implement `reload`

Open `G:\Dev\HBE\MegaX\src\World\World.cpp`.

### 4a. Cache the logical path inside `World::load`

Find the `World::load(...)` body. Very near the top, right
after the existing:

```cpp
        m_loaded = false;
        m_map = TileMap{};
        m_animatedTiles.clear();
        m_animClock = 0.0f;
        m_spriteShader = spriteShader;
        m_quadMesh = quadMesh;
```

Add ONE line to cache the path (this feeds `reload()`):

```cpp
        m_logicalMapPath = logicalMapPath;
```

The rest of `load(...)` is unchanged.

### 4b. Implement `World::reload`

Add this function **immediately after** the closing brace of
`World::load(...)`, still inside `namespace MegaX {`:

```cpp
    bool World::reload(Renderer2D& r2d, ResourceCache& resources) {
        if (m_logicalMapPath.empty()) {
            HBE::Core::LogError("World::reload: no prior successful load(); ignoring.");
            return false;
        }
        if (!m_spriteShader || !m_quadMesh) {
            HBE::Core::LogError("World::reload: spriteShader/quadMesh not cached; ignoring.");
            return false;
        }

        // We copy the current map aside so we can roll back on failure.
        // This is cheap: TileMap holds POD + a few vectors of tile ids.
        TileMap                        backupMap        = m_map;
        std::vector<AnimatedTile>      backupAnims      = m_animatedTiles;
        bool                           backupLoaded     = m_loaded;
        float                          backupAnimClock  = m_animClock;

        m_map = TileMap{};
        m_animatedTiles.clear();
        m_animClock = 0.0f;
        m_loaded = false;

        const std::string absPath = HBE::Core::AssetPaths::Resolve(m_logicalMapPath);

        std::string err;
        if (!TileMapLoader::loadFromJsonFile(absPath, m_map, &err)) {
            HBE::Core::LogError("World::reload: failed to load '"
                + m_logicalMapPath + "': " + err + " -- restoring previous map.");
            m_map            = std::move(backupMap);
            m_animatedTiles  = std::move(backupAnims);
            m_loaded         = backupLoaded;
            m_animClock      = backupAnimClock;
            return false;
        }

        if (!m_renderer.build(r2d, resources, m_spriteShader, m_quadMesh, m_map)) {
            HBE::Core::LogError("World::reload: TileMapRenderer::build failed for '"
                + m_logicalMapPath + "' -- restoring previous map.");
            m_map            = std::move(backupMap);
            m_animatedTiles  = std::move(backupAnims);
            m_loaded         = backupLoaded;
            m_animClock      = backupAnimClock;

            // The renderer already partially rebuilt against the (bad) map.
            // Force it to rebuild against the restored map so we're
            // consistent.
            m_renderer.build(r2d, resources, m_spriteShader, m_quadMesh, m_map);
            return false;
        }

        loadAnimatedTiles(resources, absPath);
        m_loaded = true;

        HBE::Core::LogInfo("World::reload: '" + m_logicalMapPath + "' reloaded ("
            + std::to_string(m_map.tilesets.size()) + " tilesets, "
            + std::to_string(m_map.layers.size()) + " layers).");
        return true;
    }
```

Key points:

* We snapshot the previous map + animated tiles up front. If
  either the JSON parse or the renderer build fails, we roll
  back and the caller keeps running the old scene.
* `loadAnimatedTiles(...)` is the same private helper `load(...)`
  already calls; it repopulates `m_animatedTiles` from the map's
  animated tile definitions.
* `m_animClock` is reset to 0 on success — animated tiles
  restart from frame 0 after a reload, which is what you want
  during map tuning.

---

## 5. `include/Game/Player.h` — add `resetForRespawn()`

Open `G:\Dev\HBE\MegaX\include\Game\Player.h`.

Find the existing:

```cpp
        void refillHp() { m_hp = startHp; m_hurtFlashTimer = 0.0f; }
```

Add the new declaration **immediately below** `refillHp()`:

```cpp
        void refillHp() { m_hp = startHp; m_hurtFlashTimer = 0.0f; }

        // Item 11: called by GameLayer during a scene reload. Zeroes
        // every transient combat/animation field (velocities, shot
        // pending, shoot timers, recoil, fire cooldown, anim state) so
        // the player boots into idle. Does NOT change position (call
        // setPosition after), mode or helmet.
        void resetForRespawn();
```

---

## 6. `src/Game/Player.cpp` — implement `resetForRespawn()`

Open `G:\Dev\HBE\MegaX\src\Game\Player.cpp`.

Add the new function **immediately below** the existing
`Player::setPosition(...)` body (the one that ends with
`m_box.cy = y - feetToCenterOffset(m_box.h);` and closes with
`}`):

```cpp
    void Player::resetForRespawn() {
        // Full HP + i-frame + hurt flash reset (mirrors refillHp()).
        m_hp             = startHp;
        m_invulnTimer    = 0.0f;
        m_hurtFlashTimer = 0.0f;

        // Kinematics — kill all momentum.
        m_vx = 0.0f;
        m_vy = 0.0f;
        m_inX = 0.0f;
        m_inY = 0.0f;

        // Play-mode transient state.
        m_grounded  = false;
        m_crouching = false;
        m_coyote    = 0.0f;
        m_jumpBuf   = 0.0f;
        m_landTimer = 0.0f;
        m_landedThisFrame = false;
        m_groundTileId    = 0;

        // Latched input intents from the previous frame.
        m_jumpPressed = false;
        m_jumpHeld    = false;
        m_crouchHeld  = false;
        m_firePressed = false;
        m_fireHeld    = false;

        // Shooting subsystem — zero the recoil impulse and any pending
        // bullet so we never spawn a stale shot after teleporting.
        m_fireCooldown = 0.0f;
        m_shootTimer   = 0.0f;
        m_recoilVx     = 0.0f;
        m_shotPending  = false;
        m_shotX = 0.0f;
        m_shotY = 0.0f;
        m_shotDir = m_facing;

        // Force the anim state to reevaluate on the next update; -1 is the
        // sentinel value setAnimState uses for "no active state".
        m_animState = -1;
    }
```

Field-name sanity check (grep any of them in `Player.h` if in
doubt — every one is already declared as a `private:` member).

Two deliberate omissions:

* We do NOT touch `m_mode` — Ghost/Play is preserved across
  reload.
* We do NOT touch `m_helmet` / `m_activeSheet` — the helmet
  toggle is preserved across reload.

If you want a completely-fresh player on `F5` (mode + helmet
reset too), add `setMode(Mode::Play); setHelmet(true);` *at
the GameLayer callsite* — never in `resetForRespawn()` itself.
The helper is designed to be state-preserving; the GameLayer
policy layer decides what to preserve.

---

## 7. Sanity check — a compile at this point should still work

Item 11 doc 01 introduces no new callers of any of these new
functions. Effects, World and Player each get one extra method
that nobody references yet, so a build here should compile
clean (no unresolved externals, no unused-variable warnings —
warning level in MegaX is set to allow unused private methods).

Do NOT attempt a build if you've already started doc 02 —
the GameLayer changes assume all three helpers exist.

If it fails, common causes:

| Symptom | Fix |
|---|---|
| `error C2039 'clear': is not a member of 'MegaX::Effects'` in *Effects.cpp* | You edited the header but forgot to save. Save `Effects.h`, then rebuild. |
| `error C2065 'm_ps': undeclared identifier` in *Effects.cpp* | Field is spelled differently in your copy of `Effects.h` (older revision used `m_particles`). Match the actual name. |
| `error C2027 use of undefined type 'HBE::Renderer::ParticleSystem'` in *Effects.cpp* | You accidentally moved `#include "HBE/Renderer/ParticleSystem.h"` out of `Effects.cpp`. Put it back at the top of the .cpp — not in the .h. |
| `error C2039 'reload': is not a member of 'MegaX::World'` | Same as above — `World.h` change wasn't saved. |
| `error C2039 'resetForRespawn': is not a member of 'MegaX::Player'` | Same as above — `Player.h` change wasn't saved. |
| `error C2660 'World::load' does not take 4 arguments` in *World.cpp* | You accidentally removed a parameter from `load(...)`. Its signature is unchanged in Item 11. |
| `error C2065 'm_logicalMapPath': undeclared identifier` in *World.cpp* | The private field addition to `World.h` wasn't saved. |

---

Next: `02_gamelayer_reload_helpers.md` — extract
`spawnDemoEnemies()` out of `onAttach`, add `reloadScene(...)`
and the F5/F7 hotkeys.
