# 04 — Build, run, verify, tune

You've now touched every file listed in doc 00's table. This
final doc walks the build, the manual verify checklist for
every difficulty × sensor path, a difficulty tuning table, and
a troubleshooting matrix keyed off the most likely bugs.

---

## 1. Project file — add the new files

Item 10 introduces two new source files:

* `MegaX\include\Game\EnemyBullet.h`
* `MegaX\src\Game\EnemyBullet.cpp`

Add both to `MegaX.vcxproj` (mirroring how `Bullet.h/.cpp` are
listed). Open `G:\Dev\HBE\MegaX\MegaX.vcxproj` and add:

Under the existing `<ItemGroup>` that contains `<ClInclude ... />`
entries:

```xml
    <ClInclude Include="include\Game\EnemyBullet.h" />
```

Under the `<ItemGroup>` that contains `<ClCompile ... />` entries:

```xml
    <ClCompile Include="src\Game\EnemyBullet.cpp" />
```

Also mirror the entry in `MegaX.vcxproj.filters` if you keep it
in sync (the project builds either way — `.filters` only affects
the Solution Explorer tree).

**Asset copy:** `MegaX.vcxproj` still contains the
`<MegaXAsset Include="$(ProjectDir)assets\**\*" />` glob from
Item 07/08, so no extra copy rule is needed. Item 10 doesn't
add any assets.

---

## 2. Build

From a `vcvars64.bat`-initialized shell:

```
msbuild HonestlyBadEngine.slnx /p:Configuration=Debug /p:Platform=x64 /t:MegaX /m /nologo /v:m
```

Expected new lines:

```
  EnemyBullet.cpp
  Enemy.cpp
  EnemyManager.cpp
  Player.cpp
  GameLayer.cpp
  ...
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

Expected startup log lines:

```
[INFO ] MegaX GameLayer attached (Play mode; press G for Ghost).
```

The HUD should show 5 red pips top-left (or whatever
`Player::startHp` is) and a yellow "Difficult" pill top-right
with 2 black dots.

---

## 4. Verify checklist

Group A is unchanged Item 09 regression — those should still
work. Group B is Item 10 proper. Group C is difficulty swaps.

### Group A — Item 09 regression

* [ ] Patrol back-and-forth still works.
* [ ] `?` / `!` bubbles still appear.
* [ ] `B` still toggles hit/hurt overlay + vision cone + hearing
      ring.
* [ ] `G` still toggles Ghost.
* [ ] Standoff behavior in chase still holds the enemy at
      `shootingRange * standoffFrac`.
* [ ] Firing your gun still pings enemies via
      `notifyGunshot(...)`.
* [ ] 3 player hits still kill the soldier; sprite fades over
      ~0.6s.

### Group B — Item 10 combat

* [ ] Walk into the soldier's cone → after `alertLatchTime`, it
      begins firing. Bullets are red streaks that visibly angle
      slightly if fired from an elevated position (they aim at
      player, not straight forward).
* [ ] A bullet contacting the player hurtbox reduces HP by 1
      (Difficult profile). Player briefly flashes red, then a
      soft flicker plays for the rest of the i-frame window.
* [ ] During the flash + flicker, additional bullets pass right
      through (no double-damage).
* [ ] Enemy stops firing when player leaves cone (loses
      `m_lastSeen`).
* [ ] Player HP visibly drains to 0 across 5 hits (Difficult).
      Nothing crashes at 0; HUD reads 0 pips filled.
* [ ] Bullets die on hitting solid tiles (they don't punch
      through the ground).
* [ ] Killed enemies stop firing on death; bullets already in
      flight complete their arcs.

### Group C — Difficulty switching

* [ ] Press `F1` (**Casual**). HP refills. Pill turns green,
      shows 1 dot. Same enemy is noticeably slower to close,
      fires ~half as often, deals less damage per pip.
* [ ] Press `F2` (**Difficult**). Pill turns yellow, 2 dots.
      Behavior matches baseline.
* [ ] Press `F3` (**Challenging**). Pill turns red, 3 dots.
      Enemy sprints faster than you can walk away in a straight
      line. Fires much more often. If you strafe left/right,
      bullets clearly land where you're heading, not where you
      are.
* [ ] Switch difficulty mid-fight: enemy stats snap. Because we
      re-multiply from BASE, F1 → F3 → F1 lands back on Casual
      (idempotent, not compounding).
* [ ] Press `R`. HP refills without touching difficulty (green
      log line "HP refilled").

### Group D — Ghost + damage carve-out

* [ ] Press `G` while enemies are firing. Bullets keep flying,
      but nothing damages you. Pill/HP stay where they were
      before ghosting.
* [ ] Press `G` again to Play → damage resumes on the next
      bullet contact.

If Groups A + B + C + D all tick, Item 10 is done.

---

## 5. Tuning table — the flavor knobs

Everything here lives in `EnemyManager.cpp`'s `MakeProfile`
switch. Edit the number and re-build; there's no runtime UI for
tuning yet.

| Symptom | Field | Direction |
|---|---|---|
| Casual is *too* easy — enemy never catches you | `Casual.chaseSpeedMul` | `0.50 → 0.65` |
| Casual bullets tickle — want them to sting a little | `Casual.bulletDamage` | `1 → 1` (min); or increase `hurtFlashTime` to 0.35 for feel |
| Difficult still feels stationary | `Difficult.chaseSpeedMul` | `1.35 → 1.55` |
| Difficult fire rate too fast | `Difficult.fireCooldownSec` | `0.90 → 1.10` |
| Challenging leads too aggressively (you die instantly) | `Challenging.leadFactor` | `1.0 → 0.6` |
| Challenging aggro sticks *too* long (game feels unfair) | `Challenging.loseAggroDelayMul` | `2.0 → 1.5` |
| Enemies detect you across the whole map (Challenging) | `Challenging.sightRangeMul` | `1.35 → 1.15` |
| Player dies too fast on Challenging | `Player::startHp` | `5 → 7`; or `Player::invulnDuration` `0.55 → 0.8` |
| HP flash barely visible | `Player::hurtFlashTime` | `0.20 → 0.35` |
| Knockback launches you off ledges | `Player::knockbackImpulse` | `260 → 150` |
| Enemy shots hit ceiling above player | `Enemy::muzzleAboveFeet` | `26 → 32` (raise muzzle) |
| Muzzle inside player when standoff too tight | `Enemy::standoffFrac` | `0.5 → 0.6` (Item 09 tunable) |

Per-enemy overrides at spawn stay the same as Item 09:

```cpp
if (Enemy* e = m_enemies.spawn(ex, ey, -1)) {
    e->startHp        = 5;            // tougher designer default
    e->shootingRange  = 220.0f;       // long-shot rifleman
    e->setPatrolPath(ex - 128.0f, ex + 128.0f, 1.2f);

    e->snapshotBaseStats();                     // capture NEW base
    e->applyDifficulty(m_enemies.profile());    // re-multiply from new base
    e->spawn(ex, ey, -1);                       // re-snapshot HP to max
}
```

Order matters: any per-enemy override should happen **before**
`snapshotBaseStats()` + `applyDifficulty(...)` so the multiplier
lands on the intended base.

---

## 6. Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| Build: `error C2011 'HBE::Renderer::SpriteRenderer2D': class type redefinition` | You added `ParticleSystem.h`, `Scene2D.h`, `SpriteRenderer2D.h`, or `CombatSystem.h` to `Enemy.h`, `EnemyManager.h`, or `EnemyBullet.h`. | Move the include to the `.cpp`. See the `engine header conflict` memory. |
| Build: `error C2039 'DifficultyProfile': is not a member of 'MegaX'` in `Enemy.cpp` | Forgot `#include "Game/EnemyManager.h"` in `Enemy.cpp`. | Add the include. The `struct DifficultyProfile;` forward decl in `Enemy.h` is only enough for `Enemy.h` itself. |
| Build: `error C2440 lambda -> FireFn` | Non-capturing lambda to function pointer conversion is not automatic in some MSVC flags. | Prefix with `+`: `e.setFireCallback(+[](...){...}, this);` |
| Build: `error C2280 attempting to reference a deleted function` on `EnemyManager` | Compiler needed a defaulted destructor for `unique_ptr<EnemyBulletManager>` where `EnemyBulletManager` is forward-declared only. | Declare `EnemyManager() = default;` and `~EnemyManager();` in the header; `= default` the destructor in the .cpp (doc 01 §2c). |
| Runtime: enemies never fire | `m_fireFn` is null. | Confirm `EnemyManager::spawn` calls `e.setFireCallback(...)`. |
| Runtime: enemies fire twice a frame | You called `tickShooting` in `tickChase` *and* somewhere else. | It should only be called from `tickChase` (or from a future `Hold` state). |
| Runtime: bullets spawn behind the enemy | `muzzleWorldPos` multiplied by facing twice, or the sign is inverted. | Only `m_x + m_facing * muzzleForwardX` — no other muls. |
| Runtime: switching F1 → F3 → F1 gives different stats than the first F1 | `applyDifficulty` used current stats as the base, so it compounds. | It must read from `m_baseChaseSpeed` etc., not from `chaseSpeed`. See doc 01 §4. |
| Runtime: player takes damage while ghosting | Missed the ghost early-out. | Confirm `Player::takeDamage` starts with `if (m_mode == Mode::Ghost) return false;`. |
| Runtime: player HP goes negative | `takeDamage` uses `m_hp -= amount` without `std::max(0, ...)`. | Ensure the clamp is present. |
| Runtime: bullet passes through wall on the same tile the player is standing on | `pointInSolid` reads `map->tilesets[layer->tilesetIndex]` but the layer's ground uses a different tileset. | Confirm you passed the same `m_ground` layer to `ebm.update(...)` that the player collides with. |
| Runtime: hurt flash never appears | `m_hurtFlashTimer` is reset to 0 in `Player::updatePlay` every frame. | The timer decay lives at the *top* of `Player::update`, before the mode switch. `updatePlay`/`updateGhost` never touch it. |
| Runtime: HUD scrolls with the world instead of sticking to the screen | You used raw `pipX = 20.0f` in world space. | HUD X/Y must anchor to `cam.x - halfW + margin`. |
| Runtime: Challenging bullets pull *backwards* when the player runs away | The lead computation used `m_facing` instead of `player.velX()` (or you inverted the sign). | Lead formula: `aimX = player.x() + player.velX() * flightTime * leadFactor;` — no sign flip. |
| Runtime: enemy fires while dying | `Enemy::tick` doesn't early-out on `m_dead` before entering `tickShooting`. | `Enemy::tick` (Item 09) already has `if (m_dead) { ... return; }` — verify the block wasn't removed. |
| Bullets look black instead of red | `m_material.texture` bound to a wrong resource. | `EnemyBulletManager::init` reuses `"megax_white1x1"`. Confirm the tint is applied at `m_item.tint = Color4{1,0.35,0.35,1}`. |
| Enemy holds fire even in sight | `shootingRange` was scaled to 0 by `sightRangeMul` (they're separate fields but easy to conflate). | Only `sightRange` scales; `shootingRange` is a separate designer default. Check `MakeProfile` you didn't reuse `sightRangeMul` for shooting. |
| HUD pill sits below the top edge of the view | Camera height is off; you likely used `viewportHeight` without dividing by zoom. | Formula uses `halfH = viewportHeight / (2 * zoom)`. |
| Enemies fire before Alert latch time completes | `tickShooting` is called from `tickAlert` accidentally. | It should only be inside `tickChase`. |
| Difficulty change resets HP but leaves i-frames counting down | `refillHp()` only touches HP, not `m_invulnTimer`. | The provided `refillHp()` also zeros `m_hurtFlashTimer`. If you want to also clear i-frames, add `m_invulnTimer = 0.0f;`. |

---

## 7. What Item 11 will add

Item 11 will:

* Wire a scene-reload hotkey (`F5` per Sandbox convention) that
  clears enemies + bullets + effects, re-runs `GameLayer::onAttach`
  cleanly.
* Persist a scratch save of the current difficulty across
  reloads so tuning stays sticky.
* Add a Level Select overlay (optional stretch).

Everything from Item 10 stays; Item 11 is purely orthogonal
lifecycle plumbing.

Item 12 then layers enemy particle FX (muzzle flashes, casings,
hit sparks, death explosion, blood splatter) onto the combat
loop you just built.

Have fun. Difficulty selection is the moment the game starts
feeling like a *game* instead of a tech demo.
