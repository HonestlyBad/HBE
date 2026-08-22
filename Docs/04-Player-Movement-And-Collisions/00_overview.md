# Item 04 · Doc 00 — Player Movement, Jump, Crouch, Gravity & Collisions (Overview)

Goal: turn the free‑flying "ghost" player from item 01 into a real **Mega Man X‑style
platformer** character — gravity, running, a snappy variable‑height jump, crouch,
and **solid‑tile collisions** against the world we built in items 02–03.

Everything is done **inside MegaX**. We do **not** edit engine code — the engine
already ships a tile collision solver (`HBE::Renderer::TileCollision`) that we
simply drive from `Player`.

---

## What already exists (the seams we build on)

- **`Player`** (item 01) owns a sprite sheet, plays idle/walk animations, flips
  by facing, and hot‑swaps the helmet on `H`. It currently moves by directly
  integrating an input vector (free fly).
- **`World`** (items 02–03) loads `maps/level_01.json` into an engine
  `HBE::Renderer::TileMap` and renders static + animated tiles. It exposes
  `const TileMap& map()`.
- The map already flags **solid** tiles: `Tileset1H` (tileset index 0) has
  `solidTiles = [1,2,3,4,7,8,9,10,14,15]`, and there is a layer named **`Ground`**
  that uses that tileset. **That `Ground` layer is our collision layer.**

So the wiring is short: give `Player` a pointer to the map + the `Ground` layer,
add a physics update path, and feed it the right inputs.

---

## The engine facility we use: `TileCollision`

`HBE/Renderer/TileCollision.h` gives us exactly what a platformer needs — no
engine edits required:

```cpp
struct AABB { float cx, cy, w, h; };           // center-based, world space
struct MoveResult2D { bool hitX, hitY, grounded, ceiling, steppedUp; };

MoveResult2D TileCollision::moveAndCollideEx(
    const TileMap& map, const TileMapLayer& layer,
    AABB& box, float& velX, float& velY, float dt,
    float maxStepUp, bool enableOneWay, bool enableSlopes,
    float oneWayPrevBottom);
```

It integrates the box by `vel * dt`, resolves **X then Y** (classic platformer
order), zeroes the blocked velocity component, and tells us whether we became
**grounded** (landed) or hit a **ceiling**. We pass our player AABB and read the
result back every frame.

**World Y is up‑positive, bottom‑left origin** (same as the tilemap). So:

| Motion | Sign |
|--------|------|
| Jump / rise | `velY > 0` |
| Gravity / fall | `velY < 0` |
| Ground is below | smaller `y` |

Gravity therefore **subtracts** from `velY`; a jump **sets `velY = +jumpSpeed`**.

---

## The sprite sheet row map (measured from `player.png`)

`player.png` is **10 columns × 14 rows**, each cell **75 × 48 px**. Rows below are
**0‑indexed from the top** (the "Sheet row" column). The jump animation is the
**last row** (0‑indexed 13) and the crouch is a **prone crawl** on 0‑indexed
row 1:

| State | Sheet row | Cols | Frames | Notes |
|-------|-----------|------|--------|-------|
| Idle | 3 | 0–3 | 4 | existing |
| Walk / Run | 6 | 0–9 | 10 | existing |
| **Crouch (prone crawl)** | **1** | **0–5** | 6 | loops; plays while `S` is held |
| **Jump — rising** | **13** | **0–2** | 3 | ascent → apex; non‑looping |
| **Jump — falling** | **13** | **3** | 1 | single fall frame |
| **Jump — landing** | **13** | **4–5** | 2 | brief touchdown recovery; non‑looping |

> The last row (13) is one 6‑frame jump cycle: frames 1‑3 = ascent to the apex,
> frame 4 = fall, frames 5‑6 = landing. We pick **rise** while `velY > 0`,
> **fall** while airborne and `velY ≤ 0`, and play **landing** briefly on
> touchdown. If a row looks wrong on your sheet, change **one constant** in
> `Player.cpp`.

The `player-no-helm.png` sheet shares the identical grid, so the helmet swap
(item 01) keeps working for every new animation automatically.

---

## Hitbox & feet alignment (why the numbers matter)

The character art sits near the **bottom** of each 75×48 cell (feet ≈ frame
bottom; measured body ≈ 22–24 px wide, ≈ 37 px tall). We therefore use a
collision box **smaller than the sprite**, and we keep the sprite's **frame
bottom locked to the box bottom** so the feet rest exactly on the floor:

```
render:  posY = box.cy - box.h*0.5 + (frameH*0.5)     // frame bottom == box bottom
```

Chosen box (tunable constants, world px):

| | Width | Height |
|--|-------|--------|
| Standing | 24 | 40 |
| Crouching | 24 | 24 |

Crouching keeps the **box bottom fixed** (feet don't move) and shrinks the top,
so you fit under 1‑tile gaps. Standing back up is only allowed when the taller
box **doesn't overlap a solid tile** (so you can't stand into a ceiling).

---

## Play mode vs Ghost mode

Item 01's free‑fly is the future **Ghost mode** (used for map building — no
gravity, no collisions). Item 05 will add the hot‑key that toggles it. To keep
that forward‑compatible, item 04 introduces `Player::Mode { Play, Ghost }` and
**keeps the old free‑fly code as the Ghost path**. For item 04 we **default to
`Play`** so we can actually test gravity/jump/crouch/collisions.

- **Play** — gravity, run, variable jump, crouch, tile collisions. (NEW)
- **Ghost** — the item‑01 free 8‑way fly, no collisions. (preserved)

Item 05 just calls `m_player.toggleMode()` on a key press — no rework.

---

## Feel: the Mega Man X touches

Implemented for a good game feel (all tunable constants):

- **Snappy horizontal** — velocity is set directly from input (full air control),
  no mushy acceleration.
- **Variable jump height** — release the jump button while rising and the upward
  velocity is cut, giving short hops vs full jumps.
- **Coyote time** (~0.08 s) — you can still jump for a few frames after walking
  off a ledge.
- **Jump buffer** (~0.10 s) — pressing jump just before landing still jumps.
- **Fall speed clamp** — terminal velocity so long falls stay controllable.

---

## What you'll change (all MegaX, no engine edits)

| File | Change |
|------|--------|
| `include/Game/Player.h` | Add `Mode`, collision pointers, jump/crouch input intents, physics tunables, the AABB + physics/anim state, and new animation members. |
| `src/Game/Player.cpp`   | Add gravity/run/jump/crouch + `TileCollision` resolve, the animation state machine, and the Ghost/Play split. |
| `src/Game/GameLayer.cpp`| Wire the `Ground` collision layer into the player, map inputs (A/D + Space jump + S crouch), keep camera follow. |

**No `GameLayer.h`, `World.*`, `MegaX.vcxproj`, or map JSON changes are needed.**
`GameLayer` already holds `Player`, `World`, and the `CameraController`, and all
these source files are already registered in the project.

---

## Golden rules

1. **MegaX only.** Never touch engine code or reference `HBE.Sandbox`.
2. **The `Ground` layer is the collision layer.** Solid tiles must be listed in
   the tileset's `solidTiles` in the map JSON (already true for `Tileset1H`).
3. **Feet == box bottom.** Keep the render sync formula intact or the player will
   float or sink into the floor.
4. **World Y is up‑positive.** Gravity subtracts from `velY`; jump adds.
5. **Ghost path stays.** Don't delete the item‑01 free‑fly — it becomes Ghost
   mode in item 05.

Docs in this item:

- `01_player_physics_code.md` — the full updated `Player.h` and `Player.cpp`.
- `02_gamelayer_wiring.md` — collision wiring + input mapping in `GameLayer.cpp`.
- `03_build_run_and_verify.md` — build, run, the verification checklist, tuning &
  troubleshooting.
