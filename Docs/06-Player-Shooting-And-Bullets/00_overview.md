# Item 06 · Doc 00 — Player Shooting & Bullet Collisions (Overview)

Goal: give the player a **gun**. Pressing the fire key plays a shoot pose, spits a
**sleek bullet** from the gun tip, and the bullet flies until it either **hits a
solid tile** or travels **4 tiles past the edge of the view**. Firing **while
airborne** shoves the player back a little, like the gun's recoil in mid‑air.

(The sheet also has a *throw* animation on row 0 — per the work item we focus on
shooting now and leave throwing for later.)

All new code is **MegaX‑only**. No engine edits; we reuse the engine's sprite
renderer, `TileMap` solid‑tile data, and `Camera2D`.

---

## What we add

1. **`BulletManager`** (new `Game/Bullet.h` + `Game/Bullet.cpp`) — owns a pool of
   bullets, moves them, culls them on tile hit / off‑screen, and draws them as
   thin bright quads. Rendered with the existing sprite shader + the quad mesh +
   a generated **1×1 white texture** (tinted yellow), so no new art asset is
   needed.
2. **Player shooting** — a fire input, an auto‑fire cadence, a shoot‑pose overlay
   on the animation state machine, a **muzzle position** that depends on the
   pose, an **air recoil** impulse, and a `consumeShot()` the layer polls to know
   when/where to spawn a bullet.
3. **`GameLayer` wiring** — read the fire key, feed it to the player, spawn a
   bullet whenever the player reports a shot, and update/render the bullets.

---

## The sprite sheet shoot rows (measured from `player.png`)

`player.png` is 10 cols × 14 rows, 75×48 px. Shoot poses (0‑indexed rows):

| State | Row | Cols | Frames | Notes |
|-------|-----|------|--------|-------|
| Shoot — standing | 10 | 0–1 | 2 | includes a muzzle‑flash frame |
| Shoot — run‑and‑gun | 5 | 0–9 | 10 | used while moving on the ground |
| Shoot — airborne | 8 | 0–2 | 3 | used while jumping/falling |

Crouch shooting keeps the prone crawl (row 1) and just fires from a low muzzle —
there's no separate prone‑shoot pose. (Other gun rows on the sheet: row 4 aim,
rows 11–12 more standing‑shoot variants — swap the row constants if you prefer
one of those.)

### Muzzle offsets (gun tip, frame‑local: +x forward, +y up)

Measured from the muzzle‑flash centroid on the shoot rows, relative to the sprite
**center** (`m_x, m_y`). `x` is multiplied by facing (`±1`):

| Pose | forward x | up y |
|------|-----------|------|
| Standing / walking | 30 | +1 |
| Airborne | 30 | +2 |
| Crouch (prone) | 28 | −14 |

Bullet spawn world position = `(m_x + facing*fwd, m_y + up)`.

---

## Firing model

- **Key: `J`** (tunable). `IsKeyPressed` gives an instant first shot; holding `J`
  **auto‑fires** at a cadence (`kFireCooldown = 0.15 s`).
- Each shot sets a **shoot‑pose timer** (`kShootHold = 0.22 s`, slightly longer
  than the cooldown so the pose stays up during sustained fire).
- **Only in Play mode.** Ghost mode is for map building — no shooting.
- **Air recoil:** an airborne shot sets a decaying horizontal velocity
  (`kRecoilImpulse = 120 px/s`, opposite facing) that's added on top of air
  control and fades out (`kRecoilDamp = 7/s`). On the ground the recoil is
  cancelled (feet grip), so only mid‑air shots knock you back.

---

## Bullet model

Each bullet is `{ x, y, vx }`, travelling horizontally at `speed = 640 px/s` in
the facing direction. Every frame:

1. Integrate `x += vx*dt`.
2. **Tile hit:** if the bullet's point is inside a **solid** tile of the
   `Ground` layer, it dies. (Same solid data the player collides with — future
   enemies can be added to this test.)
3. **Off‑screen:** if it passes the camera's visible rect by more than
   `offscreenTiles = 4` tiles, it dies.

Dead bullets are removed each frame. Bullets draw on **layer 101** (in front of
the player) as a 16×4 px bright‑yellow quad.

---

## What you'll change (all MegaX, no engine edits)

| File | Change |
|------|--------|
| `include/Game/Bullet.h` (new) | `BulletManager` declaration. |
| `src/Game/Bullet.cpp` (new) | Bullet spawn/update/cull/render + white texture. |
| `include/Game/Player.h` | Fire input, `consumeShot`, shoot/recoil state, 3 shoot anims. |
| `src/Game/Player.cpp` | Shoot constants, shoot anims, fire + recoil logic, shoot overlay. |
| `include/Game/GameLayer.h` | `BulletManager` member + cached `Ground` layer pointer. |
| `src/Game/GameLayer.cpp` | Init bullets, read `J`, spawn/update/render bullets. |
| `MegaX.vcxproj` | Register `Bullet.h` / `Bullet.cpp`. |

## Golden rules

1. **MegaX only.** No engine or `HBE.Sandbox` changes.
2. **Bullets share the player's solid data** (`Ground` layer) — one source of
   truth for collisions.
3. **Muzzle is frame‑local × facing.** Keep the spawn formula so bullets leave
   the gun tip in both directions.
4. **Recoil is air‑only.** Grounded cancels it.

Docs in this item:

- `01_bullet_system_code.md` — `Bullet.h` and `Bullet.cpp`.
- `02_player_shooting_code.md` — the `Player.h` / `Player.cpp` additions.
- `03_gamelayer_and_project.md` — `GameLayer` wiring + `.vcxproj` entries.
- `04_build_run_and_verify.md` — build, run, checklist, tuning, troubleshooting.
