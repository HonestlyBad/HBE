# 04 — Build, run, verify, tune

You've now touched every file listed in doc 00's table. This
final doc walks the build, the manual verify checklist for
every new effect × trigger combination, tuning knobs for the
color / intensity / cadence of each effect, and a
troubleshooting matrix keyed off the most likely bugs.

---

## 1. Project file — no new sources this time

Item 12 introduces **zero** new `.h` / `.cpp` files. Every
edit lives in files that are already listed in
`MegaX.vcxproj`:

* `Effects.h` / `Effects.cpp`
* `Enemy.h` / `Enemy.cpp`
* `EnemyManager.h` / `EnemyManager.cpp`
* `EnemyBullet.h` / `EnemyBullet.cpp`
* `GameLayer.cpp`

**No `MegaX.vcxproj` edits required.** Skip this step (same
as Item 11).

Quick sanity grep — each of these should return exactly one
`<ClCompile Include="..." />` line:

```
grep "Effects.cpp"     G:\Dev\HBE\MegaX\MegaX.vcxproj
grep "Enemy.cpp"       G:\Dev\HBE\MegaX\MegaX.vcxproj
grep "EnemyManager.cpp" G:\Dev\HBE\MegaX\MegaX.vcxproj
grep "EnemyBullet.cpp" G:\Dev\HBE\MegaX\MegaX.vcxproj
grep "GameLayer.cpp"   G:\Dev\HBE\MegaX\MegaX.vcxproj
```

---

## 2. Build

From a `vcvars64.bat`-initialized shell:

```
msbuild HonestlyBadEngine.slnx /p:Configuration=Debug /p:Platform=x64 /t:MegaX /m /nologo /v:m
```

Expected new lines (touched TUs only recompile):

```
  Effects.cpp
  Enemy.cpp
  EnemyManager.cpp
  EnemyBullet.cpp
  GameLayer.cpp
  MegaX.vcxproj -> G:\Dev\HBE\MegaX\bin\x64\Debug\MegaX.exe
  MegaX: copying runtime deps to G:\Dev\HBE\MegaX\bin\x64\Debug\
```

Only pre-existing warnings should appear (`C4099 TileMap`
from Item 07, `C4244 InputMap` from Core, `C4305` in
`Effects.cpp` from Item 07). Any new C-level error → jump to
the troubleshooting matrix (§6).

---

## 3. Run

```
G:\Dev\HBE\MegaX\bin\x64\Debug\MegaX.exe
```

Expected startup log lines (unchanged from Item 11):

```
[INFO ] World loaded 'maps/level_01.json' (3 tilesets, N layers, ... animated tiles, ... instances).
[INFO ] MegaX GameLayer attached (Play mode; press G for Ghost).
[INFO ] MegaX GameLayer: scene watches active (maps/level_01.json, sprite shader).
```

No Item 12-specific log line — all wiring is silent (as
intended: FX are visual, not logged).

Confirm the HUD reads 5 HP pips + yellow "Difficult" pill.

---

## 4. Verify checklist

Group A is Item 11 regression (F5 / F6 / F7 must still work).
Groups B–G are Item 12 proper.

### Group A — Item 11 regression

* [ ] F5 reloads the scene, clears all particles, respawns
      enemy at post.
* [ ] F6 hot reloads the sprite shader.
* [ ] F7 soft respawn.
* [ ] Editing `level_01.json` triggers auto-reload.
* [ ] All Item 10 combat behavior unchanged (F1/F2/F3, R, G,
      B, H).

### Group B — Enemy movement effects

* [ ] Walk toward the soldier without provoking him
      (crouch-approach so hearing radius doesn't trigger, or
      approach from behind cover). Watch his feet as he
      patrols left/right.
* [ ] Small tan dust puffs kick up on the same cadence as
      the player's walk dust (~5 per second at
      `walkDustPeriod = 0.18`).
* [ ] Dust is TILE-COLORED — if you build a walkway of a
      different tileset color and stand him on it, the dust
      changes tint. (Requires a modified `level_01.json`;
      optional check.)
* [ ] When he pauses at a patrol endpoint (`patrolWaitAtEnd`
      seconds), NO dust puffs fire.
* [ ] When he jumps during chase and lands, a bigger tan
      burst spawns at the landing feet position. (Requires
      chase state, which requires being detected — trigger
      by walking into his cone.)
* [ ] Ghost mode: dust still spawns (dust is a visual, not
      damage). Doc 00's rule: FX are not gated by Ghost;
      only damage-adjacent FX (blood splatter) are.

### Group C — Enemy shooting effects

* [ ] Get spotted -> Alert -> Chase. Every shot leaves a
      brief red-orange muzzle flash at the muzzle tip.
* [ ] Face-right enemies flash a cone to the right;
      face-left enemies flash a cone to the left (visible
      by comparing shots at both patrol endpoints).
* [ ] The red bullet streak from Item 10 still fires — the
      muzzle flash is *additional*, not replacement.

### Group D — Enemy bullet impacts on tiles

* [ ] Duck behind cover. Enemy bullets hit the tile in front
      of you. Each impact spawns:
      * Tile-tinted chunk chunks (same color as the tile
        top).
      * A red-spark burst on top.
* [ ] Impacts on air (offscreen or view-culled) do NOT
      spawn particles.
* [ ] Multiple simultaneous impacts: each spawns its own
      burst, no missed ones.

### Group E — Player hits enemy (hit sparks)

* [ ] Shoot the soldier. Each hit produces a bright metallic
      spark (white -> icy blue), additive, small directional
      cone back toward you.
* [ ] The old tan chunk + yellow sparks (Item 07's
      `bullet_impact`) does NOT fire on enemy hits anymore.
      (It DOES still fire on player bullets hitting walls —
      that's the Item 07 `consumeImpacts` block in
      `GameLayer`, untouched.)
* [ ] Sparks direction: shooting the enemy from the LEFT
      makes sparks fly leftward (back toward you). Shooting
      from the RIGHT makes them fly rightward.

### Group F — Enemy hits player (blood splatter)

* [ ] Stand still, let a bullet hit you.
* [ ] Small red splatter puffs off your hurtbox. NON-additive
      (looks solid, not glowing).
* [ ] Splatter direction matches the bullet's travel: a
      right-flying bullet produces a right-splatter cone.
* [ ] During i-frames, subsequent bullets pass through with
      NO splatter (`takeDamage` returns false, splatter
      guarded).
* [ ] Ghost mode: NO splatter (bullets pass through the
      player; `takeDamage` early-outs on Ghost).

### Group G — Enemy death explosion

* [ ] Land the third hit on the soldier. On the very frame
      HP hits 0:
      * A yellow-white core flashes at his center of mass.
      * Orange-red chunks fly outward.
      * A dark smoke poof rises above.
      * The sprite starts fading (Item 08's death fade,
        unchanged — takes `deathFadeTime` seconds).
* [ ] The explosion plays exactly ONCE. It does NOT re-fire
      each frame during the fade.
* [ ] Bullets already in flight when the enemy dies keep
      flying — muzzle flashes for those bullets already
      fired when he was alive don't retroactively vanish.
* [ ] After the sprite fully fades, the enemy vector erases
      him (`isFinished()`), no leftover explosion state.

### Group H — Composite scenarios

* [ ] Full combat pass: walk in, get detected, take 2 hits
      (see splatter twice), dodge behind cover, hear impacts
      on the wall, come out, kill the soldier, watch the
      explosion. All FX should coexist without frame drops.
* [ ] Press F1 (Casual). Same soldier walks slower ->
      slower dust cadence (visible), fires slower -> fewer
      muzzle flashes.
* [ ] Press F3 (Challenging). Faster dust, dense muzzle
      flashes, dense impact bursts.
* [ ] Press F5 mid-explosion. Explosion particles are wiped
      cleanly by `Effects::clear()`; fresh soldier walks in
      with fresh dust.
* [ ] Enemy jumps mid-chase (Item 09 chase-jump logic). On
      landing, dust burst fires. Then the walk-dust cadence
      picks back up.

If Groups A–H all tick, Item 12 is done.

---

## 5. Tuning table — the flavor knobs

Every knob lives in `Effects.cpp`'s `make<name>()` factories
(edit + F5 to iterate). A few are in headers.

### Cadence / intensity

| Symptom | Field | Direction |
|---|---|---|
| Enemy walk dust feels too frequent | `EnemyManager::walkDustPeriod` | `0.18 → 0.24` |
| Enemy walk dust feels too sparse | `EnemyManager::walkDustPeriod` | `0.18 → 0.12` |
| Muzzle flash too subtle | `makeEnemyMuzzleFlash().core.startSizeMin/Max` | `8-12 → 12-16` |
| Muzzle flash too gaudy on Challenging (fires every 0.55s) | `makeEnemyMuzzleFlash().sparks.maxParticles` | `10 → 6` |
| Blood splatter reads too small | `makeBloodSplatter().drops.speedMin/Max` | `90-220 → 140-260` (bigger cone) |
| Blood splatter too dense at low HP | `makeBloodSplatter().drops.bursts` | `{ 0, 8, 1 } → { 0, 5, 1 }` |
| Hit sparks feel weak | `makeHitSpark().sparks.maxParticles` | `14 → 20` |
| Hit sparks too flashy (blinding on Challenging) | `makeHitSpark().sparks.startA` | `1.0 → 0.75` |
| Explosion feels weak | `makeEnemyExplosion().chunks.speedMin/Max` | `180-360 → 240-480` |
| Explosion smoke doesn't linger | `makeEnemyExplosion().smoke.lifetimeMin/Max` | `0.50-0.90 → 0.80-1.30` |
| Explosion core too bright | `makeEnemyExplosion().core.startA` | `1.0 → 0.85` |

### Color

| Symptom | Field | Direction |
|---|---|---|
| Enemy muzzle flash reads as yellow (not distinguishable from player's) | `makeEnemyMuzzleFlash().core.startR/G/B` | `1.0 / 0.55 / 0.20 → 1.0 / 0.35 / 0.10` (deeper red) |
| Blood splatter too pink | `makeBloodSplatter().drops.startR/G/B` | `0.75 / 0.05 / 0.05 → 0.90 / 0.02 / 0.02` |
| Hit sparks too white (not "electric") | `makeHitSpark().sparks.endB` | `1.0 → 1.0` (already blue) — instead push endR down `0.4 → 0.15` for cooler tone |
| Explosion looks like a fire, not a boom | Increase `makeEnemyExplosion().core.startG` from `1.0 → 1.0` (unchanged) and push `core.startB` up `0.85 → 0.95` for whiter core |

### Layout / anchor

| Symptom | Field | Direction |
|---|---|---|
| Muzzle flash spawns behind the sprite | `Enemy::muzzleForwardX` | `22 → 26` |
| Muzzle flash spawns below the gun | `Enemy::muzzleAboveFeet` | `26 → 30` |
| Explosion spawns at the feet (looks like the ground exploded) | Edit the manager call: `feetY + e.boxHalfH` -> `feetY + 2.0f * e.boxHalfH` for higher pivot |
| Blood splatter spawns at bullet, not player center | Intentional — the splatter marks the wound. If you want it centered on the player instead, replace `b.x, b.y` with `pb.cx, pb.cy` in the GameLayer wire (doc 03 §4b) |
| Enemy walk dust spawns behind the feet on Challenging | `EnemyManager::walkDustPeriod` scales with speed only via cadence; the puff itself is centered on `(e.x(), e.feetY())`. If the sprite's visual feet are offset, adjust `Enemy::spriteFeetOffsetY` (Item 08 tunable) — the FX will follow automatically. |

Per-enemy overrides work the same way Item 09/10 established
them: mutate the Item 12 accessor's *base* if you need
per-enemy tuning:

```cpp
if (Enemy* e = m_enemies.spawn(ex, eGroundY, -1)) {
    e->startHp = 3;
    e->setPatrolPath(ex - 3.0f * kTilePx, ex + 3.0f * kTilePx, 1.0f);
    e->snapshotBaseStats();
    e->applyDifficulty(m_enemies.profile());
    e->spawn(ex, eGroundY, -1);
}
```

No Item 12 accessors need overriding — everything is baked
into `Effects` recipes and dispatched by the manager.

---

## 6. Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Build: `error C2039 'spawnEnemyMuzzleFlash': is not a member of 'MegaX::Effects'` | `Effects.h` not saved after doc 01 §1. | Save + rebuild. |
| Build: `error C2065 'makeEnemyExplosion': undeclared identifier` | Factory pasted below `Effects::init` instead of above it. | Move it above `init` (doc 01 §2). |
| Build: `error C2079 'Impact' uses undefined class` | `struct Impact` placed outside the `EnemyBulletManager` class body. | Move inside `class EnemyBulletManager { public: ... };` (doc 03 §1a). |
| Build: `error C2248 'EnemyManager::m_effects': cannot access private member` | The fire callback lambda moved outside the enclosing member function. | Re-inline it inside `EnemyManager::spawn(...)`. |
| Build: `error C2039 'consumeJustDied': is not a member of 'MegaX::Enemy'` | `Enemy.h` not saved after doc 02 §1a. | Save + rebuild. |
| Runtime: no muzzle flash | `m_enemies.setEffects(&m_effects)` not called before the first shot. Doc 03 §4a. | Add the missing line. |
| Runtime: muzzle flash spawns at (0,0) at start of scene | `sx/sy` passed to callback are 0 because `muzzleWorldPos` wasn't computed. | Verify `Enemy::tickShooting` calls `muzzleWorldPos(mx, my);` before `m_fireFn(m_fireCtx, mx, my, ...)` (this is Item 10 code — should not have changed). |
| Runtime: no walk dust for enemy | `Enemy::consumeWalkDustPuff` sees `m_vx == 0` because the enemy is Idle (patrol dwell) or ChasingWithStandoff (v=0 near player). | Correct — walk dust only fires while horizontal velocity > 5. Widen the threshold in `consumeWalkDustPuff` from 5 → 1 if you want dust on very slow patrol. |
| Runtime: walk dust looks black | Tile top color lookup failed for the tile under the enemy's feet. Fallback tan should show — if you're seeing black, `m_ps->spawn` was called with the wrong effect key. | Grep `"walk_dust"` in `Effects.cpp` — the registration and the spawn key must match. |
| Runtime: landing dust fires on scene start | `m_wasGroundedLast` defaulted to `false` instead of `true`. Doc 02 §1b: `bool m_wasGroundedLast = true;`. | Check the default value. |
| Runtime: landing dust fires every frame while airborne | `m_landedThisFrame` never cleared at top of tick. Doc 02 §2a. | Add `m_landedThisFrame = false;` at top of `Enemy::tick`. |
| Runtime: explosion fires every frame during death fade | `m_justDied` never cleared by `consumeJustDied()` from the manager. | Doc 02 §4b — the dispatch loop must call `e.consumeJustDied()` after `spawnEnemyExplosion`. |
| Runtime: explosion never fires | `takeDamage` was refactored and the `m_justDied = true;` line moved outside the `m_hp <= 0` block. | Verify the line is inside the `if (m_hp <= 0)` branch, right below `m_dead = true;`. |
| Runtime: hit spark direction inverted | `dir = (b.vx >= 0.0f) ? +1 : -1` — bullet flying right = `dir +1` -> spark cone at `dirMin/dirMax` unchanged. The spark cone in `makeHitSpark` is centered at 180 degrees (`dirMin=150, dirMax=210`), so `dir +1` means "sparks fly leftward (back)". If your test shows sparks fly rightward, the mirror was applied inversely. | Double-check the mirror block: `if (dir < 0) { ... }` — for `dir >= 0` we do NOT mirror. |
| Runtime: blood splatter never fires | Player is in Ghost mode, or the `if (m_player.takeDamage(...))` returned false. Splatter is intentionally gated behind the successful damage return. | Confirm you took actual HP damage before checking for splatter. |
| Runtime: blood splatter fires but nothing else | Only the splatter wire is in place; the FX registrations from doc 01 §3 didn't get committed. | Verify the five `m_ps->registerEffect(...)` calls at the bottom of `Effects::init`. |
| Runtime: enemy bullet impact particles never fire on wall hits | The drain block wasn't inserted after `ebm.update(...)`. Doc 03 §4c. | Insert the drain block. |
| Runtime: enemy bullet impact particles fire on empty air | You're recording impacts in the offscreen-cull path too. Doc 03 §2 says: only push an Impact inside the `pointInSolid` branch. | Restrict the `push_back` to the tile-hit branch. |
| Runtime: enemy bullet impact tile id is always 0 | The tile probe reads `layer->at(tx, ty)` at the exact bullet position. If the bullet is one pixel above the tile, `at` returns 0. | Add a small downward probe: use `b.y - 0.5f` instead of `b.y` in the impact tile lookup. |
| Runtime: F5 leaves stale enemy bullet impact particles | `EnemyBulletManager::clear()` doesn't drop `m_impacts`. Doc 03 §1a extended it. | Verify `clear() { m_bullets.clear(); m_impacts.clear(); }`. |
| Runtime: particles disappear immediately after spawn | `Effects::update` isn't being called. Grep `m_effects.update(dt)` in `GameLayer::onUpdate` — should be exactly one call, at the end of the update pass. | Item 07 code — check it wasn't accidentally deleted during Item 11 or 12 edits. |
| Runtime: HUD is covered by particle bursts | Particles render on layer 102/103 (world), HUD renders in `RenderPass::Overlay` — no overlap. If you see HUD occlusion, you accidentally changed a particle's `pass` from `RenderPass::World` to `RenderPass::Overlay`. | All effect `pass` fields must remain `RenderPass::World` (which is the default; do not set them explicitly). |
| Runtime: build succeeds but nothing new visible | You built to Release and are running Debug (or vice versa). | `dir G:\Dev\HBE\MegaX\bin\x64\Debug\MegaX.exe /A:H` — check timestamp matches the last build. |
| Runtime: crash on enemy death — `Access violation` inside `spawnEnemyExplosion` | `m_effects` is null (setEffects was never called before the enemy could die). | Doc 03 §4a — verify the setter is called in `onAttach`. Even without it, the `if (m_effects)` guard should prevent the crash — verify the guard is present in `EnemyManager::update`. |
| Runtime: enemy bullet impact sparks look yellow, not red | You copy-pasted `makeBulletImpact` verbatim instead of `makeEnemyBulletImpact`. | Compare the `sparks.startR/G/B` values against doc 01 §2b — should be red-orange. |

---

## 7. What Item 13 will add (rough shape — not committed)

Item 12 wraps the "combat feel" arc. Item 13+ is expected to
cover one of:

* Audio — SFX for muzzle flashes, hit sparks, explosions.
  All the moments Item 12 gave visual feedback to will
  probably want audio feedback next.
* Multiple enemy types — each with their own explosion +
  hit spark recipes registered.
* Boss patterns — bigger effects, screen shake, particle
  waves.

Whichever comes first, Item 12 is the last item that will
touch `Effects.cpp` in this exact pattern of "add a new
`make...` + registration + spawn". After this the recipe
count is high enough that the Effects TU is starting to
strain — future items might factor recipes into a data-driven
JSON registration pass.

---

## 8. Wrap-up

If Groups A–H all tick, Item 12 is complete. The Robot
Soldier is now a fully-flavored combat opponent: he walks,
shoots, dies, and bleeds *you* — every action has a
particle event tied to it, calibrated against the Item 07
player effects for visual cohesion.

Have fun. This is the item where the game finally starts to
*feel* like Mega Man X.
