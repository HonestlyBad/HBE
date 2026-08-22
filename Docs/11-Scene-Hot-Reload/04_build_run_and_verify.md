# 04 — Build, run, verify, tune

You've now touched every file listed in doc 00's table. This
final doc walks the build, the manual verify checklist for
every reload path, tuning knobs, and a troubleshooting matrix
keyed off the most likely bugs.

---

## 1. Project file — no new sources this time

Item 11 introduces **zero** new `.h` / `.cpp` files. Every edit
lives in files that are already listed in `MegaX.vcxproj`:

* `GameLayer.h` / `GameLayer.cpp`
* `Effects.h` / `Effects.cpp`
* `World.h` / `World.cpp`
* `Player.h` / `Player.cpp`

**No `MegaX.vcxproj` edits required.** Skip this step (unlike
Item 10, which added `EnemyBullet.h`/`.cpp`).

If you *want* to double-check nothing got dropped from the
project during editing:

```
grep "GameLayer.cpp" G:\Dev\HBE\MegaX\MegaX.vcxproj
grep "Effects.cpp"   G:\Dev\HBE\MegaX\MegaX.vcxproj
grep "World.cpp"     G:\Dev\HBE\MegaX\MegaX.vcxproj
grep "Player.cpp"    G:\Dev\HBE\MegaX\MegaX.vcxproj
```

All four should return exactly one `<ClCompile Include="..." />`
line each.

---

## 2. Build

From a `vcvars64.bat`-initialized shell:

```
msbuild HonestlyBadEngine.slnx /p:Configuration=Debug /p:Platform=x64 /t:MegaX /m /nologo /v:m
```

Expected new lines (touched TUs only recompile):

```
  Player.cpp
  Effects.cpp
  World.cpp
  GameLayer.cpp
  MegaX.vcxproj -> G:\Dev\HBE\MegaX\bin\x64\Debug\MegaX.exe
  MegaX: copying runtime deps to G:\Dev\HBE\MegaX\bin\x64\Debug\
```

Only pre-existing warnings should appear (`C4099 TileMap` from
Item 07, `C4244 InputMap` from Core, `C4305` in `Effects.cpp`
from Item 07). Any new C-level error → jump to the
troubleshooting matrix at the bottom.

---

## 3. Run

```
G:\Dev\HBE\MegaX\bin\x64\Debug\MegaX.exe
```

Expected startup log lines (in order):

```
[INFO ] World loaded 'maps/level_01.json' (3 tilesets, N layers, ... animated tiles, ... instances).
[INFO ] MegaX GameLayer attached (Play mode; press G for Ghost).
[INFO ] MegaX GameLayer: scene watches active (maps/level_01.json, sprite shader).
```

The HUD reads 5 HP pips (top-left), yellow "Difficult" pill
(top-right). Robot Soldier is patrolling ~5 tiles to the right
of spawn. Everything from Item 10 is unchanged pre-F5.

---

## 4. Verify checklist

Group A is unchanged Item 10 regression — those should still
work identically. Groups B–E are Item 11 proper.

### Group A — Item 10 regression (no reload keys pressed)

* [ ] F1/F2/F3 still swap difficulty. HUD pill retints.
* [ ] R still refills HP without touching anything else.
* [ ] Damage / i-frames / knockback still play correctly on
      enemy bullet hit.
* [ ] Ghost mode (G) still makes enemy bullets pass through
      the player.
* [ ] Killing the soldier (3 shots) still fades the sprite.

### Group B — F5 full reload

* [ ] Press F5 while walking mid-map, not in combat.
      * Log: `MegaX: scene reload requested (F5).` +
        `MegaX: scene reloaded (map + entities) in X ms.`
      * Player teleports to `m_startX, m_startY`.
      * Camera SNAPS (no follow-lerp fling — this is
        `m_camera.snapTo`, not `setFollowTarget`).
      * HP pills = full.
      * Difficulty pill = same color as before.
      * Soldier is back at his post, walking left-right.
* [ ] Press F5 mid-fight (soldier is firing, bullets in
      flight, you have 2 HP). Same as above:
      * All enemy bullets removed instantly (no red streaks
        left drifting).
      * All your bullets removed too.
      * Particles, casings, walk-dust: gone.
* [ ] Press F5 while in Ghost mode. You reappear at spawn
      *still in Ghost mode* (no gravity applied — verify by
      not pressing S/SPACE for 2 seconds and confirming you
      don't fall).
* [ ] Press F5 with helmet off (H). After the reload the
      helmet is still off.
* [ ] Press F5 with hitboxes on (B). After the reload the
      hitbox overlay is still on.
* [ ] Press F5 while shooting (E held). No bullets get spawned
      by the "was shooting on last frame" latched intent. This
      is `resetForRespawn()` clearing `m_shotPending` +
      `m_firePressed` (doc 01 §6).
* [ ] Press F5 while airborne mid-jump. You land at spawn
      standing, not falling. This is `resetForRespawn()`
      zeroing `m_vy` and `m_grounded=false` → the very next
      collision resolve pass grounds you cleanly.

### Group C — F7 soft respawn

* [ ] Press F7 not-in-combat. Log:
      `MegaX: soft respawn requested (F7).` +
      `MegaX: scene reloaded (entities only) in X ms.`
* [ ] Player still teleports back to spawn (this is the
      documented behavior — see doc 02 §5 caveat). If you
      picked the "true soft respawn" option, F7 does NOT
      teleport; verify that instead.
* [ ] Soldier is back, patrolling.
* [ ] Tilemap did NOT reload (edit the JSON, save, don't
      trigger the watcher, press F7 — the map is unchanged).

### Group D — F6 shader hot reload

* [ ] Press F6 with no source edits. Log:
      `MegaX: sprite shader hot reload requested (F6).` +
      `Shader reloaded: sprite`.
* [ ] Everything on screen looks identical (correct — no
      shader edit was applied).
* [ ] Open `MegaX\assets\shaders\sprite.frag`, change the
      final `FragColor` line to `FragColor.rgb *= 0.5;`. Save.
      Press F6.
      * Everything darkens on the next draw.
      * Revert the change, save, F6 → everything brightens
        back.
* [ ] Introduce a GLSL syntax error, save, F6. Log:
      `Shader reload FAILED: sprite (see previous log for GLSL error).`
      The scene keeps rendering with the previously-good
      shader.

### Group E — FileWatcher auto-triggers

* [ ] Edit + save `MegaX\assets\maps\level_01.json`. Within
      ~0.25s the console prints
      `MegaX: tilemap file changed on disk -> scene reload.`
      followed by the scene reload success log.
* [ ] Edit + save `MegaX\assets\shaders\sprite.frag`. Within
      ~0.25s the console prints
      `MegaX: shader file changed on disk -> hot reload.`
      followed by `Shader reloaded: sprite`.
* [ ] Save the same file twice within 0.1s (double-save). The
      watcher fires **once** (its 0.25s debounce absorbs the
      duplicate).
* [ ] Break the tilemap JSON (delete a `}`), save.
      * Log: `[ERROR] World::reload: failed to load ...`
        + `MegaX: scene reload FAILED -- tilemap load error`
        + `MegaX: keeping previous scene state.`
      * The game keeps running with the OLD map. No crash.
      * Fix the JSON. Watcher fires again on save; scene
        reloads cleanly.

If Groups A–E all tick, Item 11 is done.

---

## 5. Tuning table — the polish knobs

Everything here lives in `GameLayer::setupHotReloadWatches()`
or in the reload flow itself. Edit the number and re-build.

| Symptom | Field | Direction |
|---|---|---|
| Watcher fires too often (feels laggy while typing in Aseprite) | `FileWatcher::Options.pollIntervalSeconds` | `0.20 → 0.35` |
| Watcher misses fast edits (save → save → save quickly) | `FileWatcher::Options.debounceSeconds` | `0.25 → 0.15` |
| Reload feels visually harsh (camera whip) | swap `m_camera.snapTo(...)` for `m_camera.setFollowTarget(...)` + one frame of `update(0)` in `reloadScene` | subtle |
| F5 spam crashes eventually | see troubleshooting §6 — you likely have a leftover raw pointer somewhere | fix, don't tune |
| F7 should NOT teleport the player | See doc 02 §5. Split `reloadScene` into `reloadWorldAndEntities` vs. `reloadEntitiesOnly`, and only the World+player path calls `setPosition`. | design |
| Reload restores you to a different spot than spawn | `m_startX` / `m_startY` are computed in `onAttach` and cached. If you resize the map you need to relaunch OR re-derive them at the top of `reloadScene` (uncomment the `m_startX = (m_world.pixelWidth() ...)` line and mirror doc 02 §2b there). | later |
| Ghost mode always resets to Play on F5 | You forgot to save `preservedMode` / restore with `setMode(preservedMode)`. See doc 02 §3b. | fix |
| Helmet always resets on F5 | Same as above — `preservedHelm` / `setHelmet(preservedHelm)`. | fix |
| Difficulty pill goes yellow on F5 even when it was red | `m_difficulty` isn't being read post-reload. Confirm `m_enemies.setDifficulty(m_difficulty)` in `reloadScene`. | fix |
| Enemy respawns with 5 HP instead of 3 | You forgot to move the per-enemy override (`e->startHp = 3`) into `spawnDemoEnemies()`. Copy-paste from doc 02 §2d. | fix |

Non-goals — don't tune these here (they belong to future
items):

* Per-map difficulty override → later map integration
* Data-driven enemy spawn list → later data pipeline
* Save/restore player position across reload → later polish

---

## 6. Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Build: `error C3861 'LogWarning': identifier not found` | Engine spells the API `LogWarn`. | Replace `LogWarning(...)` with `LogWarn(...)`. |
| Build: `error C2039 'clear': is not a member of 'MegaX::Effects'` | You forgot to save `Effects.h` after adding the declaration. | Save `Effects.h`, rebuild. |
| Build: `error C2065 'm_ps': undeclared identifier` in `Effects.cpp` | The particle-system field is called `m_particles` in your local revision, not `m_ps`. | Grep `Effects.h` for the actual name, use that in `Effects::clear()`. |
| Build: `error C2027 use of undefined type 'HBE::Renderer::ParticleSystem'` in `Effects.cpp` | Someone deleted the `#include "HBE/Renderer/ParticleSystem.h"` from the top of `Effects.cpp`. | Restore it. That include lives in the .cpp, never the .h (see `Effects.h` header comment). |
| Build: `error C2039 'reload': is not a member of 'MegaX::World'` | `World.h` change wasn't saved. | Save, rebuild. |
| Build: `error C2065 'm_logicalMapPath': undeclared identifier` in `World.cpp` | The private field addition in `World.h` (doc 01 §3b) wasn't saved. | Save, rebuild. |
| Build: `error C2039 'resetForRespawn': is not a member of 'MegaX::Player'` | `Player.h` change wasn't saved. | Save, rebuild. |
| Build: `error C2039 'FileWatcher': is not a member of 'HBE::Core'` | Missing `#include "HBE/Core/FileWatcher.h"` in `GameLayer.h`. | Add it (doc 02 §1c). |
| Build: `error C2039 'poll': is not a member of ...` | You typed `m_watcher.poll();` — the engine watcher expects `poll(dt)`. | Add the `dt` argument. |
| Runtime: F5 does nothing, no log line | You added the F5 handler outside `onUpdate`. | Grep `SDL_SCANCODE_F5` — should be one and only one, inside `onUpdate`. |
| Runtime: F5 crashes (`Access violation reading location ...`) | You're calling `reloadScene(...)` mid-iteration of `m_bullets.bullets()` or `m_enemies.enemies()`. | Move the F5 check to the *top* of `onUpdate`, per doc 02 §4. |
| Runtime: F5 works but the map doesn't visually update after a JSON edit | The `<MegaXAsset>` build step copies assets on build. Editing the source file doesn't change the runtime copy. | Either edit the runtime copy in `MegaX\bin\x64\Debug\assets\maps\level_01.json`, or watch the runtime path (`AssetPaths::Resolve` returns the runtime path — that's what we already watch), so **make sure you're editing the runtime copy**. Alternately, rebuild after every source edit. |
| Runtime: watcher fires on shader save but not on JSON save | `FileWatcher` polls file *modification time*. Some map editors (Aseprite, Tiled) write to a temp file and rename. That still updates the mtime, so it should work. If it doesn't, `unwatchFile()` + `watchFile()` again after the first failed fire. |
| Runtime: F6 says "reloaded" but nothing changed | Same as above — you're editing the source shader, not the runtime one. `AssetPaths::Resolve` returns the runtime path. Save into `MegaX\bin\x64\Debug\assets\shaders\`. Or rebuild after each edit. |
| Runtime: shader reload says FAILED | GLSL syntax error. Look at the log line *before* the "FAILED" line for the compiler output. Fix the syntax, save; watcher will retry. |
| Runtime: player falls through the ground after F5 | `m_ground = m_world.map().findLayer("Ground");` was skipped after the reload. Doc 02 §3b explicitly re-runs this — verify it's there. |
| Runtime: enemies appear in the wrong Y position after F5 | The reload used the *pre-reload* `m_ground` (a stale pointer into the old TileMap). Same fix — re-run `findLayer("Ground")` after `World::reload`. |
| Runtime: F5 succeeds but difficulty pill goes yellow | You either forgot `m_enemies.setDifficulty(m_difficulty);` in `reloadScene`, or the enemy's applied profile is being overwritten by a post-`spawn` `applyDifficulty(MakeProfile(Difficulty::Difficult))` somewhere. Grep `MakeProfile(` for anything besides `EnemyManager.cpp` — should be nothing. |
| Runtime: F5 doubles enemy count each press | You forgot `m_enemies.clear();` — probably typo'd `m_enemies.enemyBullets().clear();` twice instead. | Verify `clearTransientEntities()` calls `m_enemies.clear()` exactly once and `m_enemies.enemyBullets().clear()` exactly once. |
| Runtime: F5 works but F7 crashes with "world not loaded" | You added a `m_world.someAccessor()` inside `reloadScene(false)` path where `alsoReloadMap=false` skipped a piece of setup that F5 does. Re-read doc 02 §3b — the only piece skipped on F7 is the `World::reload(...)` call and the `m_ground = ...` re-lookup. Everything else runs both paths. |
| Runtime: pressing F5 during the auto-triggered reload prints a warning about a reload while a reload is in progress | Not a real bug (the second reload just replaces the first before it finishes). Only worth chasing if it actually crashes. |
| Runtime: watcher fires on every save but the reload log says "in 900 ms" or more | You're reloading a very large map. Expected. Not an Item 11 bug. |
| Runtime: watcher fires on every save with **no** reload log | The watcher callback is running but the reload is early-returning. Look for the `[ERROR]` line just before the `keeping previous scene state` warning. |
| Runtime: FileWatcher never fires at all | `m_watcher.poll(dt);` isn't at the top of `onUpdate`, or `setupHotReloadWatches()` never ran. Grep both, confirm they're present. |

---

## 7. What Item 12 will add (context for the tuning workflow)

Item 12 layers enemy particle FX on top of Item 10's combat
loop (muzzle flashes for enemies, casings, hit sparks, death
explosion, blood splatter on the player). During Item 12 you
will lean heavily on this item's F5:

```
tweak sparks color / lifetime  →  F5  →  fire on the soldier  →  see the change in 2 seconds
```

That's the whole point of Item 11 existing.

---

## 8. Wrap-up

If Groups A–E all tick and you can iterate on
`level_01.json` + `sprite.frag` without alt-tabbing to
`msbuild`, Item 11 is complete. The Sandbox-style hot-reload
workflow is now available in MegaX, purely game-side, zero
engine edits.

Have fun. From here on out, tuning is the fun part.
