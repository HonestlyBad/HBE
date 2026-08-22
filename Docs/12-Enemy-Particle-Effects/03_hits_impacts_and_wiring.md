# 03 — Hits, impacts, and the final `GameLayer` wiring

Doc 02 taught the `Enemy` when to emit FX and the
`EnemyManager` how to dispatch them — but the manager is
still holding a null `Effects*`, and three FX still don't
have a source hook: enemy bullet impacts on tiles, blood
splatter on the player, and metallic sparks when the player
hits a robot enemy.

Doc 03 wires all four in. Files touched:

* `G:\Dev\HBE\MegaX\include\Game\EnemyBullet.h`
* `G:\Dev\HBE\MegaX\src\Game\EnemyBullet.cpp`
* `G:\Dev\HBE\MegaX\src\Game\EnemyManager.cpp` (one line changed)
* `G:\Dev\HBE\MegaX\src\Game\GameLayer.cpp`

---

## 1. `include/Game/EnemyBullet.h` — impact tracking

Open `G:\Dev\HBE\MegaX\include\Game\EnemyBullet.h`.

### 1a. Add `struct Impact`, `m_impacts`, and `consumeImpacts`

Find the existing:

```cpp
        struct Bullet {
            float x = 0.0f, y = 0.0f;
            float vx = 0.0f, vy = 0.0f;
            int damage = 1;
            bool alive = true;
        };
```

**Immediately above** `struct Bullet`, add:

```cpp
        // Item 12: enemy bullets that die on a solid tile record their
        // impact point + tile id here. GameLayer drains this list each
        // frame and calls Effects::spawnEnemyBulletImpact per impact.
        struct Impact {
            float x = 0.0f;
            float y = 0.0f;
            int   tileId = 0;
        };
```

Then find the existing `clear()`:

```cpp
        void clear() { m_bullets.clear(); }
```

Extend it so a scene reload (Item 11) also drops pending
impacts:

```cpp
        void clear() { m_bullets.clear(); m_impacts.clear(); }

        // Item 12: drain pending impacts. Returns true if any impacts
        // were appended to `out`. Mirrors BulletManager::consumeImpacts.
        bool consumeImpacts(std::vector<Impact>& out) {
            if (m_impacts.empty()) return false;
            out.insert(out.end(), m_impacts.begin(), m_impacts.end());
            m_impacts.clear();
            return true;
        }
```

Finally, find the private members block:

```cpp
    private:
        bool pointInSolid(const HBE::Renderer::TileMap* map, const HBE::Renderer::TileMapLayer* layer, float x, float y) const;

        std::vector<Bullet> m_bullets;
```

Add the impacts vector **immediately below** `m_bullets`:

```cpp
        std::vector<Bullet> m_bullets;
        std::vector<Impact> m_impacts;   // Item 12
```

### 1b. Sanity: header include check

No new includes needed — `std::vector` is already there, and
`Impact` is a POD.

---

## 2. `src/Game/EnemyBullet.cpp` — push an `Impact` on tile kill

Open `G:\Dev\HBE\MegaX\src\Game\EnemyBullet.cpp`.

Find the current tile-kill site inside `update(...)`:

```cpp
            if (pointInSolid(map, layer, b.x, b.y)) {
                b.alive = false;
                continue;
            }
```

Replace those three lines with an impact-recording variant:

```cpp
            if (pointInSolid(map, layer, b.x, b.y)) {
                // Record the impact so GameLayer can spawn a particle
                // burst at this point. Tile id is looked up the same way
                // pointInSolid does; if the layer's tile at (x,y) is 0
                // (empty) something is off -- default to 0 and let
                // spawnEnemyBulletImpact's colorForTile fallback handle it.
                int tileId = 0;
                if (map && layer) {
                    const float tw = map->worldTileW();
                    const float th = map->worldTileH();
                    if (tw > 0.0f && th > 0.0f) {
                        const int tx = static_cast<int>(std::floor(b.x / tw));
                        const int ty = static_cast<int>(std::floor(b.y / th));
                        tileId = layer->at(tx, ty);
                    }
                }
                m_impacts.push_back(Impact{ b.x, b.y, tileId });
                b.alive = false;
                continue;
            }
```

The offscreen-cull path (`if (b.x < minX ...)`) does **not**
record an impact — those bullets flew past the view without
hitting anything, so a particle burst there would be invisible
and wasteful. Keep that path alive-flag-only.

`std::floor` requires `<cmath>` which is already included at
the top of `EnemyBullet.cpp`. No new includes.

---

## 3. `src/Game/EnemyManager.cpp` — swap `spawnBulletImpact` for `spawnHitSpark`

Open `G:\Dev\HBE\MegaX\src\Game\EnemyManager.cpp`.

Find the current inside of `checkBulletHits(...)`:

```cpp
                if (e.takeDamage(damagePerBullet)) {
                    b.alive = false;
                    ++hits;
                    if (effects) effects->spawnBulletImpact(b.x, b.y, 0);
                    break;
                }
```

Replace that one `if (effects) ...` line with the hit-spark
variant. The spark direction points AWAY from the shooter,
which is the same direction the bullet was traveling — since
the player's `Bullet::vx` is either `+speed` or `-speed`, we
use `b.vx` sign:

```cpp
                if (e.takeDamage(damagePerBullet)) {
                    b.alive = false;
                    ++hits;
                    if (effects) {
                        const int dir = (b.vx >= 0.0f) ? +1 : -1;
                        effects->spawnHitSpark(b.x, b.y, dir);
                    }
                    break;
                }
```

Rationale for switching effects:

* `spawnBulletImpact` was a temporary Item 07/08 reuse — it
  emits tan chunks + yellow sparks, which reads as "bullet
  hit a wall", not "bullet hit a robot".
* `spawnHitSpark` is metal-electric (white -> icy blue,
  additive), which reads as "the bullet just ricocheted off
  a hard shiny surface".

If your team decides sparks should fire on wall hits too, the
hook still exists in the drain block in doc 04 §3 — this doc
only changes the *enemy hit* case.

Look through the rest of `EnemyManager.cpp` — there should be
NO other `spawnBulletImpact` call. Grep to confirm. If
there's another one somewhere, it's dead code, not another
hit site. Leave those alone.

---

## 4. `src/Game/GameLayer.cpp` — the final wiring

Open `G:\Dev\HBE\MegaX\src\Game\GameLayer.cpp`.

### 4a. `setEffects` call in `onAttach`

Find the block right after the enemy manager is initialized
in `onAttach`:

```cpp
        if (!m_enemies.init(app.resources(), m_quadMesh, m_spriteShader)) {
            LogError("MegaX GameLayer: enemy manager init failed.");
        }
```

Add ONE line **immediately below** the closing `}` of that
`if` block:

```cpp
        if (!m_enemies.init(app.resources(), m_quadMesh, m_spriteShader)) {
            LogError("MegaX GameLayer: enemy manager init failed.");
        }
        m_enemies.setEffects(&m_effects);   // Item 12: FX dispatch target
```

Placement matters. `m_effects` must be `init(...)`-ed before
`setEffects` is meaningful, but the pointer itself is valid
the moment the layer is constructed (the `Effects` object
lives on the layer). The `Effects::init` call happens
*before* the enemy manager init on the current layout (grep
`m_effects.init` — it's a few lines up), so by the time we
call `setEffects` here, `m_effects` is a fully-initialized
object. Perfect.

Do NOT put `setEffects(nullptr)` anywhere. Once set, it
stays set until `GameLayer` is torn down. Item 11's scene
reload (`F5`) doesn't null it out; the reload calls
`m_effects.clear()` (particles empty, casings empty), not
`m_effects.shutdown()`.

### 4b. Blood splatter on player hit

Find the existing enemy-bullet-vs-player block in
`onUpdate(...)`:

```cpp
        {
            const AABB pb = m_player.hurtbox();
            for (auto& b : ebm.bullets()) {
                if (!b.alive) continue;
                if (std::fabs(b.x - pb.cx) > pb.w * 0.5f) continue;
                if (std::fabs(b.y - pb.cy) > pb.h * 0.5f) continue;

                const int kbDir = (b.vx >= 0.0f) ? +1 : -1;
                if (m_player.takeDamage(b.damage, kbDir)) {
                    b.alive = false;
                }
            }
        }
```

Add ONE line inside the `if (m_player.takeDamage(...))`
block, **immediately above** `b.alive = false;`:

```cpp
                if (m_player.takeDamage(b.damage, kbDir)) {
                    m_effects.spawnBloodSplatter(b.x, b.y, kbDir);   // Item 12
                    b.alive = false;
                }
```

Ghost mode is already gated inside `Player::takeDamage`
(returns false in Ghost) — the splatter never fires when the
player is intangible. i-frames are also gated the same way.
No new checks needed.

### 4c. Enemy bullet impact drain

Find the block where the enemy bullet manager gets updated:

```cpp
        auto& ebm = m_enemies.enemyBullets();
        ebm.update(dt, &m_world.map(), m_ground, m_camera.camera());
```

Insert a drain block **immediately below** the `ebm.update(...)`
line, and **above** the player-hurtbox loop from §4b:

```cpp
        auto& ebm = m_enemies.enemyBullets();
        ebm.update(dt, &m_world.map(), m_ground, m_camera.camera());

        // Item 12: enemy bullets that died on a solid tile this frame
        // produce a tile-tinted impact burst + red sparks. Drain BEFORE
        // the player-hurtbox loop so we don't peek at bullets already
        // culled by the update pass.
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
                ...
            }
        }
```

The order is deliberate:

1. `ebm.update` moves bullets, kills tile-hitters, records
   impacts.
2. The drain block spawns the impact bursts on those dead
   bullets.
3. The player-hurtbox loop only sees the surviving bullets
   (the tile-kill path already flipped `b.alive = false` and
   the `if (!b.alive) continue;` gate in the loop skips them).

If you flip steps 2 and 3, nothing bad happens — but a bullet
that hits the wall right next to the player would first
trigger the wall impact particle, then never touch the
player (which is correct). Order-independent, but the
top-to-bottom "update -> drain -> hurtbox" reads cleaner.

### 4d. `EnemyBullet.h` include check

`GameLayer.cpp` already `#include "Game/EnemyBullet.h"` from
Item 10 (needed for `m_enemies.enemyBullets()` to return a
concrete type). The new `EnemyBulletManager::Impact` type is
declared inside that header, so the include is sufficient —
no new `#include` line needed.

Grep to confirm the include is present:

```
grep "EnemyBullet.h" G:\Dev\HBE\MegaX\src\Game\GameLayer.cpp
```

If it's missing, add it at the top, next to the existing
Game/* includes.

---

## 5. Sanity check at end of doc 03

At this point you have every wire tied off. Attempt a build.

Expected build output — only these TUs should recompile:

```
  Effects.cpp
  Enemy.cpp
  EnemyManager.cpp
  EnemyBullet.cpp
  GameLayer.cpp
  MegaX.vcxproj -> ...\MegaX.exe
```

If any other TU recompiles unexpectedly, you touched a header
you shouldn't have. In particular: `Enemy.h`,
`EnemyManager.h`, and `EnemyBullet.h` are transitively
included by `GameLayer.h`, so any header change *does* fan
out to `GameLayer.cpp`. That's expected. But `Effects.h`
should only affect files that `#include "Game/Effects.h"`
(grep confirms: `GameLayer.cpp`, `EnemyManager.cpp`,
`Effects.cpp`).

Run the game. You should now see:

* Walk dust puffs off the soldier's feet in the same cadence
  as the player's.
* Muzzle flash fires with every enemy shot, red-orange.
* Bullets hitting walls produce a tile-tinted + red-spark
  burst.
* Taking a hit produces a small red splatter.
* Shooting the soldier produces bright metallic sparks (not
  the old tan chunks + yellow sparks).
* Killing the soldier plays the explosion once, then the
  sprite fades out over `deathFadeTime`.
* F5 (Item 11 reload) clears all particles and resets the
  fight; the new soldier walks with fresh dust puffs.

If any of those are missing, drop to the troubleshooting
matrix in doc 04 §6.

Common errors:

| Symptom | Cause | Fix |
|---|---|---|
| `error C2039 'consumeImpacts': is not a member of 'MegaX::EnemyBulletManager'` | `EnemyBullet.h` not saved. | Save + rebuild. |
| `error C2079 'MegaX::EnemyBulletManager::Impact'` (no forward decl) | `Impact` was declared outside the class body. | Move it inside `class EnemyBulletManager { public: ... };`. |
| Runtime: no muzzle flash | `m_enemies.setEffects(&m_effects)` not called before enemies fire. | Doc 03 §4a — check the placement. |
| Runtime: muzzle flash direction always faces right | Callback lambda used a wrong sign expression. | `(aimX >= sx) ? +1 : -1` — verify. |
| Runtime: no blood splatter | Player is in Ghost mode. `takeDamage` returns false, splatter guarded correctly. | Toggle Ghost off (G). Or you skipped Item 10's `Player::takeDamage(...)` change. |
| Runtime: hit spark cone points at the enemy instead of back at the shooter | `dirMin/dirMax` in `makeHitSpark` sit at 150-210 (pointing back). If the sign of `dir` is inverted at spawn, the mirror flips it back. | Check: shooter is to the LEFT of the enemy -> `b.vx > 0` -> `dir = +1` -> no mirror -> cone at 150-210 (roughly left) -> correct. |
| Runtime: enemy bullet impacts spawn at (0,0) | `imp.x`/`imp.y` uninitialized because `Impact{ b.x, b.y, tileId }` used aggregate init but a field was omitted from `struct Impact`. | Verify the struct has exactly three fields in the header AND you're pushing three values. |
| Runtime: blood splatter and hit spark both fire when player shoots wall next to enemy | Player bullet impacts wall (yellow sparks) + hit spark should NOT trigger. Verify `checkBulletHits` only fires spark on `takeDamage` return true. | The current code path is correct: player bullet vs. wall goes through `BulletManager::update` and `consumeImpacts` in `GameLayer` (Item 07's `spawnBulletImpact` call, untouched). Hit spark only comes from `EnemyManager::checkBulletHits`. |
| Runtime: explosion feels weak | Adjust `makeEnemyExplosion` chunk burst count from 20 -> 32 and speed from 180-360 -> 240-480. Also try increasing `core.startSizeMin/Max`. | Tuning knob. |
| Runtime: F5 leaves stale enemy-bullet impacts | `clear()` doesn't clear `m_impacts`. | Doc 03 §1a extended `clear()` — verify the code change. |

---

Next: `04_build_run_and_verify.md` — the msbuild command,
the full six-group verify checklist, tuning knobs, and the
comprehensive troubleshooting matrix.
