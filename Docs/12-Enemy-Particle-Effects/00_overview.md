# 12 — Enemy particle effects (overview)

Item 10 gave the Robot Soldier a working combat loop. Item 11
made iterating on that loop bearable (F5 reloads the scene in 2
seconds). Item 12 finally gives the fight some *feel*: every
significant enemy action now has a particle effect wired to it,
mirroring the player-side FX Item 07 already delivered.

Concretely, Item 12 adds seven new game-side effects and wires
them into the six moments that matter:

| Moment | Effect | Where it fires |
|---|---|---|
| Enemy walks on ground | `walk_dust` (existing, tile-colored) | `EnemyManager::update` per enemy per frame |
| Enemy lands from a jump | `landing_dust` (existing, tile-colored) | `EnemyManager::update`, on `Enemy::landedThisFrame()` transition |
| Enemy fires its gun | `enemy_muzzle_flash` (**new**, red/orange) | Inside the wrapped fire callback in `EnemyManager::spawn` |
| Enemy bullet hits a tile | `enemy_bullet_impact` (**new**, tile-tint + red sparks) | `EnemyBulletManager` records `Impact`s; `GameLayer` drains them |
| Enemy bullet hits the player | `blood_splatter` (**new**, small red splatter) | `GameLayer` enemy-bullet-vs-hurtbox block, on damage taken |
| Player bullet hits the enemy | `hit_spark` (**new**, metallic sparks — enemy is a robot) | `EnemyManager::checkBulletHits`, replacing the current `bullet_impact` call |
| Enemy dies | `enemy_explosion` (**new**, big yellow-orange-red burst) | `EnemyManager::update`, on `Enemy::justDied()` transition — plays **once** on the sprite before the existing death fade completes |

Zero engine edits. All new effect defs (`EmitterConfig`
composites) live in `Effects.cpp` next to the existing player
effects.

---

## Goals

* Reuse the existing tile-colored `walk_dust` and `land_dust`
  effect registrations for enemies (the Item 07 code already
  paints them from `TileMapLoader::sampleTileTopColors`). No
  need for enemy-specific dust colors.
* **New**: `spawnEnemyMuzzleFlash(x, y, dir)` — reddish core +
  short muzzle sparks, mirrored across facing just like the
  player muzzle flash.
* **New**: `spawnEnemyBulletImpact(x, y, tileId)` — tile-tinted
  chunk burst + small red-tint sparks. The color contract for
  enemy bullets is red-orange to distinguish from the player's
  yellow-flash impacts.
* **New**: `spawnEnemyExplosion(x, y)` — one big burst of
  yellow-white core + orange-red chunks + short-lived black
  smoke sim (via particle color fade — no new subsystem). Plays
  once per enemy death.
* **New**: `spawnHitSpark(x, y, dir)` — bright electric
  spark burst tinted metallic (cool white -> icy blue),
  directional back toward the shooter. Replaces the current
  generic `bullet_impact` used inside
  `EnemyManager::checkBulletHits` (which was a temporary Item 07
  reuse).
* **New**: `spawnBloodSplatter(x, y, dir)` — small deep-red
  splatter, directional in the direction the bullet was
  traveling. Non-additive blend so it reads as blood, not fire.
* `Enemy` learns three self-observation accessors so the
  manager can spawn effects at the right moment without
  peeking into private state:
  * `bool landedThisFrame() const;` — set once when
    `m_grounded` transitions `false -> true` in `applyPhysics`.
    Cleared at the top of the next `tick`.
  * `int  groundTileId() const;` — the tile id currently under
    the enemy's feet (0 if not grounded / no tile below).
    Sampled inside `applyPhysics` when grounded.
  * `bool justDied() const;` — set exactly once, at the frame
    HP drops to 0 inside `Enemy::takeDamage`. Cleared the next
    time `EnemyManager::update` sees it.
  * Bonus: `float m_walkDustAccum` per-enemy accumulator so
    multi-enemy scenes don't share a single cadence.
* `EnemyManager` gains `setEffects(Effects*)`, called once from
  `GameLayer::onAttach`. Every frame it iterates live enemies
  and spawns walk dust / landing dust / explosion at the right
  moment.
* `EnemyManager::spawn` wraps the existing fire callback with a
  new one that ALSO calls `effects->spawnEnemyMuzzleFlash(...)`
  before delegating to the bullet spawn. The bullet spawn logic
  itself is unchanged.
* `EnemyBulletManager` gains an `Impact` mechanism identical
  in shape to `BulletManager::Impact` so `GameLayer` can drain
  impacts each frame and spawn `enemy_bullet_impact` particles.
* `EnemyManager::checkBulletHits` swaps its
  `effects->spawnBulletImpact(...)` call for
  `effects->spawnHitSpark(...)` (sparks are correct — it's a
  robot).
* `GameLayer` gains one line in the enemy-bullet-vs-player-
  hurtbox block: on a successful `takeDamage(...)` call,
  `m_effects.spawnBloodSplatter(b.x, b.y, kbDir)`.

## Non-goals (deferred)

| Behavior | Deferred to |
|---|---|
| Enemy-type-specific effects (different explosion per enemy) | Later polish, once a second enemy exists |
| Screen shake on enemy death | Later polish |
| Blood pool decal that persists on the ground | Later polish (decal system doesn't exist yet) |
| Casing ejection on the enemy's fire (Robot Soldier doesn't need casings — it's laser-flavor) | Never (design call) |
| Damage numbers popping off hit sparks | Later HUD polish |
| Sound effects | Never in this item (Item 13+ audio) |
| Full data-driven effect binding per enemy stat block | Later data pipeline |
| Different hit spark color for shots that kill vs. shots that don't | Later polish |

Item 12 stays inside the game project. **Zero engine edits.**

---

## What "done" looks like

You'll know Item 12 is finished when:

1. Fresh scene, walk toward the Robot Soldier. As **the
   soldier** patrols left/right, small tan dust puffs kick up
   from his feet on the same cadence as the player's walk
   dust (the same registration is used for both).

2. Get seen (`!` bubble). Soldier fires. Every shot leaves a
   short red-orange muzzle flash at the muzzle tip AND spawns
   the existing red bullet streak (Item 10 behavior unchanged).

3. Dodge behind cover. The bullets thump into the tiles behind
   you and each impact spawns a tiny tile-tinted chunk burst +
   red spark burst (`enemy_bullet_impact`), matching the
   ground color of the tile they hit.

4. Take a hit. The moment `takeDamage(...)` succeeds a small
   deep-red splatter (`blood_splatter`) puffs off your hurtbox
   in the bullet's travel direction. Ghost mode still bypasses
   damage AND the splatter — the existing early-out in
   `Player::takeDamage` gates both.

5. Shoot the soldier. Instead of the generic tile-chunk burst
   Item 07/10 was reusing, each hit produces a bright metallic
   spark burst (cool white -> icy blue). Additive blend, ~0.15s
   lifetime, small radial cone.

6. Land the killing shot. The moment HP drops to 0:
   * `enemy_explosion` plays **once** at the enemy's center of
     mass (roughly `feetY + boxHalfH`).
   * The existing death fade animation (Item 08) keeps
     running unchanged — sprite tint fades over
     `deathFadeTime` (0.6s by default). The explosion is a
     separate one-shot particle event on top of the fade, not
     a replacement.

7. Watch the soldier jump during a chase (jump-cooldown
   permitting from Item 09/10). The moment his feet touch
   ground again, a tan `landing_dust` puff fires — same
   registration the player uses, tinted by the actual tile
   under his feet.

8. Press `F1` (Casual). The enemy walk cadence looks slower
   (matches the reduced `chaseSpeed`). The muzzle flash and
   blood splatter still fire at the same visual intensity —
   difficulty affects **rate**, not effect appearance.

9. Press `F5` (Item 11 scene reload). All effect state is
   cleared (Item 11's `Effects::clear()` already handles this).
   No stale sparks / dust / splatter left over. New soldier
   spawns clean.

10. Press `B` — the debug overlay still shows the hurtbox +
    hitbox. Particles do NOT clutter it: effects render on
    layer 102 (above bullets), overlay renders in
    `RenderPass::Overlay`.

## Files touched (all game-side)

| File | Item 11 | Item 12 |
|---|---|---|
| `include/Game/Effects.h` | + `clear()` | + `spawnEnemyMuzzleFlash`, `spawnEnemyBulletImpact`, `spawnEnemyExplosion`, `spawnHitSpark`, `spawnBloodSplatter`, `spawnWalkDustBurst` |
| `src/Game/Effects.cpp` | + `clear()` | + 5 new `make...` effect def factories, 5 new spawn implementations, extract `spawnWalkDustBurst` from `tickWalkDust` |
| `include/Game/Enemy.h` | shooting + difficulty | + `landedThisFrame()`, `groundTileId()`, `justDied()`, `consumeWalkDustPuff(dt, period)`, private fields `m_landedThisFrame`, `m_wasGroundedLast`, `m_groundTileId`, `m_justDied`, `m_walkDustAccum` |
| `src/Game/Enemy.cpp` | shooting logic | + landing detection in `applyPhysics`, tile sample under feet, `justDied` set in `takeDamage`, top-of-tick clearing |
| `include/Game/EnemyManager.h` | + difficulty + enemy bullets | + `setEffects(Effects*)`, `Effects* m_effects = nullptr;`, `float walkDustPeriod = 0.18f;` |
| `src/Game/EnemyManager.cpp` | manager loops | + per-enemy walk/land dust + explosion dispatch in `update`, fire-callback wrapping in `spawn`, swap `spawnBulletImpact` for `spawnHitSpark` in `checkBulletHits` |
| `include/Game/EnemyBullet.h` | thin bullet vec | + `struct Impact`, `std::vector<Impact> m_impacts`, `consumeImpacts(out)`, extended `update(...)` to record impacts on tile hit |
| `src/Game/EnemyBullet.cpp` | update/render/cull | + push an `Impact` when `pointInSolid` returns true |
| `src/Game/GameLayer.cpp` | reload + hotkeys | + `m_enemies.setEffects(&m_effects)` in `onAttach`, blood splatter on enemy-bullet-vs-player hit, drain enemy bullet impacts and call `spawnEnemyBulletImpact` |
| `include/Game/GameLayer.h` | — | *no change* |
| `MegaX.vcxproj` / `.filters` | — | *no new source files* — everything piggy-backs on existing TUs |

Zero engine files.

---

## Design shape (short version)

### New effect defs

Each of the five new spawn functions has a matching
`make<Name>()` static helper in `Effects.cpp` that returns an
`EffectDef` (vector of `EmitterConfig`s), same shape as the
existing player effects. Naming convention:

```
static EffectDef makeEnemyMuzzleFlash();
static EffectDef makeEnemyBulletImpact();
static EffectDef makeEnemyExplosion();
static EffectDef makeHitSpark();
static EffectDef makeBloodSplatter();
```

Each of those gets a corresponding
`m_ps->registerEffect("<name>", ...)` call in `Effects::init`,
following the existing pattern for `walk_dust`, `land_dust`,
etc. Registration names (lowercase, snake_case) match the
`c.name` field in the `EmitterConfig`.

Full recipes with all the emitter tunables are in doc 01 §2.
These aren't arbitrary — they're calibrated against Item 10's
existing muzzle_flash / bullet_impact recipes so the two sides
of the fight look coherent (same particle counts, same size
ranges, different colors + blend modes).

### `Enemy` self-observation

Three read-only accessors plus one consume-style helper. All
read from private state that's already tracked internally:

```cpp
bool  landedThisFrame() const;   // set/cleared by tick/applyPhysics
int   groundTileId()    const;   // sampled by applyPhysics when grounded
bool  justDied()        const;   // set by takeDamage, cleared by manager
bool  consumeWalkDustPuff(float dt, float period);   // per-enemy accum
```

`landedThisFrame()` is a one-shot boolean pattern used by the
Item 04 player code (`Player::landedThisFrame()`) — it's set
inside `applyPhysics` when `!prevGrounded && m_grounded`, and
cleared at the top of `Enemy::tick`. `EnemyManager::update`
runs `tick` first (so `landedThisFrame` is fresh), then reads
it.

`justDied()` mirrors the same pattern but is set inside
`takeDamage` and cleared by `EnemyManager::update` after
spawning the explosion. If two things kill the enemy in the
same frame (impossible today but be paranoid),
`takeDamage`'s early-out `if (m_dead) return false;` prevents
double-firing.

`consumeWalkDustPuff(dt, period)` is deliberately per-enemy so
multiple enemies don't share one accumulator. Returns `true`
at most once per `period` seconds, only if the enemy is
grounded and horizontally moving.

### Manager wiring

`EnemyManager` gains one non-owning pointer:

```cpp
Effects* m_effects = nullptr;
void setEffects(Effects* fx) { m_effects = fx; }
```

Called from `GameLayer::onAttach` once, right after
`m_effects.init(...)` and `m_enemies.init(...)` succeed.

In `EnemyManager::update`, right after the tick loop, we run:

```cpp
for (auto& e : m_enemies) {
    if (e.isFinished()) continue;

    // Death explosion -- one-shot on the frame HP hit zero.
    if (e.justDied() && m_effects) {
        m_effects->spawnEnemyExplosion(e.x(), e.feetY() + e.boxHalfH);
        e.consumeJustDied();   // clears the latch
    }

    if (!e.isAlive()) continue;   // dying (fade in progress) -> no dust

    // Landing dust -- on the frame m_grounded went false -> true.
    if (e.landedThisFrame() && m_effects) {
        m_effects->spawnLandingDust(e.x(), e.feetY(), e.groundTileId());
    }

    // Walking dust -- per-enemy accumulator.
    if (m_effects && e.consumeWalkDustPuff(dt, walkDustPeriod)) {
        m_effects->spawnWalkDustBurst(e.x(), e.feetY(), e.groundTileId());
    }
}
```

The `erase-remove` for finished enemies happens **after** this
block, so a just-died enemy still gets its explosion frame
before being erased on the following frame(s).

### Fire callback wrapping

Current callback in `EnemyManager::spawn` (Item 10):

```cpp
e.setFireCallback(
    [](void* ctx, float sx, float sy, float aimX, float aimY, float speed, int damage) {
        static_cast<EnemyManager*>(ctx)->m_enemyBullets->spawn(sx, sy, aimX, aimY, speed, damage);
    },
    this);
```

We extend it to fire the muzzle flash particle first, using
the aim direction as the flash direction:

```cpp
e.setFireCallback(
    [](void* ctx, float sx, float sy, float aimX, float aimY, float speed, int damage) {
        auto* mgr = static_cast<EnemyManager*>(ctx);
        // dir is +1 if the shot is heading right, -1 if left.
        const int dir = (aimX >= sx) ? +1 : -1;
        if (mgr->m_effects) mgr->m_effects->spawnEnemyMuzzleFlash(sx, sy, dir);
        mgr->m_enemyBullets->spawn(sx, sy, aimX, aimY, speed, damage);
    },
    this);
```

Because the `FireFn` signature is a `void(*)` (raw function
pointer), non-capturing lambdas are the only way to interop.
`Effects* m_effects` is reached via `mgr->m_effects` — which
means we need to make `m_effects` accessible from inside the
lambda. Two clean options:

* Declare the lambda as a `friend` of `EnemyManager` (adds
  boilerplate).
* Make `m_effects` a `public:` member of `EnemyManager`. It's
  a raw non-owning pointer, so exposing it doesn't hand out
  ownership. Doc 02 uses this route.

### Enemy bullet impact

Mirrors the player-bullet path exactly:

* `EnemyBulletManager` grows a `struct Impact { float x, y; int tileId; };`
  and a `std::vector<Impact> m_impacts;` (currently only the
  live bullet vector exists).
* `EnemyBulletManager::update` calls `map->tilesets[...].isSolid`
  already; when `pointInSolid(...)` returns true, push an
  `Impact` with `tileId = layer->at(tx, ty)` before killing
  the bullet.
* `EnemyBulletManager::consumeImpacts(std::vector<Impact>&)`
  drains + returns `true` if any were drained (identical to
  `BulletManager::consumeImpacts`).
* `GameLayer::onUpdate`, right after `ebm.update(dt, ...)`,
  drains and dispatches:

```cpp
{
    std::vector<EnemyBulletManager::Impact> impacts;
    if (ebm.consumeImpacts(impacts)) {
        for (const auto& imp : impacts) {
            m_effects.spawnEnemyBulletImpact(imp.x, imp.y, imp.tileId);
        }
    }
}
```

Placement note: this must happen **before** the enemy-bullet-
vs-player-hurtbox loop, otherwise a bullet that dies on a wall
this frame would still be checked against the player hurtbox
by the next block. The Item 10 code already drains bullets in
`ebm.update`, so ordering is: `ebm.update` (bullets fly, some
die on walls, impacts recorded) -> drain + spawn impact FX ->
player-hurtbox check on the *survivors*.

### Blood splatter

The enemy-bullet-vs-player check today is:

```cpp
const int kbDir = (b.vx >= 0.0f) ? +1 : -1;
if (m_player.takeDamage(b.damage, kbDir)) {
    b.alive = false;
}
```

Item 12 inserts one line:

```cpp
const int kbDir = (b.vx >= 0.0f) ? +1 : -1;
if (m_player.takeDamage(b.damage, kbDir)) {
    m_effects.spawnBloodSplatter(b.x, b.y, kbDir);
    b.alive = false;
}
```

`spawnBloodSplatter(x, y, dir)` treats `dir` the same way the
existing muzzle flash treats it: `+1` = splatter cone to the
right (bullet continuing forward), `-1` = mirrored to the
left. Non-additive so blood doesn't glow.

Ghost mode: `Player::takeDamage` already early-outs in Ghost
mode, so this branch never runs and the splatter never
spawns. Zero new ghost checks required.

### Enemy death explosion timing

The explosion plays exactly once, on the *frame* HP drops to
zero. The existing death fade (Item 08) continues for
`deathFadeTime` seconds independently — the explosion is *not*
gated by the fade timer. So the visual sequence is:

```
t = 0     : takeDamage kills the enemy
             + m_dead = true; m_deathTimer = deathFadeTime;
             + m_justDied = true;
t = 0 (+1 tick) : EnemyManager::update sees justDied()
             + spawnEnemyExplosion(x, feetY + boxHalfH)
             + consumeJustDied() clears the latch
t = 0..deathFadeTime : sprite fades (existing Item 08 logic)
t = deathFadeTime : isFinished() -> erased from m_enemies
```

The explosion particles keep drawing (they live for their own
`lifetimeMin/Max`, typically ~0.5s) even after the sprite is
gone. That's the desired "sprite disappears in a burst"
reading.

---

## Golden rules (read before touching code)

1. **No engine edits.** Same rule as items 09–11. Every file
   lives under `G:\Dev\HBE\MegaX\`. Do not open a file under
   `HBE.Core\` or `HBE.Renderer.GL\`.
2. **Effects header hygiene stays** (Item 07 / 11 rule). Do
   NOT add `HBE/Renderer/ParticleSystem.h` to `Effects.h`.
   The new `make...` factories and spawn implementations
   belong in `Effects.cpp` where `ParticleSystem` is already
   fully included.
3. **`EnemyManager::m_effects` must be a raw non-owning
   pointer.** `Effects` is owned by `GameLayer`. Do not add a
   `std::unique_ptr<Effects>` or `shared_ptr` — you'll
   double-free at shutdown. The pointer being null is a valid
   state (before `setEffects` is called or after `GameLayer`
   is torn down): every dispatch site null-checks
   `if (m_effects)` before spawning.
4. **`justDied()` is a one-shot latch.** Do NOT set it on
   every frame the enemy is dead — that would spawn an
   explosion every frame during the fade. It's set inside
   `takeDamage` on the transition, and cleared by the manager
   on the frame it dispatches the explosion.
5. **Blood splatter only fires on a successful damage hit.**
   Check the return value of `takeDamage`. During i-frames or
   Ghost mode, `takeDamage` returns false — no splatter.
6. **Hit sparks are additive; blood splatter is NOT.** Enemy
   is a robot -> sparks are electric / hot. Player has flesh
   -> blood does not glow. This is baked into the
   `additiveBlend` field on the emitter configs; don't flip it
   in one place and forget the other.
7. **The muzzle flash direction uses the aim, not the enemy
   facing.** Enemies with `leadFactor > 0` (Challenging)
   sometimes aim slightly ahead of the player — the flash
   should follow the actual bullet trajectory, not the raw
   facing. Doc 02 uses `(aimX >= sx) ? +1 : -1;` which is
   robust to any aim vector.
8. **Do not spawn walk dust while dying.** The block in
   `EnemyManager::update` explicitly guards
   `if (!e.isAlive()) continue;` before the walk-dust call.
   Otherwise the enemy's death fade would look like a
   dust-storm.
9. **Enemy bullet impacts do NOT trigger blood splatter.**
   The impact drain runs *only* against tiles (via `pointInSolid`).
   Bullets that hit the player die in the player-hurtbox
   check, which spawns blood splatter — impact drain never
   sees them because they're already `!alive`.
10. **Every new registration name must be unique across the
    Effects registrations.** Grep `registerEffect(` in
    `Effects.cpp` — you should see exactly nine entries after
    Item 12 (`walk_dust`, `land_dust`, `muzzle_flash`,
    `bullet_impact`, `enemy_muzzle_flash`,
    `enemy_bullet_impact`, `enemy_explosion`, `hit_spark`,
    `blood_splatter`). No collisions with Item 07 names.

---

## Controls added in Item 12

**None.** Item 12 is purely presentational. Every existing
control (movement, jump, crouch, shoot, F1/F2/F3 difficulty,
G ghost, B debug overlay, R HP refill, F5 reload, F6 shader,
F7 respawn, H helmet) keeps its exact Item 11 behavior.

## What comes after this item

Item 12 is the end of the "combat feel" arc that started with
Item 06 (player shooting) and closed the loop with Items
08–10 (enemies + AI + difficulty). Everything from here is
either:

* New enemy types (each of which will register their own
  `<enemy>_explosion` / `<enemy>_muzzle_flash` in the same
  pattern as Item 12).
* New levels / map maker integration.
* Audio (Item 13+, probably).
* Boss patterns.

Item 11's F5 reload flow is what makes Item 12 tunable: the
color grading, particle counts, and lifetimes in doc 01 will
all be iterated on in tight seconds-long cycles. Take
advantage of it.

Next: `01_effects_new_registrations.md` — the five new
`make...` factories, the `registerEffect` wiring, and the
six new `spawn...` public methods on `Effects`.
