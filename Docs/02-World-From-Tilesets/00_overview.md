# Item 02 — Create World out of TileSets (overview)

## Goal

Render a **world built from tiles** using the engine's tilemap system, driven by
a JSON map file. Support **3 (and later more) tilesets** in one world, follow the
engine's conventions, and lay the groundwork so **animated tiles (item 03)** drop
in cleanly.

When this item is done:

- A tile world loads from `assets/maps/level_01.json` and renders behind the
  ghost player.
- Tiles from **all three** static tilesets appear on screen.
- The camera still follows the ghost smoothly; you can fly around the level.
- The code is structured (a MegaX `World` class) with a documented seam for
  animated tiles.

## Golden rules (unchanged)

1. **Never edit engine code.** Only add/modify files under `G:\Dev\HBE\MegaX\`.
2. **Never reference `HBE.Sandbox`.** We consume the engine as a package
   (public headers + built `.lib`s).
3. Build the **solution** (`HonestlyBadEngine.slnx`), not the bare `MegaX.vcxproj`.

## How the engine's tilemap system works (read this first)

The engine already provides everything we need — we only wire it up:

| Piece | Header | Role |
|---|---|---|
| `TileMap` | `HBE/Renderer/TileMap.h` | Data: tilesets + layers (plain structs). |
| `TileMapLoader` | `HBE/Renderer/TileMapLoader.h` | `loadFromJsonFile(path, outMap, &err)`. |
| `TileMapRenderer` | `HBE/Renderer/TileMapRenderer.h` | `build(...)` GPU state, `draw(r2d, map)`. |

Key facts (verified in the engine source):

- **A layer references exactly ONE tileset** (`layer.tileset` = index into
  `map.tilesets`). Tile IDs in a layer are **local, 1-based** for that tileset
  (`atlasIndex = tileId - 1`). `0` = empty cell.
- Tiles are laid out in the atlas **left→right, top→bottom**, columns derived
  from the texture width (`cols = (texW - 2*margin) / (tileW + spacing)`).
- **Layer data is row-major, bottom-left origin.** `data[0]` is the tile at
  world cell `(x=0, y=0)` = **bottom-left**. The **first row of numbers you
  author is the BOTTOM row of the world.** (The engine uses +Y = up, matching
  the player from item 01.) ← this is the #1 authoring gotcha.
- `TileMapRenderer::build` loads each tileset texture with the cache key
  `"tileset_" + tileset.name`, so our names stay unique and never collide with
  the player textures (remember the item‑01 same‑name cache bug).
- `TileMapRenderer::draw` culls to the active camera and must be called **inside
  an active `beginScene(camera, RenderPass::World)`** pass.

## The 3‑tileset challenge (and our solution)

The engine binds **one tileset per layer**. So to use three tilesets in a single
world, we author **one layer per tileset** and stack them. Layers are drawn in
array order (painter's algorithm), so earlier layers are behind later ones.

Assets (each **192×128 px → 6 cols × 4 rows → 24 tiles**, tile = 32×32):

| Tileset index | JSON `name` | Texture |
|---|---|---|
| 0 | `Military_tileset_1` | `tilesets/Military/Static/Tileset1H.png` |
| 1 | `Military_tileset_2` | `tilesets/Military/Static/Tileset2H.png` |
| 2 | `Military_tileset_3` | `tilesets/Military/Static/Tileset3H.png` |

**Naming convention** (per your instruction): `<folder>_tileset_<n>`, i.e.
`Military_tileset_1`, `Military_tileset_2`, `Military_tileset_3`. When more
tileset folders arrive later (e.g. `Jungle/`), they become `Jungle_tileset_1`,
etc. — no collisions.

Our starter map uses three layers:

| Draw order | Layer name | Tileset | Purpose |
|---|---|---|---|
| 1 (back) | `BG_Columns` | 2 (`Military_tileset_3`) | vertical pipe columns |
| 2 | `BG_Machinery` | 1 (`Military_tileset_2`) | machine blocks |
| 3 (front) | `Ground` | 0 (`Military_tileset_1`) | floor + platforms |

> **Collision (item 04):** the engine's tile collision reads **one** layer +
> that layer's `solidTiles`. We already tag solid tiles per tileset in the JSON
> so item 04 can pick a collision layer without re-authoring. For item 02 there
> is **no collision** — the ghost flies through everything.

## Preparing for animated tiles (item 03)

Item 03 adds animated tiles (`tilesets/Military/Animated/tile-8_Anim_spritesheet.png`,
`tile-13/14/15/20`, each **128×64 → 4×2 → 8 frames**). Those numbers map to
**static tile IDs in `Military_tileset_1`** (e.g. tile `8` gets an animated
variant). The engine's `TileMapRenderer` only draws static atlas tiles and we
**cannot edit it**, so animated tiles will be drawn by **MegaX** on top of the
static cells.

To make item 03 a drop-in, item 02 establishes:

- A **`World` class** (`src/World/World.cpp`, `include/World/World.h`) that owns
  the map + renderer and exposes `load / update(dt) / render(r2d)`.
- A **`AnimatedTile` data struct** (`include/World/AnimatedTile.h`) — the record
  item 03 will fill from the map JSON.
- A reserved **`"animatedTiles": []`** array in the map JSON (the engine loader
  ignores unknown keys; MegaX will parse it in item 03 via the already‑available
  `nlohmann/json`).
- `World::update(dt)` advances an animation clock, and `World::render` calls a
  private `renderAnimatedTiles()` seam (an intentional **no‑op until item 03**).

Nothing animates yet in item 02 — but the class shape, JSON schema, naming, and
render order are all locked so item 03 only adds logic, not restructuring.

## Deliverables / documents

1. `01_world_class.md` — `AnimatedTile.h`, `World.h`, `World.cpp` (full code).
2. `02_map_json.md` — full `level_01.json` + schema + how to design levels.
3. `03_gamelayer_wiring.md` — `GameLayer.h` / `GameLayer.cpp` edits.
4. `04_build_run_and_verify.md` — add files to the project, build, verify,
   troubleshoot.

## Coordinate / scale recap (from item 01)

- 32×32 tiles, `tilePixelScale = 1.0` → **1 world unit = 1 pixel**, tile = 32 wu.
- Camera zoom = 3 (magnifies). `pixelSnap` stays **off** (item‑01 fix).
- +Y is up (SPACE = up, S = down). Map occupies world `x∈[0, W·32]`,
  `y∈[0, H·32]` (first quadrant). We spawn the ghost near the map center.
