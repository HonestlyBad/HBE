# Item 03 · Doc 02 — Map JSON edits (`assets/maps/level_01.json`)

Two edits to the item‑02 map:

1. **Define** the 5 animations by filling the `"animatedTiles"` array.
2. **Place** those animated tile ids in a tileset‑0 layer so `World` has cells to
   animate (otherwise nothing shows — the animations are defined but unused).

---

## Edit 1 — fill `"animatedTiles"`

Replace the reserved empty array at the bottom of the file:

```json
  "animatedTiles": []
```

with:

```json
  "animatedTiles": [
    { "tileset": 0, "tileId": 8,  "sheet": "../tilesets/Military/Animated/tile-8_Anim_spritesheet.png",  "frameW": 32, "frameH": 32, "frames": 8, "fps": 8 },
    { "tileset": 0, "tileId": 13, "sheet": "../tilesets/Military/Animated/tile-13_Anim_spritesheet.png", "frameW": 32, "frameH": 32, "frames": 8, "fps": 8 },
    { "tileset": 0, "tileId": 14, "sheet": "../tilesets/Military/Animated/tile-14_Anim_spritesheet.png", "frameW": 32, "frameH": 32, "frames": 8, "fps": 8 },
    { "tileset": 0, "tileId": 15, "sheet": "../tilesets/Military/Animated/tile-15_Anim_spritesheet.png", "frameW": 32, "frameH": 32, "frames": 8, "fps": 8 },
    { "tileset": 0, "tileId": 20, "sheet": "../tilesets/Military/Animated/tile-20_Anim_spritesheet.png", "frameW": 32, "frameH": 32, "frames": 8, "fps": 8 }
  ]
```

### Field meanings

| Field | Meaning |
|-------|---------|
| `tileset` | 0‑based index into `"tilesets"`. Always `0` here (the animations belong to `Military_tileset_1`). |
| `tileId`  | 1‑based local tile id this animation replaces (matches the sheet name `tile-N_…`). |
| `sheet`   | Path to the spritesheet, **relative to this map file** (same rule as a tileset `texture`). |
| `frameW`/`frameH` | Size of one frame in the sheet (32×32). |
| `frames`  | Frame count (8). |
| `fps`     | Playback speed. `8` fps = a full 8‑frame loop every second. Tune per taste. |

---

## Edit 2 — place the animated tiles in the world

The animations only render on cells that actually contain their tile id in a
tileset‑0 layer. The item‑02 `Ground` layer used ids `4/10/16` only — none
animated — so **add a new front layer** that scatters the 5 animated ids where
you can see them.

Add this layer object to the `"layers"` array, **after** the `Ground` layer (so
it draws in front of it):

```json
    ,
    {
      "name": "FG_Machines",
      "w": 32, "h": 12,
      "tileset": 0,
      "data": [
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 13, 0, 0, 0, 14, 0, 0, 0, 15, 0, 0, 0, 20, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
      ]
    }
```

> ⚠️ **JSON comma:** the leading `,` above assumes you paste this **right after
> the closing `}` of the `Ground` layer** and before the `]` that ends
> `"layers"`. If your editor complains, make sure there's exactly one comma
> between each layer object and none before the final `]`.

### What this places

Reading `data` **bottom‑up** (row 0 = bottom), the only non‑empty row is the
**third row (`y = 2`)** — the row that sits right on top of the 2‑row floor. It
places, left→right:

| Column | Tile id | Animation |
|--------|---------|-----------|
| 9  | `8`  | tile‑8 loop  |
| 13 | `13` | tile‑13 loop |
| 17 | `14` | tile‑14 loop |
| 21 | `15` | tile‑15 loop |
| 25 | `20` | tile‑20 loop |

So you get a **row of 5 animated machines standing on the floor**, spaced 4 tiles
apart and centered around the map's middle — right where the player spawns and
the camera starts, so they're immediately visible.

---

## How it renders

1. `TileMapRenderer` draws `FG_Machines` like any layer → each cell shows the
   **static** (frame‑0) art of tile 8/13/14/15/20 at render layer 0.
2. `World::loadAnimatedTiles` scans layers, finds these 5 cells (one per id), and
   records them.
3. Every frame, `World::renderAnimatedTiles` overdraws each cell with the current
   animation frame at render layer 1 — on top of the static art, under the player.

Because frame 0 equals the static art, there's no pop when the loop wraps.

---

## Designing with animated tiles later

- Place tile id `8`, `13`, `14`, `15`, or `20` (tileset 0) **anywhere** in any
  tileset‑0 layer and it animates automatically — no extra JSON per placement.
- Repeat an id as many times as you like; every occurrence animates in sync.
- To add a **new** animated tile: drop a sheet in `assets/tilesets/Military/Animated/`,
  add one entry to `"animatedTiles"` (its `tileId` + `sheet`), and place that id.
- Per‑tile speed: change that entry's `fps`.
