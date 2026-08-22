# 02 — GameLayer reload plumbing (`reloadScene`, F5, F7, spawn helper)

Doc 01 gave you the small building blocks. Doc 02 wires them
into `GameLayer` and adds the two hotkeys that don't need
FileWatcher (F5 = full reload, F7 = soft respawn).

**Do not skip the extraction step in §2.** The reload path
needs the exact same spawn logic `onAttach` uses. If you
duplicate it inline instead of extracting a helper, F7 and F5
will slowly drift apart during Item 12 tuning and you'll spend
an afternoon figuring out why the F7-respawned soldier has 5 HP
but the F5-respawned soldier has 3 HP.

---

## 1. `include/Game/GameLayer.h` — add fields + helper decls

Open `G:\Dev\HBE\MegaX\include\Game\GameLayer.h`.

### 1a. Add caches for spawn coordinates + tilemap path

Find the existing `bool m_showHitBoxes = false;` line (last
line before the class' closing brace). Add these fields
**immediately below** it (still inside the `private:` block):

```cpp
        bool m_showHitBoxes = false;

        // Item 11: cached scene-setup state used by reloadScene().
        std::string m_tileMapPath = "maps/level_01.json";
        float m_startX = 0.0f;
        float m_startY = 0.0f;
```

### 1b. Add the helper method declarations

Still in `GameLayer.h`, inside the existing `private:` block
(right above `buildSpritePipeline();`), add:

```cpp
        // Item 11: authored inline for now (no per-map spawn JSON yet).
        // Called from onAttach() AND reloadScene() so both paths land on
        // the same enemy composition.
        void spawnDemoEnemies();

        // Item 11: full scene reload. If alsoReloadMap == true, first
        // asks m_world to re-load level_01.json; otherwise keeps the
        // existing map and just recycles entities. Returns false and
        // leaves the previous scene running on any failure.
        bool reloadScene(bool alsoReloadMap);

        // Item 11: convenience -- clears bullets + enemy bullets + enemies
        // + effects. Does NOT touch the world, the player, or the camera.
        void clearTransientEntities();
```

*(`buildSpritePipeline()` and `drawHud(...)` stay right where
they are.)*

### 1c. Add the include for `FileWatcher`

The `FileWatcher` type is added in doc 03, but the header
change here is a single-line convenience. Add near the other
`#include "HBE/..."` lines at the top of `GameLayer.h`:

```cpp
#include "HBE/Core/FileWatcher.h"
```

And declare the watcher member alongside the other engine
members (right above `bool m_showHitBoxes = false;`):

```cpp
        HBE::Core::FileWatcher m_watcher{};
```

Doc 03 will populate the watcher; leaving it default-constructed
here is harmless (`poll(dt)` on an empty watcher is a no-op).

### 1d. Full expected private-block after doc 02 + doc 03 edits

For quick sanity check, the private section of `GameLayer.h`
should read roughly like:

```cpp
    private:
        void buildSpritePipeline();
        void drawHud(HBE::Renderer::Renderer2D& r2d);

        void spawnDemoEnemies();                // Item 11
        bool reloadScene(bool alsoReloadMap);   // Item 11
        void clearTransientEntities();          // Item 11

        HBE::Core::Application* m_app = nullptr;

        HBE::Renderer::Mesh* m_quadMesh = nullptr;
        HBE::Renderer::GLShader* m_spriteShader = nullptr;

        HBE::Renderer::CameraController m_camera{};
        Player m_player{};
        World m_world{};
        BulletManager m_bullets{};
        Effects m_effects{};
        EnemyManager m_enemies{};
        const HBE::Renderer::TileMapLayer* m_ground = nullptr;

        HBE::Renderer::DebugDraw2D m_debug{};
        HBE::Core::FileWatcher m_watcher{};             // Item 11

        bool m_showHitBoxes = false;

        std::string m_tileMapPath = "maps/level_01.json";  // Item 11
        float m_startX = 0.0f;                              // Item 11
        float m_startY = 0.0f;                              // Item 11
    };
```

`Difficulty m_difficulty = Difficulty::Difficult;` is
already declared in the `public:` block from Item 10 — leave
it there.

---

## 2. `src/Game/GameLayer.cpp` — extract `spawnDemoEnemies()`

Open `G:\Dev\HBE\MegaX\src\Game\GameLayer.cpp`.

### 2a. Replace the inline enemy-spawn block in `onAttach`

Find the current inline spawn block near the bottom of
`onAttach(...)`. It looks like this (from Item 10):

```cpp
        {
            constexpr float kTilePx = 32.0f;
            const float ex = startX + 5.0f * kTilePx;
            const float eGroundY = 130.0f;    // or your literal
            if (Enemy* e = m_enemies.spawn(ex, eGroundY, -1)) {
                e->startHp = 3;
                e->setPatrolPath(ex - 3.0f * kTilePx, ex + 3.0f * kTilePx, 1.0f);
                e->snapshotBaseStats();
                e->applyDifficulty(m_enemies.profile());

                e->spawn(ex, eGroundY, -1);
            }
        }
```

Replace the entire block (braces included) with a single call
to the new helper:

```cpp
        spawnDemoEnemies();
```

### 2b. Replace the inline start-position literals

A few lines above the spawn block you have:

```cpp
        const float startX = (m_world.pixelWidth() * 0.5f) - 128.0f; // X
        const float startY = 130.0f; // Y
        m_player.setPosition(startX, startY);
        m_camera.snapTo(startX, startY);
        app.gl().setCamera(m_camera.camera());
```

Cache those numbers on the layer so `reloadScene()` can reuse
them. Replace the block with:

```cpp
        m_startX = (m_world.pixelWidth() * 0.5f) - 128.0f;
        m_startY = 130.0f;
        m_player.setPosition(m_startX, m_startY);
        m_camera.snapTo(m_startX, m_startY);
        app.gl().setCamera(m_camera.camera());
```

### 2c. Point `onAttach` at the cached tilemap path

Find the `m_world.load(...)` call near the top of `onAttach`:

```cpp
        if (!m_world.load(app.renderer2D(), app.resources(),
            m_spriteShader, m_quadMesh, "maps/level_01.json")) {
```

Replace the literal `"maps/level_01.json"` with the cached
member so there's a single source of truth for the map path:

```cpp
        if (!m_world.load(app.renderer2D(), app.resources(),
            m_spriteShader, m_quadMesh, m_tileMapPath)) {
```

(`m_tileMapPath` defaults to `"maps/level_01.json"` from the
header change in §1a, so behavior is unchanged.)

### 2d. Define `spawnDemoEnemies()` at the bottom of the .cpp

Add the new function **just above** the closing `}` of
`namespace MegaX { ... }`, alongside `buildSpritePipeline()`
and `drawHud(...)`:

```cpp
    void GameLayer::spawnDemoEnemies() {
        constexpr float kTilePx = 32.0f;
        const float ex = m_startX + 5.0f * kTilePx;
        const float eGroundY = 130.0f;   // matches m_startY -- the demo
                                         // patrol runs on the same ground row
                                         // as the player spawn.
        if (Enemy* e = m_enemies.spawn(ex, eGroundY, -1)) {
            e->startHp = 3;
            e->setPatrolPath(ex - 3.0f * kTilePx, ex + 3.0f * kTilePx, 1.0f);
            e->snapshotBaseStats();
            e->applyDifficulty(m_enemies.profile());
            e->spawn(ex, eGroundY, -1);
        }
    }
```

Any per-map / per-enemy tuning you had in the old inline block
goes here — this is the *single* place enemies are authored in
Item 11.

### 2e. Sanity: `onAttach` should now end with the log line, unchanged

The last two lines of `onAttach` should still be:

```cpp
        spawnDemoEnemies();

        LogInfo("MegaX GameLayer attached (Play mode; press G for Ghost).");
    }
```

---

## 3. `src/Game/GameLayer.cpp` — `clearTransientEntities` + `reloadScene`

### 3a. `clearTransientEntities()`

Add this helper **immediately above** `spawnDemoEnemies()`:

```cpp
    void GameLayer::clearTransientEntities() {
        // Bullets first so nothing is holding a raw pointer to an enemy
        // when we clear the enemy vector below.
        m_bullets.clear();
        m_enemies.enemyBullets().clear();

        // Enemies. Their manager keeps its resource/shader/mesh refs and
        // its current difficulty profile; only the vector of live enemies
        // is dropped.
        m_enemies.clear();

        // Effects: particles + casings + walk-dust cadence.
        m_effects.clear();
    }
```

### 3b. `reloadScene(bool alsoReloadMap)`

Add this immediately below `clearTransientEntities()`:

```cpp
    bool GameLayer::reloadScene(bool alsoReloadMap) {
        if (!m_app) {
            LogError("MegaX: reloadScene called before onAttach; ignoring.");
            return false;
        }

        const auto tStart = std::chrono::steady_clock::now();

        // Preserved state (see doc 00 "Preserved state" table).
        const Player::Mode preservedMode = m_player.mode();
        const bool         preservedHelm = m_player.hasHelmet();

        // Order matters: reload the world FIRST. If it fails we early-out
        // BEFORE clearing entities, so the previous scene keeps running.
        if (alsoReloadMap) {
            if (!m_world.reload(m_app->renderer2D(), m_app->resources())) {
                LogError("MegaX: scene reload FAILED -- tilemap load error (see previous log).");
                LogWarn("MegaX: keeping previous scene state.");
                return false;
            }
            m_ground = m_world.map().findLayer("Ground");
            if (!m_ground) {
                LogError("MegaX: scene reload FAILED -- reloaded map has no 'Ground' layer.");
                LogWarn("MegaX: keeping previous scene state.");
                return false;
            }
        }

        // World is valid past this point. Safe to nuke transient state.
        clearTransientEntities();

        // Rewire collision context (the TileMap object address may have
        // moved during World::reload; TileMap holds vectors that get
        // reassigned via std::move).
        m_player.setCollision(&m_world.map(), m_ground);

        // Player: fresh combat/anim state, then teleport, then restore
        // Ghost/Play and helmet from the pre-reload snapshot.
        m_player.resetForRespawn();
        m_player.setPosition(m_startX, m_startY);
        m_player.setMode(preservedMode);
        m_player.setHelmet(preservedHelm);

        // Camera: snap (do NOT follow-lerp). The controller keeps its
        // followResponse and zoom.
        m_camera.snapTo(m_startX, m_startY);
        m_app->gl().setCamera(m_camera.camera());

        // Enemy manager: player ref + collision + difficulty are already
        // set from onAttach; setDifficulty is safe to call again and also
        // re-computes the profile from the current enum.
        m_enemies.setPlayerRef(&m_player);
        m_enemies.setCollision(&m_world.map(), m_ground);
        m_enemies.setDifficulty(m_difficulty);

        // Authored spawn (same code path as onAttach uses).
        spawnDemoEnemies();

        const auto tEnd = std::chrono::steady_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

        LogInfo("MegaX: scene reloaded ("
            + std::string(alsoReloadMap ? "map + entities" : "entities only")
            + ") in " + std::to_string(ms) + " ms.");
        return true;
    }
```

### 3c. Required includes at the top of `GameLayer.cpp`

Add these two headers alongside the existing includes at the
top of the file (they may already be present transitively, but
be explicit):

```cpp
#include "HBE/Core/Log.h"    // (already there)

#include <chrono>            // NEW: for the reload-time metric
```

`LogWarn` (the engine spells it `LogWarn`, not `LogWarning`)
is declared in the same `HBE/Core/Log.h` header as
`LogInfo`/`LogError`, so no additional include is needed. If
you see `error C3861 'LogWarning': identifier not found` you
typed `LogWarning` — swap it for `LogWarn`.

---

## 4. `src/Game/GameLayer.cpp` — F5 / F7 hotkeys in `onUpdate`

Open `GameLayer::onUpdate(float dt)`.

Find the existing R-key handler that refills HP:

```cpp
        if (Input::IsKeyPressed(SDL_SCANCODE_R)) {
            m_player.refillHp();
            LogInfo("HP refilled");
        }
```

**Immediately below** that block, add the three new hotkeys:

```cpp
        if (Input::IsKeyPressed(SDL_SCANCODE_F5)) {
            LogInfo("MegaX: scene reload requested (F5).");
            reloadScene(/*alsoReloadMap=*/true);
        }
        if (Input::IsKeyPressed(SDL_SCANCODE_F7)) {
            LogInfo("MegaX: soft respawn requested (F7).");
            // F7 = same as F5 but skip the tilemap reload. Player HP,
            // position, mode all get reset the same way.
            reloadScene(/*alsoReloadMap=*/false);
        }
        // F6 is handled in doc 03 (shader hot reload) -- do not add here
        // yet, or you'll have to remove it in doc 03.
```

**Placement matters.** These checks must sit *above* the
`m_world.update(dt);` line — after the reload the world/enemies
just got rebuilt from scratch, and continuing straight into the
per-frame update pass is correct (the reload leaves everything in a
valid post-`init` state). If you put the check *after*
`m_enemies.update(dt);`, the just-spawned enemy runs its first
frame of AI with stale delta-time carryover from the pre-reload
frame — usually invisible, occasionally weird.

The existing "check all the intents" block (`ix`, `iy`,
`jumpPressed`, etc.) at the very top of `onUpdate` stays
exactly where it is; reload hotkeys and gameplay input coexist
happily in the same frame (worst case: you press F5 mid-jump and
the queued `jumpPressed` intent is discarded by
`resetForRespawn()` — which is the correct behavior).

---

## 5. Sanity check at end of doc 02

At this point:

* You can build and run.
* Pressing **F5** should teleport you back to spawn with a
  fresh soldier and fresh HP. The log should show
  `[INFO ] MegaX: scene reloaded (map + entities) in X ms.`
* Pressing **F7** should do the same *without* teleporting the
  player. Position stays. HP resets. Wait — does HP reset?
  Yes, because `reloadScene(false)` still calls
  `resetForRespawn()` and `setPosition(m_startX, m_startY)`.
  That's technically a teleport too.

  **If you want F7 to leave the player exactly where they are:**
  factor the "teleport + refresh player" block out of
  `reloadScene` into a small internal helper and skip it in the
  `!alsoReloadMap` branch. The doc 00 "done" checklist matches
  the always-teleport behavior — keep that as the default
  unless you deliberately choose otherwise. The doc 00 wording
  is intentional: F7 = "reset the fight, keep exploring", which
  is *close enough* to "keep exploring" for our tuning use case.

* Difficulty pill should stay whatever color it was pre-reload
  (F1/F2/F3 not required to reactivate).
* Ghost mode should stay Ghost if you were ghosting.

The tilemap auto-reload on file change is doc 03.

---

Next: `03_filewatcher_and_shader.md` — wire
`HBE::Core::FileWatcher` for the tilemap JSON + sprite shader
files, and add the F6 shader hot reload hotkey + helper.
