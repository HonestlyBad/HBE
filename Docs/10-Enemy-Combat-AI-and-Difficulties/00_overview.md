# 10 — Enemy Combat AI with Difficulty (overview)

Item 09 gave the Robot Soldier its brain: it patrols a segment,
sees & hears the player, chases them, and holds a **standoff
distance** (half of `shootingRange`) once it closes in. Item 10
finally lets it *fire*, gives the player HP so those bullets
mean something, and layers a **difficulty system** on top that
scales enemy speed, aggression, damage, health and shooting
behavior across three modes:

| Difficulty  | Vibe |
|---|---|
| **Casual**      | Half the player's move speed, light damage, quick to lose aggro, harder to detect the player. |
| **Difficult**   | A little faster, aggros easier, hits harder, more health, longer aggro leash. |
| **Challenging** | Faster than the player, shoots more often, **leads** the shot to where the player is *going*, aggro is a bear to shake. |

The player gets a small HP pool and takes damage from enemy
bullets (and, when hit, briefly flashes + is invulnerable for a
fraction of a second to prevent multi-frame chip damage).

Ghost mode remains a hard carve-out for enemy sensing (Item 09
guarantee), and now also for damage: enemy bullets pass through
the player while ghosting.

---

## Goals

* Introduce an `EnemyManager::Difficulty` enum {`Casual`,
  `Difficult`, `Challenging`} and a `DifficultyProfile` struct
  of multipliers. `EnemyManager::setDifficulty(...)` applies
  the profile to every enemy currently in the manager, and to
  every future `spawn(...)`.
* Enemy fires while in **Chase** state whenever the player is
  within `shootingRange`, LOS is clear, and its
  `m_fireCooldown` has elapsed. Rate of fire, damage and
  bullet speed all scale with the difficulty profile.
* Introduce `EnemyBulletManager` (a slimmer variant of
  `BulletManager`) so we can distinguish enemy bullets from
  player bullets at bookkeeping time (no self-hits, different
  tint, different visuals down the road). Enemy bullets:
  * die on solid tiles (same tilemap layer the player uses).
  * die on hitting the player hurtbox.
  * pass through in Ghost mode.
* Add `Player::takeDamage(int)`, `Player::hp()`,
  `Player::isInvulnerable()`, a short i-frame + hurt flash, and
  a tiny knockback impulse.
* **Challenging** enemies **lead** the player: their aim point
  is `player.x() + player.velX() * timeToImpact` where
  `timeToImpact ≈ distance / enemyBulletSpeed`.
* Hotkeys `F1` / `F2` / `F3` switch difficulty at runtime; the
  currently-active difficulty is shown as a colored bar in the
  top-right of the HUD along with the player HP bar in the
  top-left. Both are drawn via `DebugDraw2D` anchored to the
  camera (no new engine text infrastructure required).
* Item 09's standoff behavior in `tickChase` **stays**. The
  enemy still holds `shootingRange * standoffFrac` away from
  the player and now uses the extra breathing room to shoot.

## Non-goals (deferred)

| Behavior | Deferred to |
|---|---|
| Enemy death explosions + spark hit effects | 12 — Enemy particle effects |
| Blood splatter on player hit | 12 |
| Muzzle flash / bullet casings for enemies | 12 |
| Difficulty screen / menu UI | Later polish item |
| Per-map difficulty override in the map JSON | Later map integration |
| Multi-shot / laser boss patterns | Boss items |
| Data-driven enemy stat sheets | Later data pipeline |

Item 10 stays inside the game project. **Zero engine edits.**

---

## What "done" looks like

You'll know Item 10 is finished when:

1. Fresh scene, default difficulty is **Difficult**. The HP bar
   in the top-left reads full (5 hearts / 5 pips). The
   difficulty pip in the top-right is yellow.
2. Wander into the soldier's cone → `!` bubble → Chase → soldier
   stops at standoff distance (half `shootingRange`), fires a
   glowing red bullet at you every ~0.9 s.
3. Take a bullet → HP drops by 1, screen flashes red briefly,
   player is invulnerable for ~0.5 s (subsequent bullets pass
   through the flash). Small horizontal knockback impulse.
4. Press `F1` (**Casual**). HP bar refills. Difficulty pip
   turns green. Same soldier now feels much slower, shoots less
   often, deals less damage per pip, gives up chase quickly.
5. Press `F3` (**Challenging**). Difficulty pip goes red. The
   same soldier now sprints faster than you, shoots twice as
   fast, and if you're moving, its bullets **lead you**
   (visible: fire path lands where you *would* be, not where you
   are). Cover behind a wall for a few seconds and it *still*
   waits nearby before returning to post.
6. Press `G` (Ghost) mid-fight → all enemy bullets stop damaging
   you (they still hit walls and expire). You reappear at full
   HP once you toggle back to Play (see doc 03 § ghost policy).
7. Kill the soldier with 3 player shots (unchanged from Item
   09). The soldier stops firing on death; bullets already in
   flight continue to their natural end.
8. Press `B` → hitbox overlay now shows an additional red
   segment for the enemy's *shooting hitbox* (small box at the
   muzzle tip that spawns bullets). This is purely diagnostic;
   contact damage from body slams is still off — damage only
   comes from bullets in Item 10.

## Files touched (all game-side)

| File | Item 09 | Item 10 |
|---|---|---|
| `include/Game/Enemy.h` | AI state, sensors, standoff | + shooting tunables, `fireCooldown`, `applyDifficulty(profile)`, `muzzleWorldPos()` |
| `src/Game/Enemy.cpp` | patrol / sense / chase / search | + `tickShooting()` inside Chase, calls a callback to spawn enemy bullets |
| `include/Game/EnemyManager.h` | manager + gunshot ping | + `Difficulty` enum, `DifficultyProfile`, `setDifficulty()`, `enemyBullets()` |
| `src/Game/EnemyManager.cpp` | manager loops | + owns `EnemyBulletManager`, calls it from `update()`, applies difficulty, checks player hit |
| `include/Game/EnemyBullet.h` | *new* | thin bullet struct + manager (mirrors `Bullet.h`) |
| `src/Game/EnemyBullet.cpp` | *new* | update / render / cull, plus optional lead prediction stored per-bullet |
| `include/Game/Player.h` | movement + shooting | + `startHp`, `m_hp`, `takeDamage`, `isInvulnerable`, `hp()` accessor |
| `src/Game/Player.cpp` | movement + shooting | + i-frames, hurt flash tint, knockback impulse |
| `src/Game/GameLayer.cpp` | senses + bubbles wired | + difficulty hotkeys F1/F2/F3, HP + difficulty HUD via `DebugDraw2D`, enemy-bullet update/render/check-hit |
| `include/Game/GameLayer.h` | manager fields | + `Difficulty m_difficulty` cached |
| `MegaX.vcxproj` / `.filters` | — | add `EnemyBullet.h` / `.cpp` |

Zero engine files.

---

## Design shape (short version)

### Difficulty scaling

`DifficultyProfile` is a plain struct of float multipliers plus
a few raw values:

```
struct DifficultyProfile {
    float chaseSpeedMul     = 1.0f;
    float sightRangeMul     = 1.0f;
    float hearingRadiusMul  = 1.0f;
    float loseAggroDelayMul = 1.0f;

    float startHpMul        = 1.0f;   // scales enemy HP
    int   bulletDamage      = 1;      // raw hit damage per enemy bullet
    float fireCooldownSec   = 0.90f;  // seconds between shots while chasing
    float bulletSpeed       = 480.0f; // px/s
    float leadFactor        = 0.0f;   // 0=no lead, 1=full velocity-lead
};
```

Casual / Difficult / Challenging return three baked profiles.
`EnemyManager::setDifficulty(d)` stores it, then walks every
enemy and calls `e.applyDifficulty(profile)` — which
re-multiplies every relevant field from a saved *base* stat
snapshot (so switching difficulty is idempotent, not
compounding).

### Enemy shooting

Inside `Enemy::tickChase`, after the standoff-hold logic Item 09
introduced, we run a compact shooting subroutine:

```
if (m_fireCooldown > 0) m_fireCooldown -= dt;
if (m_lastSeen && withinRange(player, shootingRange) && m_fireCooldown <= 0) {
    fire(player, spawnCallback);
    m_fireCooldown = fireCooldownSec;
}
```

`fire(...)` invokes a manager-owned callback: `spawnEnemyBullet(
sx, sy, aimX, aimY, damage, speed)`. The manager owns the
enemy-bullet vector; the enemy is decoupled from bullet storage
(same shape as player shooting in Item 06).

### Lead prediction

Only Challenging uses `leadFactor > 0`. The aim target used by
`fire(...)` is:

```
predX = player.x() + player.velX() * (dist / bulletSpeed) * leadFactor;
predY = player.y();   // vertical lead not attempted in this item
```

`predY` stays flat (2D shooter) — leading vertical arcs is a
later item once we have proper projectile arcs.

### Player HP + i-frames

Player HP is a plain int (`m_hp`), maxed at `startHp = 5`.
`takeDamage(amount)` early-outs if `m_invulnTimer > 0` or if
`m_mode == Mode::Ghost`. On a legal hit:

```
m_hp -= amount;
m_invulnTimer = invulnDuration;   // ~0.5s
m_hurtFlashTimer = hurtFlashTime; // tints the sprite red for a bit
m_vx += knockbackDir * knockbackImpulse;
```

The HUD reads `m_hp` and `startHp` each frame and draws N pips.
No new engine calls.

---

## Golden rules (read before touching code)

1. **No engine edits.** Every file lives under
   `G:\Dev\HBE\MegaX\`. If you catch yourself opening a file in
   `G:\Dev\HBE\HBE.Core\` or `G:\Dev\HBE\HBE.Renderer.GL\`,
   stop and re-scope.
2. **Enemy header hygiene stays** (Item 09 rule 2). Do NOT add
   `ParticleSystem.h`, `Scene2D.h`, `CombatSystem.h`,
   `SpriteRenderer2D.h`, or `EnemyBullet.h`'s implementation-only
   includes to `Enemy.h`/`EnemyManager.h`. The new
   `EnemyBullet.h` is thin — only include it where actually
   used.
3. **Ghost bypass is unchanged and now covers damage too.**
   Every damage code path (player takeDamage, enemy hit vs
   player hurtbox) checks `player.mode() == Ghost` before
   applying damage. One guard per code path — do not scatter
   `if (ghost)` throughout gameplay.
4. **Standoff behavior in `tickChase` stays.** Item 10 layers a
   shot-cooldown on top; it does *not* rewrite Item 09's
   movement rules. If you notice standoff code disappearing
   during your edits, you deleted too much.
5. **Enemy bullets never damage other enemies.** No friendly
   fire in Item 10. Bullets only collide with the tilemap and
   the player hurtbox.
6. **Difficulty is applied to a BASE snapshot, not the current
   value.** Each `Enemy` caches its "designer" stats
   (`m_baseChaseSpeed`, `m_baseSightRange`, etc.) at spawn
   time; `applyDifficulty(profile)` computes
   `current = base * mul`. Otherwise F1→F3→F1 would compound
   into wrong values.
7. **HUD lives in world space, anchored to the camera.**
   `DebugDraw2D::rect` is world-space at layer 9000. To place
   the HP bar in the top-left of the viewport, compute
   `hudX = cam.x - halfW + margin`. No screen-space RenderPass
   work needed.

---

## Controls added in Item 10

| Key | Effect |
|---|---|
| `F1` | Switch to **Casual** difficulty; refills player HP as a courtesy |
| `F2` | Switch to **Difficult** difficulty; refills HP |
| `F3` | Switch to **Challenging** difficulty; refills HP |
| `R`  | (Dev shortcut) Instantly refill player HP without changing difficulty. Handy while tuning. Chosen over `H` because `H` already toggles the helmet (Item 01). |

All existing controls (WASD, SPACE, S crouch, G ghost, B debug,
LMB fire) behave the same.

## What comes after this item

Item 11 is scene reload / hot-reload wiring so you can iterate
on tuning without restarting the app. Item 12 layers enemy
particle effects (muzzle flashes, casings, hit sparks, death
explosion, blood splatter on the player) onto the combat loop
you just built.

Next: `01_difficulty_profile.md` — the `Difficulty` enum,
`DifficultyProfile` struct, `EnemyManager` plumbing, and the
`Enemy::applyDifficulty` machinery + base-stat snapshot.
