# Item 07 · Doc 00 — Particle Effects (Overview)

Goal: bring the world alive with **particle feedback** for every player action
we already have signals for. Nothing in this item changes gameplay — it's all
"juice".

The engine already ships a full particle system
(`HBE::Renderer::ParticleSystem`, `EmitterConfig`, `EffectDef`), so we don't
touch engine code. We just **author configs** in MegaX, wire them into a small
`MegaX::Effects` helper, and call it from the places we already have signals
for (footsteps, landings, shots, bullet impacts).

---

## What we add

Five particle effects, all authored MegaX‑side as `EffectDef`s and registered
by name with `ParticleSystem::registerEffect(...)`:

| Effect name | When it fires | Look |
|-------------|---------------|------|
| `walk_dust` | Player walking on the ground, at a small cadence | Little kicks of dust behind the feet, coloured by the top of the ground tile. |
| `land_dust` | Player touches down after being airborne | Wider fan of dust under both feet, same tile colour, slightly larger than walk. |
| `muzzle_flash` | Every shot | Bright additive puff at the gun tip, aimed along facing. |
| `bullet_casing` | Every shot | A brass casing ejected upward + back from the gun, tumbling under gravity. |
| `bullet_impact` | Bullet dies on a solid tile | Two‑layer burst: chunks tinted with the *hit* tile's colour + additive yellow "metal" sparks for the bullet exploding. |

All effects are **world‑space**, one‑shot bursts (except `walk_dust`, which
fires at a cadence while walking). They share the sprite shader and a 1×1
white texture that `ParticleSystem::initialize()` builds for us.

---

## How the effects tint themselves from the world

Three of the five effects need the **colour of the tile they're touching**.
The engine has that helper baked in:

```cpp
HBE::Renderer::TileMapLoader::sampleTileTopColors(
    map, outColors /* std::unordered_map<int, std::array<float,4>> */,
    /*topRowsToSample*/ 3);
```

Notes:

- It samples **tileset index 0** and keys by the **1‑based tile ID**.
- Our current `level_01.json` uses tileset 0 for both the `Ground` and
  `Background` layers, so this "just works" today. If a future map changes
  the Ground to a different tileset index we'll fall back to a neutral tan
  when the tile ID is missing from the map.
- We call `sampleTileTopColors` **once** in `Effects::init()` after the world
  has been loaded, and cache the result in an `std::unordered_map`.

At spawn time we clone the base config, patch `startR/G/B` and `endR/G/B`
with the sampled colour, `registerEffect(name, cfg)` to overwrite the library
entry, then `spawn(name, x, y)`. This mirrors exactly what the Sandbox does
for its own `player_dust` (see `HBE.Sandbox/src/GameLayer.cpp` around
`spawnPlayerDustAtFeet`).

---

## Data flow

```
Player.update() ──► records:
   • landedThisFrame flag        (already computed locally, we expose it)
   • groundTileId                (tile ID beneath the feet, 0 if airborne)
   • consumeShot()               (already exists: muzzle x/y + facing)

BulletManager.update() ──► records:
   • Impact events {x, y, tileId}  (new: collected when a bullet dies on a tile)

GameLayer.onUpdate() ──►
   • if landed:            m_effects.spawnLandingDust(feetX, feetY, groundTileId)
   • while walking:        m_effects.tickWalkDust(dt, feetX, feetY, groundTileId,
                                                  moving, grounded)
   • on shot fired:        m_effects.spawnMuzzleFlash(mx, my, dir)
                           m_effects.spawnCasing(mx, my, dir)
   • per bullet impact:    m_effects.spawnBulletImpact(ix, iy, tileId)
   • every frame:          m_effects.update(dt)

GameLayer.onRender() ──►
   • inside beginScene(World): after world + player + bullets, m_effects.render(r2d)
```

`MegaX::Effects` owns exactly one `HBE::Renderer::ParticleSystem` and one
cached `unordered_map<int, std::array<float,4>>` of tile→top colour. Nothing
else in it is stateful except the small walk‑dust cadence timer.

---

## Files touched (all **MegaX‑only**, no engine edits)

| File | Change |
|------|--------|
| `include/Game/Effects.h` (new) | `MegaX::Effects` declaration. |
| `src/Game/Effects.cpp` (new) | Emitter factories + wrapper class impl. |
| `include/Game/Player.h` | Three tiny getters: `landedThisFrame()`, `groundTileId()`, `feetY()`. |
| `src/Game/Player.cpp` | Cache those values from `updatePlay()`. |
| `include/Game/Bullet.h` | `Impact` struct + `consumeImpacts()` queue. |
| `src/Game/Bullet.cpp` | Record impact `{x, y, tileId}` when a bullet dies on a tile. |
| `include/Game/GameLayer.h` | `Effects m_effects{};` member. |
| `src/Game/GameLayer.cpp` | Init effects; fire spawns; update / render effects. |
| `MegaX.vcxproj` (+ `.filters`) | Register `Effects.h` / `Effects.cpp`. |

---

## Golden rules

1. **MegaX only.** No engine or Sandbox edits. Anything missing engine‑side
   is noted in‑doc for the future engine pass, but we work around it here.
2. **One `ParticleSystem`.** Owned by `MegaX::Effects`, initialized after
   the sprite pipeline + quad mesh exist **and** the world has been loaded
   (so we can sample tile colours).
3. **Tint via re‑registration.** For colour‑from‑world effects, clone the
   base cfg, patch RGBs, `registerEffect(name, cfg)` then `spawn(...)`.
   Do NOT try to mutate a library entry in place — the API takes the
   config by value on register.
4. **Muzzle / casing reuse the shot coords** the player already publishes
   via `consumeShot()`, so nothing drifts if we ever retune the muzzle
   offsets in item 06.
5. **Impacts belong to bullets.** `BulletManager` is the only thing that
   knows a bullet died on a tile; it emits an event, `Effects` reacts.
   Never re‑sample tiles from `GameLayer`.

---

## Docs in this item

- `01_effects_configs_and_class.md` — the five `make*()` factories and
  the `MegaX::Effects` wrapper.
- `02_player_and_bullet_hooks.md` — small additions to `Player` and
  `BulletManager` that expose the signals `Effects` needs.
- `03_gamelayer_and_project.md` — wiring in `GameLayer` + the two new
  `.vcxproj` (and `.filters`) entries.
- `04_build_run_and_verify.md` — build, run, verify checklist, tuning
  cheat‑sheet, and troubleshooting.
