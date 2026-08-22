# Item 03 · Doc 00 — Animated Tiles Overview

Goal: make the machinery tiles in the world **come alive** — blinking screens,
moving levers, running circuits — by animating specific tiles that were static
in item 02. We do this **entirely in MegaX**, without editing engine code.

Item 02 already left the seam: `World` has an empty `renderAnimatedTiles()` and a
reserved `m_animClock` / `m_animatedTiles`, and the map JSON reserves an
`"animatedTiles": []` array. Item 03 fills all of that in.

---

## The assets

`assets/tilesets/Military/Animated/` contains 5 spritesheets:

| File | Grid | Frames |
|------|------|--------|
| `tile-8_Anim_spritesheet.png`  | 128×64 | 8 (4 cols × 2 rows, 32×32) |
| `tile-13_Anim_spritesheet.png` | 128×64 | 8 |
| `tile-14_Anim_spritesheet.png` | 128×64 | 8 |
| `tile-15_Anim_spritesheet.png` | 128×64 | 8 |
| `tile-20_Anim_spritesheet.png` | 128×64 | 8 |

Each sheet is an **8‑frame loop** of a single 32×32 tile, frames laid out
left→right then top→bottom (frame 0 = top‑left, frame 7 = bottom‑right).

### The naming is the mapping (important)

`tile-N_Anim_spritesheet.png` is the animation of the **static tile whose local
id is `N`** in **`Military_tileset_1`** (tileset **index 0**). Frame 0 of each
sheet is (visually) the same as the static tile it replaces. So:

| Sheet | Overrides tileset index | Overrides tile id |
|-------|-------------------------|-------------------|
| tile-8_Anim  | 0 | 8  |
| tile-13_Anim | 0 | 13 |
| tile-14_Anim | 0 | 14 |
| tile-15_Anim | 0 | 15 |
| tile-20_Anim | 0 | 20 |

---

## The approach: **overdraw**

The engine's `TileMapRenderer` already draws tile ids 8/13/14/15/20 wherever you
place them, using their **static** (≈ frame‑0) art. Rather than fight the engine
or rewrite the tilemap renderer, MegaX simply **draws the current animation frame
on top** of those cells every frame, at a **higher render layer**.

```
render layer 0  ──  static tile layers      (TileMapRenderer)
render layer 1  ──  animated overdraw        (World::renderAnimatedTiles)   ← NEW
render layer 100 ─  the player               (Player)
```

Why this is nice:

- **No special map placement.** You author animated tiles like any other tile —
  just place id `8`, `13`, `14`, `15`, or `20` in a tileset‑0 layer.
- **No popping.** At frame 0 the overdraw matches the static tile underneath, so
  there's never a visible seam even for the one frame they coincide.
- **The animated cells are ~fully opaque**, so the overdraw completely hides the
  static tile on every other frame.
- **Zero engine edits.** Everything lives in `World`.

How `World` finds the cells to animate: on load it scans every tileset‑0 layer
for cells equal to an animated id and records their `(x, y)`. Each frame it
overdraws exactly those cells.

---

## The one engine gotcha you must respect

The sprite batch stores the **material pointer** at submit time and reads
`material->texture` at **flush** time (end of the scene). That means:

> ❌ You **cannot** share one `Material` and mutate its `texture` between draws —
> every quad would end up bound to whichever texture the material held last.
>
> ✅ Each animation must own its **own persistent `Material`** (with its own
> texture), exactly like `TileMapRenderer` gives each tileset its own material.

Conversely, the **UV rect is baked into the vertices at submit time**, so it is
perfectly safe to change `RenderItem::uvRect` every frame to select the current
animation frame. That is the whole trick: one persistent material per animation +
a per‑frame uvRect.

---

## What you'll change (all MegaX)

| File | Change |
|------|--------|
| `include/World/AnimatedTile.h` | Add runtime fields: own `Material`, `texW/texH`, and the list of cells where it appears. |
| `include/World/World.h` | Store the sprite shader + quad mesh; declare `loadAnimatedTiles` + `computeFrameUV`. |
| `src/World/World.cpp` | Parse `"animatedTiles"`, load sheets, scan cells, and implement `renderAnimatedTiles`. |
| `assets/maps/level_01.json` | Fill the `"animatedTiles"` array + place the animated ids in a tileset‑0 layer. |

**No `GameLayer` change and no `MegaX.vcxproj` change are needed** — item 02
already calls `m_world.update(dt)` and `m_world.render(r2d)`, and `World.cpp` /
`World.h` / `AnimatedTile.h` are already registered in the project.

---

## Golden rules

1. **MegaX only.** Never touch engine code or reference `HBE.Sandbox`.
2. **One `Material` per animation.** Never mutate a shared material's texture.
3. **`uvRect` per frame is fine** (baked at submit). Select the frame there.
4. **Animated tiles must be placed in a tileset‑0 layer** or `World` finds no
   cells to animate.
5. **Frame 0 ≈ the static tile**, so placement in the static layer is invisible
   until animation kicks in — that's intended.

Docs in this item:

- `01_animated_tile_code.md` — the updated `AnimatedTile.h`, `World.h`, `World.cpp`.
- `02_map_json.md` — the `level_01.json` edits (define + place animated tiles).
- `03_build_run_and_verify.md` — build, run, verify the tiles cycle, troubleshoot.
