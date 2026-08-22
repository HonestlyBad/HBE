# 04 — Build, run and verify

At this point Items 08 → 09 have replaced `Enemy.h`, `Enemy.cpp`,
`EnemyManager.h`, `EnemyManager.cpp`, and touched three places
in `GameLayer.cpp`. This doc walks the build, then the manual
verify checklist you should run through end-to-end.

---

## 1. Project file — nothing to add

Unlike Item 07/08, Item 09 does **not** require a
`MegaX.vcxproj` edit. Look at line 140:

```xml
<MegaXAsset Include="$(ProjectDir)assets\**\*" />
```

Every file under `MegaX\assets\` — including the new
`SoldierFordward_Spritesheet.png` — is auto-copied to
`$(TargetDir)\assets` in the post-build step. If you keep the
file in place, the build will pick it up automatically.

Confirm the file exists at:

```
G:\Dev\HBE\MegaX\assets\sprites\Enemies\RobotSoldier\SoldierFordward_Spritesheet.png
```

(256×256, 4c × 4r × 64×64. We use row 0 only.)

No `.filters` edits either.

---

## 2. Build

From a `vcvars64.bat`-initialized shell:

```
msbuild HonestlyBadEngine.slnx /p:Configuration=Debug /p:Platform=x64 /t:MegaX /m /nologo /v:m
```

Expected output:

```
  Enemy.cpp
  EnemyManager.cpp
  GameLayer.cpp
  ...
  MegaX.vcxproj -> G:\Dev\HBE\MegaX\bin\x64\Debug\MegaX.exe
  MegaX: copying runtime deps to G:\Dev\HBE\MegaX\bin\x64\Debug\
```

One pre-existing warning is expected (`C4099 TileMap: class/struct
forward-decl mismatch in Scene2D.h` from Item 07). Anything else
is new and should be fixed before proceeding.

Common build-error hits and their fixes are collected in the
troubleshooting section at the bottom of this doc.

---

## 3. Run

```
G:\Dev\HBE\MegaX\bin\x64\Debug\MegaX.exe
```

Look for a startup log line like:

```
[INFO ] MegaX GameLayer attached (Play mode; press G for Ghost).
```

No `Enemy::init: failed to load ...` errors should appear —
those mean the walk sheet path is wrong.

---

## 4. Verify checklist

Run through these top-to-bottom. Every item is a distinct scenario
that flexes a different part of the state machine.

### Patrol & animation

* [ ] **Patrol back-and-forth.** With no player input, the
  soldier walks between its two patrol X points, pauses at each
  end, turns around cleanly. Walk cycle plays only while moving;
  Idle plays while paused.
* [ ] **Wall turnaround.** Place / stand near a solid pillar in
  the patrol range; the soldier stops one tile short of it and
  waits/flips.
* [ ] **Ledge turnaround.** Set the patrol range wider than the
  platform; the soldier does NOT walk off the ledge — it treats
  the edge the same as a wall.

### Ghost bypass

* [ ] **Press `G` (Ghost).** Walk right up to the soldier from
  behind AND from in front. No "?", no "!", no reaction. Patrol
  continues.
* [ ] **Toggle `G` off mid-approach.** As soon as you're back in
  Play, if you're inside the sight cone the "!" pops within
  ~200 ms and Chase begins.

### Hearing

* [ ] **Walk up from behind (Play mode).** "?" appears; the
  soldier stops and turns to face you. If you keep walking so
  you enter the cone, it upgrades to "!" and chases.
* [ ] **Crouch-walk (`S` held) up from behind.** No reaction —
  crouching kills the hearing gate.
* [ ] **Stand still 5 px away.** No reaction — the velocity gate
  requires you to be moving.
* [ ] **Fire a bullet within ~380 px of a patrolling soldier.**
  Even from behind and behind cover, "?" pops immediately
  (gunshot ping bypasses the crouch/velocity gates).

### Sight cone

* [ ] **B toggle on.** Yellow arc + two edges show the vision
  cone; blue ring shows the hearing radius. Both anchored to
  the enemy.
* [ ] **Walk into the cone at long range.** "!" fires as soon as
  LOS is clear.
* [ ] **Duck behind a full solid tile column.** The cone still
  paints but no "!" — LOS raycast is blocked.
* [ ] **Crouch behind cover for 3 s during Chase.** Enemy exits
  Chase, enters Search, walks to your last-known X, dwells, then
  Returns to patrol. Bubble transitions "!" → "!" (Search) →
  none (Return complete).

### Chase behavior

* [ ] **Chase across a small gap.** Enemy runs off the ledge
  (edge detection is off during Chase) and either lands or jumps
  the gap depending on how the terrain is laid out.
* [ ] **Chase up a platform.** When the player is above and the
  enemy is grounded, the enemy jumps. Cooldown prevents
  jump-spam (you should see a hop-hop pattern, not a chatter).
* [ ] **Chase then Ghost.** Toggle `G` mid-chase — enemy drops
  to Search of your last position, walks over there, then heads
  home.

### Death (Item 08 regression)

* [ ] **Land 3 bullets.** Red flash on each hit, sprite fades
  out over ~0.6 s, then disappears. No crash, no lingering hurtbox.
* [ ] **Kill during Chase.** Enemy stops moving on death; the
  fade-out plays; bubble disappears immediately (not after
  fade).

If all 17 boxes tick, Item 09 is done. If any box fails, jump
to the matching row in the troubleshooting section below.

---

## 5. Tuning table

Every knob lives on the `Enemy` instance as a public field —
edit at spawn site (`e->fieldName = value;`) without recompiling
`Enemy.cpp`.

| Symptom | Field | Direction |
|---|---|---|
| Patrol too jittery near ends | `patrolWaitAtEnd` | `1.0f` → `1.5f` |
| Hearing triggers too far away | `hearingRadius` | `140` → `100` |
| Hearing triggers on tip-toe walking | `minPlayerVxToHear` | `40` → `70` |
| Vision cone too tight (misses you) | `sightHalfAngleDeg` | `35` → `45` |
| Vision cone reaches too far | `sightRange` | `260` → `180` |
| Sight peeks through thin walls | `losStepPx` | `12` → `6` |
| "?" bubble fades too fast | `suspicionDuration` | `1.2` → `2.0` |
| Aggro clings even after cover | `loseAggroDelay` | `3.0` → `1.8` |
| Enemy jumps into ceiling | `jumpSpeed` | `520` → `440` |
| Enemy hops constantly on flat ground | `chaseJumpCooldown` | `0.5` → `0.9` |
| Chase feels too slow to catch up | `chaseSpeed` | `120` → `160` |
| Gunshot pings enemies across the map | `gunshotHearRadius` | `380` → `220` |

Per-enemy overrides at spawn site:

```cpp
if (Enemy* e = m_enemies.spawn(ex, ey, -1)) {
    e->startHp        = 5;        // tougher
    e->sightRange     = 320.0f;   // eagle-eyed
    e->chaseSpeed     = 90.0f;    // slow but persistent
    e->loseAggroDelay = 5.0f;
    e->setPatrolPath(ex - 128.0f, ex + 128.0f, 1.2f);
    e->spawn(ex, ey, -1);         // re-snapshot with new tunables
}
```

---

## 6. Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Build error `C2011 'HBE::Renderer::SpriteRenderer2D': 'class' type redefinition` | Added `ParticleSystem.h`, `Scene2D.h`, or `CombatSystem.h` to `Enemy.h`/`EnemyManager.h`. | Move the include to the `.cpp`. See `07-Particle-Effects/01_effects_configs_and_class.md` for the exact trap. |
| Build error `use of undeclared identifier 'Player'` in Enemy.cpp | Forgot to `#include "Game/Player.h"` in Enemy.cpp (only forward-declared in the header). | Add the include at the top of `Enemy.cpp`. |
| Build error `class HBE::Renderer::DebugDraw2D` not found | Same C++ elaborated-name-specifier trap we hit in Item 08. | Add `class DebugDraw2D;` to the `namespace HBE::Renderer` forward-decl block in `EnemyManager.h`. |
| Build error `no member 'bullets' in class BulletManager` | Item 08's Bullet-public patch got reverted. | Re-apply the change in `08-First-Enemy/02_enemy_manager_and_bullet_integration.md`. |
| Runtime crash on first tick / `nullptr` deref inside `Enemy::tick` | `setPlayerRef` or `setCollision` never called. | Confirm both `setPlayerRef(&m_player)` and `setCollision(m_world.map(), m_ground)` run in `GameLayer::onAttach` BEFORE `spawn(...)`. |
| Enemy floats mid-air | `spawn` was called before `setCollision`, so the physics body has no map ref. | Ensure `setCollision` runs before `spawn` (the manager forwards to `Enemy::setCollision` inside `spawn` — see `EnemyManager::spawn`). |
| Enemy sinks into the ground | Feet vs box mismatch. Confirm `boxHalfH == 20.0f` and `spriteFeetOffsetY == 0.0f`. | Tweak `boxHalfH` up until the sprite's feet visibly touch the tile line. |
| Enemy never turns around during patrol | `patrolLeftX == patrolRightX`, OR the whole range is behind a solid tile. | Call `setPatrolPath(leftX, rightX)` with `rightX > leftX` and both ends above walkable ground. |
| "?" appears when I'm crouching quietly | `pb.h < 34.0f` threshold is too tight for your Player crouch box. | Log `player.hurtbox().h` while crouching; adjust the threshold in `Enemy::hearsPlayer`. |
| "!" appears through a wall | LOS step is too coarse. | Drop `losStepPx` from 12 to 6. |
| Enemy chases through Ghost | Missed the ghost check in `tickChase`. | Confirm `if (player.mode() == Player::Mode::Ghost) { enter(AIState::Search); return; }` at the top of `tickChase`. |
| Vision cone paints on the wrong side | `m_facing` was scaled twice. | The cone uses `baseTh = (facing >= 0 ? 0 : PI)`; make sure you didn't multiply the whole angle by `m_facing`. |
| Bubble sits under the sprite | The `+ 60.0f` in `renderBubbles` is too small for your character height. | Bump the `top` offset to 72 or 80. |
| Enemy walk animation is choppy | 4 frames at 10 fps isn't enough; the Fordward sheet is really a 16-frame cycle. | Optional stretch: build a `MultiRowSpriteAnimation` that steps col 0..3 then wraps to next row 0..3. Not required for Item 09. |
| Multiple enemies all react as one | Sensing has no per-enemy timers (`m_stateTimer` etc). | Every field in the state machine is per-enemy (they live on the Enemy struct, not statics). If you copied-pasted and left something `static`, remove `static`. |

---

## 7. What to look at next

Item 10 (`10-Enemy-Combat-AI-and-Difficulties`) will:

* Turn `m_hitboxActive` on and let the enemy actually damage the
  player.
* Add the shooting cadence (finally using `m_alertLatch`).
* Introduce a `Difficulty` scalar in `EnemyManager` that
  multiplies `chaseSpeed`, `sightRange`, `startHp`,
  `loseAggroDelay`, `hitDamage`.
* Extend the shooting to lead player velocity on Challenging.

Everything you built in Item 09 stays; Item 10 is purely
additive.

Have fun — this is the item where the game starts to feel alive.
