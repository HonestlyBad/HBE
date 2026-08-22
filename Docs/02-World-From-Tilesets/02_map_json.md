# Item 02 · Doc 02 — The map file `assets/maps/level_01.json`

Create `G:\Dev\HBE\MegaX\assets\maps\level_01.json` with the content below. It is
a **32×12 tile** starter world (world size **1024×384 px**) that uses **all three**
tilesets so you can confirm the multi‑tileset pipeline works. Edit it freely
afterwards to design real levels.

## Schema (what each field means)

```jsonc
{
  "version": 1,
  "tileSize":       { "w": 32, "h": 32 },   // px per tile in the atlas
  "tilePixelScale": 1.0,                     // world px per atlas px (1 => 32wu tiles)

  "tilesets": [                              // index 0,1,2,... referenced by layers
    {
      "name":        "Military_tileset_1",   // <folder>_tileset_<n> convention
      "texture":     "../tilesets/...png",   // path RELATIVE TO THIS MAP FILE
      "tileW": 32, "tileH": 32,
      "margin": 0, "spacing": 0,
      "solidTiles": [ 4, 10, 16 ]            // 1-based ids; used by collision (item 04)
    }
  ],

  "layers": [                                // drawn in array order (front = last)
    {
      "name":    "Ground",
      "w": 32, "h": 12,
      "tileset": 0,                          // index into "tilesets"
      "data":    [ /* w*h ints, bottom-up */ ]
    }
  ],

  "animatedTiles": []                        // reserved for item 03 (engine ignores it)
}
```

### Critical authoring rules

- **`data` is row-major, bottom-left origin.** The **first row of numbers is the
  BOTTOM row of the world**; the last row is the top. Read each layer block below
  bottom-to-top.
- Tile ids are **1-based and local to that layer's tileset** (`0` = empty).
  `Military_tileset_1` has ids `1..24` (6 cols × 4 rows), same for the others.
- `texture` is resolved **relative to the map file**, so from `assets/maps/` the
  tilesets are at `../tilesets/Military/Static/...`.
- Every layer's `data` length must equal `w * h` (here `32 * 12 = 384`).

## Full file

```json
{
  "version": 1,
  "tileSize": { "w": 32, "h": 32 },
  "tilePixelScale": 1.0,
  "tilesets": [
    {
      "name": "Military_tileset_1",
      "texture": "../tilesets/Military/Static/Tileset1H.png",
      "tileW": 32, "tileH": 32, "margin": 0, "spacing": 0,
      "solidTiles": [ 4, 10, 16 ]
    },
    {
      "name": "Military_tileset_2",
      "texture": "../tilesets/Military/Static/Tileset2H.png",
      "tileW": 32, "tileH": 32, "margin": 0, "spacing": 0,
      "solidTiles": []
    },
    {
      "name": "Military_tileset_3",
      "texture": "../tilesets/Military/Static/Tileset3H.png",
      "tileW": 32, "tileH": 32, "margin": 0, "spacing": 0,
      "solidTiles": []
    }
  ],
  "layers": [
    {
      "name": "BG_Columns",
      "w": 32, "h": 12,
      "tileset": 2,
      "data": [
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
      ]
    },
    {
      "name": "BG_Machinery",
      "w": 32, "h": 12,
      "tileset": 1,
      "data": [
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 20, 20, 20, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 22, 22, 22, 22, 0, 0, 0, 0,
        0, 0, 0, 20, 20, 20, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 22, 22, 22, 22, 0, 0, 0, 0,
        0, 0, 0, 20, 20, 20, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
      ]
    },
    {
      "name": "Ground",
      "w": 32, "h": 12,
      "tileset": 0,
      "data": [
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
      ]
    }
  ],
  "animatedTiles": []
}
```

## What this map draws (bottom → top)

- **`Ground`** (front, `Military_tileset_1`): a solid 2‑row floor (`4` cap over
  `10` fill) across the bottom, plus two floating platforms of tile `16`.
- **`BG_Machinery`** (`Military_tileset_2`): two machine clusters (`20`, `22`).
- **`BG_Columns`** (back, `Military_tileset_3`): two vertical pipe runs (tile `2`)
  at columns 15 and 29.

Because layers draw in array order, `BG_Columns` is behind `BG_Machinery`, which
is behind `Ground`; the player (layer 100) renders in front of all of them.

## Designing your own levels

- Pick tile ids from the tileset previews (each set is 6 cols × 4 rows → ids
  `1..24`, numbered left→right, top→bottom).
- Keep **one layer per tileset**. Add more layers (and more tilesets) as needed;
  just give each tileset a unique `<folder>_tileset_<n>` name.
- Remember rows are **bottom‑up**. A quick sanity check: your floor row is the
  **first** `data` row.
- Add a tile id to a tileset's `solidTiles` to make it collidable later (item 04).
