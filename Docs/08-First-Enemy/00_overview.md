# Item 08 — First Enemy with Hit Boxes and Health

## Goal

At the end of this item you should be able to:

* Load the **Robot Soldier** sprite sheet(s) from
  `assets/sprites/Enemies/RobotSoldier/`.
* Place an enemy in the world that plays its **idle** animation.
* Shoot it with the player's bullets. Each bullet deducts damage
  from the enemy's HP; when HP reaches zero the enemy **fades out**
  over ~0.6 s (the explosion particle burst is deferred to Item 12).
* Toggle a **hit / hurt box debug overlay** on and off with the
  `B` key.

Explicit non-goals (belong to later items):
* Enemy movement / walking / patrolling — Item 09.
* Enemy shooting or damaging the player — Item 10.
* Death explosion particles, player damage feedback — Item 12.
* Reload-scene wiring — Item 11.

---

## Golden rules

These rules match how Items 01–07 were built and must not be
violated while implementing Item 08:

1. **Do not touch any engine files** (`HBE.Core`, `HBE.Platform.SDL`,
   `HBE.Renderer.GL`). Every change is inside `G:\Dev\HBE\MegaX\`.
2. **Do not add the enemy to the ECS.** The engine ships a full
   ECS combat stack (`CombatComponents.h`, `CombatSystem.h`,
   `CombatEvents.h`) but the rest of MegaX (`Player`,
   `BulletManager`, `Effects`) does not use it. Following that
   convention, the Robot Soldier is a plain C++ class owned by an
   `EnemyManager`, mirroring how `BulletManager` owns `Bullet`.
   The engine's ECS combat pieces will be adopted during your
   future "mass engine update" pass.
3. **The `Effects.h` pimpl workaround still applies.** Any TU that
   pulls in both `Player.h` (→ `Sprite2D.h`) and `Scene2D.h`
   (→ `SpriteAnimationStateMachine.h` → `SpriteRenderer2D.h`) will
   fail with `C2011 SpriteRenderer2D redefinition`. This includes
   any TU that includes `ParticleSystem.h`, `Scene2D.h`, or
   `CombatSystem.h`. `Enemy.h` and `EnemyManager.h` therefore
   include `Sprite2D.h` (same as `Player.h`) and **must not**
   include ECS/scene headers.
4. **Do not modify `level_01.json`** to add an enemy spawn. For
   Item 08 the spawn point is a hard-coded world coordinate inside
   `GameLayer::onAttach`. Data-driven spawn definitions are their
   own future work item.

---

## High-level design

```
        +----------------+        +---------------------+
        |  GameLayer     |        |  EnemyManager       |
        |                | -----> |  m_enemies: vector  |
        |                |        |    <Enemy>          |
        +-------+--------+        +---------+-----------+
                |                            |
                | onUpdate                   | update / render
                |                            v
                v                    +-----------------+
        +----------------+           |     Enemy       |
        |  BulletManager | <-------- |  hp / hurtbox   |
        |  bullets()     |  hit-test |  facing / anim  |
        +----------------+           |  deathTimer     |
                                     +-----------------+
                        \                    /
                         \                  /
                          v                v
                        +--------------------+
                        |  Effects           |
                        |  spawnBulletImpact |
                        +--------------------+
```

Data flow every frame:

1. `Player.update()` publishes any new shot.
2. `BulletManager.spawn()` records it.
3. `BulletManager.update()` advances bullets, kills on tile impact,
   and appends world-space impact points to its `Impact` queue.
4. **NEW:** `EnemyManager.checkBulletHits(bullets, effects)`
   iterates alive bullets, tests each bullet point against every
   alive enemy's hurtbox, and on hit:
   * subtracts `Enemy::damagePerHit` from the enemy's HP,
   * kills the bullet (`Bullet::alive = false`),
   * asks `Effects` for a bullet-impact burst at the hit point
     (using `tileId = 0` so it uses the fallback tan tint —
     Item 12 will swap this for a spark burst).
5. `EnemyManager.update(dt)` ticks per-enemy state:
   * animation timer,
   * invulnerability window after a hit,
   * death fade timer.
6. `EnemyManager.render(r2d)` draws each enemy.
7. If `m_showHitboxes` is true, `GameLayer::onRender` uses
   `DebugDraw2D::rect(..., filled=false)` to outline every
   hurtbox / hitbox in the scene.

---

## Effects list (for this item)

| Trigger                | Effect                                    | Notes                                    |
|------------------------|-------------------------------------------|------------------------------------------|
| Bullet hits enemy      | Existing `Effects::spawnBulletImpact`     | `tileId=0` → fallback tan tint for now.  |
| Enemy HP drops         | Brief red flash on the enemy sprite       | Tint `Color4{1, 0.4, 0.4, 1}` for 0.1 s. |
| Enemy HP reaches zero  | Fade sprite alpha 1 → 0 over `deathFade`  | No particle burst yet (Item 12).         |
| B key pressed          | Toggle `m_showHitboxes`                   | Emits a `LogInfo(...)` when toggled.     |

---

## New / modified files

New game-side files:

```
G:\Dev\HBE\MegaX\include\Game\Enemy.h
G:\Dev\HBE\MegaX\include\Game\EnemyManager.h
G:\Dev\HBE\MegaX\src\Game\Enemy.cpp
G:\Dev\HBE\MegaX\src\Game\EnemyManager.cpp
```

Modified game-side files:

```
G:\Dev\HBE\MegaX\include\Game\Bullet.h        (expose Bullet struct + bullets() getter)
G:\Dev\HBE\MegaX\include\Game\Player.h        (add hurtbox() getter)
G:\Dev\HBE\MegaX\include\Game\GameLayer.h     (m_enemies, m_debug, m_showHitboxes)
G:\Dev\HBE\MegaX\src\Game\GameLayer.cpp       (init/update/render/B toggle)
G:\Dev\HBE\MegaX\MegaX.vcxproj                (register new files)
G:\Dev\HBE\MegaX\MegaX.vcxproj.filters        (register + create Enemies filter)
```

No engine edits. No changes to `Effects.h/.cpp`, `Player.cpp`,
`Bullet.cpp` internals (only Bullet.h is touched).

---

## Controls added / changed

| Key | Action                                             | Introduced in |
|-----|----------------------------------------------------|---------------|
| `A` / `D`         | Move                                     | 01            |
| `S`               | Crouch (Play) / down (Ghost)             | 01 / 04       |
| `SPACE`           | Jump (Play) / up (Ghost)                 | 01 / 04       |
| `E`               | Fire                                     | 06            |
| `G`               | Ghost / Play toggle                      | 05            |
| `H`               | Helmet toggle                            | 01            |
| **`B`**           | **Hit / hurt box debug overlay toggle**  | **08**        |

---

## Asset notes

The sprite sheets you dropped in
`assets/sprites/Enemies/RobotSoldier/` were verified by alpha-band
scan:

| Sheet                          | Image size | Grid    | Frame size | Notes for Item 08                        |
|--------------------------------|------------|---------|------------|------------------------------------------|
| `SoldierIdle_Spritesheet.png`  | 256 x 128  | 4c × 2r | 64 × 64    | Only row 0 is used (4 frames).           |
| `SoldierFordward_Spritesheet.png` | 256 x 256 | 4c × 4r | 64 × 64  | Loaded but not played (Item 09).         |
| `SoldierFire_Spritesheet.png`  | 256 x 256  | 4c × 4r | 64 × 64    | Not loaded in Item 08 (Item 10).         |

Within every 64 × 64 cell the visible pixels sit in the **lower
half** (roughly rows 28–63). This matches Player's convention
where the sprite is anchored at the frame's bottom, so we can use
the same `posY = boxCy + (frameH − boxH) / 2` offset that Player
uses in `feetToCenterOffset()`.

The `SpriteAnimation` helper in `Sprite2D.h` only supports a
**single row**. For Item 08 the idle animation therefore plays the
first row of the Idle sheet (`cols 0..3, row 0`) at 6 fps. A
custom multi-row animator would be nice-to-have but is not needed
here.

---

## What "done" looks like

* Enemy is visible when the game starts, idling in place.
* Firing at it produces yellow-tinted bullet-impact bursts at the
  hit point.
* Each hit briefly flashes the enemy red.
* After the configured number of hits the enemy fades out over
  ~0.6 s, then disappears.
* Pressing `B` toggles a debug overlay that outlines the player's
  hurtbox in blue, each enemy's hurtbox in green (gray while
  fading out), and each enemy's hitbox in red (only when
  `Hitbox::active` is true — nothing to show in Item 08 because
  enemies don't attack yet).
* No engine files were modified.
* MegaX still builds clean and runs without a crash.

---

Next: `01_enemy_class.md` — the full `Enemy` source with idle
animation, HP, hurtbox, hitbox stub, and the fade-out timer.
