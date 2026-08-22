# Item 05 · Doc 00 — Toggle Play Mode / Ghost Mode (Overview)

Goal: one **hot‑key** that flips the player between two modes:

- **Play** — normal gameplay: gravity, jump, crouch, and tile collisions
  (everything from item 04).
- **Ghost** — a free 8‑way fly with **no gravity and no collisions**, used while
  building/inspecting maps (and later: not aggroing enemies).

This is a tiny item because **item 04 already built both modes.** `Player`
already has `Mode { Play, Ghost }`, `toggleMode()`, `setMode()`, and the full
Ghost free‑fly path — we just never bound a key to the toggle. Item 05 binds the
key and makes the current mode **visually obvious**.

---

## What already exists (from item 04)

In `Player`:

```cpp
enum class Mode { Play, Ghost };
void setMode(Mode m);
Mode mode() const;
void toggleMode();          // Play <-> Ghost
```

- `update(dt)` dispatches to `updatePlay(dt)` or `updateGhost(dt)` based on
  `m_mode`.
- `updateGhost` is the original item‑01 free fly: A/D horizontal, Space up, S
  down, normalized so diagonals aren't faster; **no gravity, no collision**.
- `GameLayer::onUpdate` already computes *both* input sets every frame — the
  ghost `iy` (Space/S = up/down) **and** the play intents (jump/crouch) — and
  hands them all to the player. So flipping `m_mode` is all that's needed; no
  input rewiring.

So item 05 = **(1)** a key that calls `toggleMode()`, and **(2)** a visual tell.

---

## What we add

1. **Hot‑key `G`** in `GameLayer::onUpdate` → `m_player.toggleMode()`, with a log
   line reporting the new mode. `G` = "Ghost"; it's unused by gameplay
   (A/D/S/Space/H are taken).

2. **Ghost visual** — in `Player::update`, tint the sprite each frame from the
   mode: Ghost = translucent bluish (`{0.6, 0.8, 1.0, 0.5}`), Play = opaque
   white (`{1,1,1,1}`). This uses the per‑item `RenderItem::tint`, which the
   sprite shader multiplies in (`tex * uColor * vColor`) — no new material, no
   extra draw call, and `SpriteAnimation::apply` only writes `uvRect` so the tint
   survives every frame.

---

## Mode behavior at a glance

| | Play | Ghost |
|--|------|-------|
| Gravity | yes | no |
| Tile collisions | yes | no |
| `A` / `D` | run | fly left/right |
| `Space` | jump (variable height) | fly **up** |
| `S` | crouch | fly **down** |
| Look | opaque | translucent blue |

Default on launch is **Play** (set in item 04). Press `G` to enter Ghost for map
work, `G` again to return.

---

## Why toggling is seamless (no snap needed)

Both update paths keep the player's position (`m_x/m_y`) and physics box
(`m_box`) in sync every frame:

- `updateGhost` writes `m_x/m_y` from the fly and mirrors them into `m_box`, so a
  switch **into** Play already has a valid box (the next `moveAndCollideEx` just
  resolves any overlap normally).
- `updatePlay` writes `m_box` then syncs `m_x/m_y`, so a switch **into** Ghost
  starts flying from exactly where you were standing.

No teleport, no camera jump — the `CameraController` just keeps following
`m_player.x()/y()`.

---

## What you'll change (all MegaX, no engine edits)

| File | Change |
|------|--------|
| `src/Game/Player.cpp` | Set `m_item.tint` from the mode inside `update()`. |
| `src/Game/GameLayer.cpp` | `G` hot‑key → `toggleMode()` + log; update the attach log. |

**No header changes** (the `Mode` API already lives in `Player.h` from item 04),
no `World`/engine/vcxproj edits.

## Golden rules

1. **MegaX only.** No engine edits, no `HBE.Sandbox` references.
2. **Don't rewire input.** `GameLayer` already feeds both mode's inputs; only the
   `m_mode` flag decides which the player uses.
3. **Keep the Ghost path.** It's the item‑01 free‑fly; don't delete it.

Docs in this item:

- `01_code_changes.md` — the exact edits to `Player.cpp` and `GameLayer.cpp`.
- `02_build_run_and_verify.md` — build, run, verification checklist, tuning.
