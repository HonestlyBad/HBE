# 11 — Scene Reload & Hot Reload (overview)

Item 10 finished the combat loop: enemies fire, the player has
HP, and three difficulty modes flavor the fight. Playtesting
that combat loop right now is painful — every time you tweak
`Enemy::chaseSpeed`, the enemy patrol range, a tile in
`level_01.json`, or `sprite.frag`, you have to close MegaX,
recompile, and relaunch the app. That kills the "sit down, tune,
play again in 2 seconds" flow the Sandbox already has.

Item 11 wires **scene reload** (F5) and a couple of surgical
**hot-reload watches** (tilemap JSON + sprite shader) into
`GameLayer`, using the engine primitives already available:

* `HBE::Core::FileWatcher` (the same polling watcher Sandbox
  uses).
* `HBE::Renderer::ResourceCache::reloadShader(...)` — engine
  API added long before Item 11.
* `HBE::Renderer::TileMapLoader::loadFromJsonFile(...)` — the
  same loader `World::load` already calls.

Zero engine files change. Every edit lives under `MegaX/`.

---

## Goals

* **F5 — full scene reload**: from any game state (Play or
  Ghost, mid-fight or idle), pressing F5 rebuilds the scene
  in-place:
  * Reloads the tilemap JSON from disk (so map-maker edits show
    up without relaunching).
  * Clears all live bullets, enemy bullets, particle effects,
    casings, and enemies.
  * Resets the player: position -> spawn point, velocity -> 0,
    HP -> full, `m_invulnTimer`/`m_hurtFlashTimer` -> 0,
    `m_landTimer`/`m_shootTimer`/`m_fireCooldown`/`m_recoilVx`
    -> 0, animation state -> idle. Ghost/Play **mode** is
    preserved (so a tuning session doesn't force you back into
    Play every reload).
  * Snaps the camera to the fresh player position (no lerp
    fling).
  * Re-runs the enemy spawn block, then re-applies the current
    difficulty profile.
  * Preserves current helmet toggle, current difficulty pip.
* **F6 — sprite shader hot reload**: recompiles `sprite.vert` +
  `sprite.frag` from disk in place, using
  `ResourceCache::reloadShader("sprite")`.
* **F7 — soft respawn**: kills all live enemies + bullets and
  re-runs the enemy spawn block without touching the tilemap or
  the player. Useful for "reset the fight, keep exploring".
* **FileWatcher auto-triggers**:
  * `maps/level_01.json` -> auto full-reload (map + entities).
  * `shaders/sprite.vert` / `shaders/sprite.frag` -> auto
    shader hot reload.
* Difficulty (`m_difficulty`) survives every reload path.
* Log lines emitted so the console tells you *why* a reload
  happened (manual F5 vs. FileWatcher, success vs. failure).

## Non-goals (deferred)

| Behavior | Deferred to |
|---|---|
| Live tileset PNG hot reload (`Tileset1H.png`, etc.) | Item 12+ / asset polish |
| Editor overlay for authoring maps in-app | HBMapMaker item |
| Save/restore player position across reloads (not just spawn) | Later polish |
| Data-driven enemy spawn list (JSON) so map edits change enemies too | Later data pipeline |
| Difficulty persistence to disk | Later polish |
| Level select overlay | Later polish |
| Frame-by-frame stepper / rewind | Not planned |
| Full ECS scene serializer (Sandbox has one; MegaX doesn't need it yet) | Never (MegaX is not ECS-based) |

Item 11 is purely lifecycle plumbing. Nothing gameplay-facing
changes. If the enemy is behaving differently after F5, that's
a bug, not a feature.

---

## What "done" looks like

You'll know Item 11 is finished when:

1. Fresh launch. HUD shows 5 HP pips, yellow "Difficult" pill,
   Robot Soldier at his post. Log line:

   ```
   [INFO ] MegaX GameLayer attached (Play mode; press G for Ghost).
   [INFO ] MegaX GameLayer: scene watches active (maps/level_01.json, sprite shader).
   ```

2. Walk into the soldier's cone, take 2 hits. Press **F5**.
   Log line:

   ```
   [INFO ] MegaX: scene reload requested (F5).
   [INFO ] MegaX: scene reloaded (map + entities) in 0.0xxs.
   ```

   * Player teleports back to spawn, HP full, camera snaps.
   * The soldier is back at his post, walking his patrol.
   * All bullets in flight are gone. No particle streaks left
     over from before the reload.
   * Difficulty pill is still yellow. Helmet is still whatever
     you had it set to. Ghost/Play mode preserved.

3. Toggle Ghost (G). Press **F5** again. You reload back to
   spawn *still in Ghost mode* (position reset, mode
   preserved).

4. Open `MegaX\assets\maps\level_01.json`, change a Ground tile
   somewhere in the middle of the map, save. Within ~0.25s
   MegaX shows the change without you pressing anything. Log:

   ```
   [INFO ] MegaX: tilemap file changed on disk -> scene reload.
   [INFO ] MegaX: scene reloaded (map + entities) in 0.0xxs.
   ```

5. Open `MegaX\assets\shaders\sprite.frag`, tweak
   `FragColor.rgb *= 0.5;`. Save. Everything on screen visibly
   dims within ~0.25s:

   ```
   [INFO ] MegaX: shader file changed on disk -> hot reload.
   [INFO ] Shader reloaded: sprite
   ```

6. Press **F6** — same shader-reload effect, on demand:

   ```
   [INFO ] MegaX: sprite shader hot reload requested (F6).
   [INFO ] Shader reloaded: sprite
   ```

7. Kill the soldier (3 shots). Press **F7** — soldier
   respawns instantly, patrolling from his post. No map
   reload, no player teleport, no HP change. Log:

   ```
   [INFO ] MegaX: soft respawn requested (F7).
   [INFO ] MegaX: enemies respawned (1 alive).
   ```

8. Break the tilemap JSON on purpose (invalid JSON: delete a
   `}`). Save. FileWatcher fires; the reload fails cleanly and
   the OLD scene keeps running:

   ```
   [ERROR] MegaX: scene reload FAILED — tilemap load error: ...
   [WARN ] MegaX: keeping previous scene state.
   ```

   Fix the JSON. Watcher fires again; scene reloads. No app
   crash at any point.

## Files touched (all game-side)

| File | Item 10 | Item 11 |
|---|---|---|
| `include/Game/GameLayer.h` | manager + HP + difficulty fields | + `FileWatcher m_watcher`, path caches, `spawnDemoEnemies()`, `resetPlayerToSpawn()`, `reloadScene(bool alsoReloadMap)`, `hotReloadShader()`, `startX/startY` cache |
| `src/Game/GameLayer.cpp` | senses + damage + HUD | + `reloadScene`, extracted spawn/setup, F5/F6/F7 hotkeys, FileWatcher setup + poll |
| `include/Game/Effects.h` | particle + casing sim | + `void clear();` |
| `src/Game/Effects.cpp` | particle + casing sim | + `Effects::clear()` implementation |
| `include/World/World.h` | load + render + animated tiles | + `bool reload(Renderer2D&, ResourceCache&);` re-loads the last path in place |
| `src/World/World.cpp` | load + animated tiles | + `World::reload()`, cache last logical path |
| `include/Game/Player.h` | movement + HP + i-frames | + `void resetForRespawn();` — zeroes velocities, timers, refills HP, preserves mode/helmet |
| `src/Game/Player.cpp` | movement + shooting + damage | + `Player::resetForRespawn()` implementation |
| `MegaX.vcxproj` / `.filters` | — | *no new source files* — everything piggy-backs on files that already ship |

Zero engine files. No new `.cpp` / `.h` files to add to the
project (unlike Item 10 which added `EnemyBullet`).

---

## Design shape (short version)

### Reload flow

`GameLayer::reloadScene(bool alsoReloadMap)` runs in this
order. Do NOT reorder — the enemy spawn block depends on the
world being valid and the player's collision context being set
first.

```
0. Cache: mode = m_player.mode(); helmet = m_player.hasHelmet();
1. m_bullets.clear();
2. m_enemies.enemyBullets().clear();
3. m_enemies.clear();
4. m_effects.clear();        // NEW helper (doc 01)
5. if (alsoReloadMap):
       ok = m_world.reload(app.renderer2D(), app.resources());
       if (!ok) { log error + keep old world state + return false; }
       m_ground = m_world.map().findLayer("Ground");
6. m_player.setCollision(&m_world.map(), m_ground);
7. m_player.resetForRespawn();
   m_player.setPosition(m_startX, m_startY);
   m_player.setMode(mode);
   m_player.setHelmet(helmet);
8. m_camera.snapTo(m_startX, m_startY);
   app.gl().setCamera(m_camera.camera());
9. m_enemies.setPlayerRef(&m_player);
   m_enemies.setCollision(&m_world.map(), m_ground);
   m_enemies.setDifficulty(m_difficulty);
   spawnDemoEnemies();       // NEW helper (doc 02)
10. LogInfo(...);
```

### FileWatcher wiring

The engine's `HBE::Core::FileWatcher` is polled every frame in
`GameLayer::onUpdate` with `m_watcher.poll(dt)`. The watcher
owns its own debounce (0.25s) so we don't need our own.

```
m_watcher.watchFile(AssetPaths::Resolve(m_tileMapPath),
    [this](const std::string&) {
        LogInfo("MegaX: tilemap file changed on disk -> scene reload.");
        reloadScene(true);
    });
m_watcher.watchFile(AssetPaths::Resolve(m_spriteVsPath),
    [this](const std::string&) {
        LogInfo("MegaX: shader file changed on disk -> hot reload.");
        hotReloadShader();
    });
m_watcher.watchFile(AssetPaths::Resolve(m_spriteFsPath),
    [this](const std::string&) {
        LogInfo("MegaX: shader file changed on disk -> hot reload.");
        hotReloadShader();
    });
```

### Preserved state across reload

State that survives a reload (whether via F5, F7 or an
auto-triggered watch fire):

| Field | Where it lives | Why preserved |
|---|---|---|
| `m_difficulty` | `GameLayer` | Cached in the layer itself; not touched on reload. |
| `m_player.mode()` | `Player` | Cached before reload, restored after. |
| `m_player.hasHelmet()` | `Player` | Cached before reload, restored after. |
| `m_showHitBoxes` | `GameLayer` | Not touched on reload. |

State that does **not** survive a reload:

| Field | Reset value | Why |
|---|---|---|
| `m_player.hp()` | `startHp` | Fresh HP is the whole point of a reload. |
| `m_player.velX/velY` | 0 | You're teleporting — kill momentum. |
| Player i-frame / hurt flash timers | 0 | Fresh player. |
| `m_bullets` | empty | Old shots would look weird mid-air after teleport. |
| Enemy bullets | empty | Same. |
| `m_enemies` | empty, then respawned via `spawnDemoEnemies()` | The demo Robot Soldier is authored inline for now (see doc 02). |
| `m_effects` particles + casings | empty | Old dust puff behind a teleport is a bug. |
| Camera position | snapped to `startX, startY` | No follow-lerp during teleport. |

### Failure policy

If `m_world.reload(...)` fails (bad JSON, missing tileset PNG,
whatever), we log the error and `return false;` from
`reloadScene`. The **old** map is still in memory
(`World::reload` restores its previous state on failure — see
doc 01 §5). Bullets/enemies/effects are also NOT cleared in
that path (we early-out before touching them), so a bad
FileWatcher fire is completely harmless: the app keeps running
with the last known good state.

---

## Golden rules (read before touching code)

1. **No engine edits.** Same rule as items 09 and 10. Every
   file lives under `G:\Dev\HBE\MegaX\`. If you catch yourself
   opening a file in `G:\Dev\HBE\HBE.Core\` or
   `G:\Dev\HBE\HBE.Renderer.GL\`, stop and re-scope.
2. **Enemy / Effects header hygiene stays.** Do NOT include
   `HBE/Renderer/ParticleSystem.h` from `Effects.h`. The
   `Effects::clear()` implementation goes in `Effects.cpp`
   where the full type is already visible.
3. **Never call `reloadScene(...)` while iterating bullets or
   enemies.** The reload swaps the underlying `std::vector`s.
   The hotkey check happens at the *top* of `onUpdate`, before
   any bullet/enemy loops, so this is naturally safe as long
   as you don't move the check.
4. **Preserve mode + helmet, reset everything else.** If you
   catch yourself adding fields to the "preserve" list, ask
   whether it makes tuning easier or harder. Difficulty +
   mode + helmet are enough. Everything else should be fresh.
5. **FileWatcher polling is `poll(dt)`, not `poll()`.** The
   engine watcher expects seconds since the last poll to run
   its own internal debounce. Don't invent your own timer.
6. **Log every reload path.** F5, F6, F7, and the two watcher
   fires each emit a `LogInfo`. Silent reloads make bug
   reports impossible: "did it reload or not?"
7. **Reload success is measured; reload failure is loud.**
   Success logs the elapsed ms with
   `std::chrono::steady_clock` (already imported by
   `Log.h`'s TU). Failure logs `LogError` **and** keeps the
   old scene running (rule §4 in doc 02 §6).
8. **Do not clear the shader / mesh caches.** `ResourceCache`
   owns textures, meshes and shaders and reloading them is
   an in-place mutation. Never call `resources().clear()` or
   any equivalent — you'll invalidate `m_quadMesh` and every
   sprite in the layer.

---

## Controls added in Item 11

| Key | Effect |
|---|---|
| `F5` | Full scene reload — reloads `level_01.json`, clears entities, respawns, snaps camera. Preserves difficulty, mode, helmet. |
| `F6` | Sprite shader hot reload — recompiles `sprite.vert` + `sprite.frag`. Nothing else touched. |
| `F7` | Soft respawn — clears enemies + bullets, respawns the demo Robot Soldier. Player is NOT touched (position, HP, mode all preserved). |

All existing controls (WASD, SPACE, S crouch, G ghost, B debug,
LMB fire, F1/F2/F3 difficulty, H helmet, R HP refill) keep the
same behavior. `R` is still just "refill HP" — it does NOT do a
scene reload, even though the "reload the fight" idea is tempting.

## What comes after this item

Item 12 layers enemy particle FX on top (muzzle flashes,
casings, hit sparks, death explosion, blood splatter on the
player). Now that F5 exists, you'll be leaning on it hard
during Item 12 tuning: land a hit → tweak spark color → F5 →
try again in 2 seconds.

Next: `01_reset_helpers.md` — the small `clear()` / `reload()`
/ `resetForRespawn()` additions to `Effects`, `World` and
`Player` that everything else in Item 11 builds on.
