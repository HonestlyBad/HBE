# 09 — Enemy AI: Patrol & Player Detection (overview)

Item 08 got the Robot Soldier standing on the ground, taking
damage and dying. Item 09 gives it a **brain**: it patrols a
segment of ground, senses the player through hearing and sight,
and chases the player once alerted — with clean visual feedback
via a floating "?" (suspicious) or "!" (alert) bubble.

Ghost mode is a hard carve-out: while the player is in Ghost,
enemies act as if the player does not exist.

---

## Goals

* Enemy patrols between two configurable X points on its ground
  segment, waiting a tunable number of seconds at each end
  before turning around.
* Enemy has **two** senses:
  * **Hearing** — omnidirectional, radius-based. Triggers only
    when the player is *moving on foot* (walking or running,
    NOT crouching, NOT ghost).
  * **Sight** — forward cone in front of the enemy, blocked by
    solid tiles. Requires a clear line of tiles between the
    enemy's eye and the player.
* On hearing, the enemy enters **Suspicious** and shows a "?"
  bubble. It stops, turns to face the sound, and waits — if it
  actually *sees* the player during suspicion, it escalates.
* On sight (from any state), the enemy enters **Alert**,
  briefly shows a "!" bubble, then goes into **Chase**.
* Chase = enemy runs toward the player and jumps to follow when
  the player is above and there's an edge. **No shooting yet**
  — shooting is Item 10.
* If the player breaks line-of-sight AND crouches (or Ghost-toggles
  away) for `loseAggroDelay` seconds, the enemy loses aggro,
  enters **Search** (walks to the last known position and pokes
  around), then heads back to patrol.
* **Ghost mode bypass** — while `Player::mode() == Ghost`, every
  detection tick early-outs. Existing patrols continue running.
  This preserves the "invisible builder" ghost contract from Item 5.
* Player has no new controls. `B` still toggles hitbox/hurtbox
  overlay; in this item it also draws the vision cone and
  hearing ring so tuning is visual.

## Non-goals (deferred)

| Behavior | Deferred to |
|---|---|
| Enemy shooting the player | 10 — Combat AI + difficulties |
| Casual / Difficult / Challenging scaling | 10 |
| Enemy hurting the player on touch | 10 (hitbox becomes active) |
| Explosion / death particles | 12 — Enemy particle effects |
| Data-driven spawn/patrol from map JSON | Later (map integration) |
| Multi-row walk cycle (16-frame Fordward sheet) | Optional stretch — see doc 04 |

---

## The state machine

```
                ┌──────────────────────────────────────────┐
                │                                          │
                ▼                                          │
      ┌───────────────┐   hears player   ┌──────────────┐  │
      │    Patrol     │─────────────────►│  Suspicious  │──┘   suspicion ends
      │  (walk left/  │                  │   "?" over   │      w/o sighting
      │   right, wait │                  │    head      │─────────┐
      │   at ends)    │◄─────────────────│  face sound  │         │
      └───────┬───────┘  finished return └──────┬───────┘         ▼
              │                                 │ sees player     ┌─────────────┐
              │                                 ▼                 │             │
              │                          ┌──────────────┐         │   Return    │
              │       sees player        │    Alert     │         │  (walk back │
              └─────────────────────────►│  "!" over    │         │ to nearest  │
                                         │    head      │         │  patrol     │
                                         │   (latch)    │         │   endpoint) │
                                         └──────┬───────┘         └─────▲───────┘
                                                │ alertTimeToLatch      │
                                                ▼ elapsed               │
                                         ┌──────────────┐               │
                                         │    Chase     │               │
                                         │ run toward   │               │
                                         │ player, jump │               │
                                         │ over gaps    │               │
                                         └──────┬───────┘               │
                                                │ player hidden         │
                                                │ (crouched + no LOS)   │
                                                │ for loseAggroDelay    │
                                                ▼                       │
                                         ┌──────────────┐  search       │
                                         │    Search    │  dwell        │
                                         │ walk to last │  elapsed      │
                                         │ known pos    │───────────────┘
                                         └──────────────┘
```

Ghost carve-out: any sensing transition (Patrol→Suspicious,
Patrol→Alert, Suspicious→Alert, Chase-stay-latched) is skipped
while the player is in Ghost mode. State machine still ticks —
patrols, waits, and Return-to-post continue — but no aggro is
gained.

---

## Golden rules (read before touching code)

1. **No engine edits.** Same rule as items 06/07/08. Everything
   is in `G:\Dev\HBE\MegaX\`. Engine changes are batched later.
2. **Enemy header hygiene stays.** Do NOT add `#include
   "HBE/Renderer/ParticleSystem.h"`, `Scene2D.h`, or
   `CombatSystem.h` to `Enemy.h` / `EnemyManager.h`. Any of those
   pulls in a second `SpriteRenderer2D` definition (see the
   note in `07-Particle-Effects/01_effects_configs_and_class.md`).
   The list of allowed includes is unchanged from Item 08 plus
   the new `TileMap.h` (already transitively pulled by
   `TileCollision.h`).
3. **Ghost bypass is a top-of-tick check**, not a bunch of
   scattered guards. Every sensor helper takes the player by
   const ref; the sense-tick early-outs if
   `player.mode() == Player::Mode::Ghost`.
4. **Patrol is grounded.** Enemy uses feet-anchored physics via
   `TileCollision::moveAndCollideEx` (same helper Player uses).
   Off-the-ledge protection is implemented by sampling a probe
   point in front of the feet during Patrol only — Chase
   intentionally ignores edges (this is what makes the chase
   feel dangerous).
5. **Bubbles are drawn via `DebugDraw2D`.** Yes it's called
   "Debug" — it draws colored rects at layer 9000, which is
   exactly what we need for an always-on-top icon. When we get
   real "?" / "!" sprite assets later, swap the composition for
   a textured `RenderItem`.
6. **`SpriteAnimation` is single-row.** Walk uses row 0 of the
   Fordward sheet, 4 frames at ~10 fps. See doc 04 for the
   optional multi-row upgrade if the cycle feels choppy.

---

## Controls

| Key | Effect |
|---|---|
| `A`/`D` | Move (unchanged) |
| `SPACE` | Jump (unchanged) |
| `S` | Crouch (unchanged) — **also stops the enemy hearing you** |
| `G` | Toggle Ghost / Play (unchanged) — **also disables all detection** |
| `B` | Toggle hit/hurt overlay (unchanged) — now **also** draws vision cone + hearing ring |
| `LMB` / `Ctrl` | Fire (unchanged) — a gunshot **broadcasts a hearing ping** through the enemy manager, alerting nearby enemies regardless of your crouch state (see doc 03 § gunshot ping) |

---

## Sprite / asset notes

Only new asset in Item 09:

* `assets/sprites/Enemies/RobotSoldier/SoldierFordward_Spritesheet.png`
  — 256×256, 4 columns × 4 rows × 64×64 cells (all 16 cells
  populated). We use **row 0, cols 0–3** at 10 fps for the walk
  cycle. If motion looks stuttery, bump `kWalkFps` to 12 or try
  a different row via the constants in `Enemy.cpp`; the doc
  points at exactly which line to touch.

The Idle sheet loaded in Item 08 is reused for stand-still frames
(patrol wait, suspicion, alert). No re-declaration needed — the
ResourceCache is name-keyed, so `Enemy::init` just requests the
same `"robot_soldier_idle"` name.

## Files touched (all game-side)

| File | Item 08 | Item 09 |
|---|---|---|
| `include/Game/Enemy.h` | created | + AI state enum, physics box, hearing/sight tunables, walk anim, patrol/state fields, `sense/tick` API, `setPatrolPath`, `bubbleIcon()` |
| `src/Game/Enemy.cpp` | created | + `AIState` machine, patrol turnaround, hearing/sight sensors, LOS raycast, chase/jump, search dwell, walk-anim swap |
| `include/Game/EnemyManager.h` | created | + `setPlayerRef`, `setCollision`, `notifyGunshot`, `renderBubbles` |
| `src/Game/EnemyManager.cpp` | created | + stored refs, per-enemy sense+tick call, bubble composition via `DebugDraw2D` |
| `src/Game/GameLayer.cpp` | wired manager | + `setPlayerRef`/`setCollision` after spawn, `setPatrolPath`, `notifyGunshot` when player fires, `renderBubbles` in World scene, vision-cone draws in B overlay |
| `include/Game/Player.h` | added `hurtbox()` | no change (velocity + crouch + mode already exposed) |
| `MegaX.vcxproj` / `.filters` | added Enemy | + Fordward sheet under `<None Include>` (auto-copied) |

Zero engine files. Zero new .cpp/.h beyond the four above being
extended.

---

## What "done" looks like

You'll know Item 09 is finished when, with the current single
Robot Soldier on the map:

1. Fresh scene: soldier walks left and right between two X
   points, pauses at each end, turns around.
2. Walk into their front cone → "!" appears over their head,
   they charge you, jump when you're up a ledge, follow across
   platforms.
3. Sneak up behind them at normal walk speed → "?" appears
   (hearing), they turn around, then upgrade to "!" once you
   step into their line of sight.
4. Crouch-walk behind them → no reaction (you're silent).
5. Press `G` (Ghost) mid-chase → they immediately un-latch,
   dwell briefly (Search), then walk back to patrol.
6. Break line of sight behind a tall solid tile stack, crouch
   → after ~3 s they lose you and return to post.
7. Press `B` → you can see the yellow vision cone and blue
   hearing ring anchored to each enemy, plus the "!" / "?"
   bubbles.
8. Fire a bullet within earshot of a patrolling enemy → their
   `?` pops instantly regardless of your crouch state.

Next: `01_patrol_and_sensing.md` — the extended `Enemy.h` with
all AI fields, and the pure design of patrol, hearing, sight,
and LOS.
