# Item 07 · Doc 04 — Build, Run & Verify

## Build

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "G:\Dev\HBE\HonestlyBadEngine.slnx" `
  /t:MegaX /p:Configuration=Debug /p:Platform=x64 /m /nologo /v:minimal
```

Expected tail:

```
  Bullet.cpp
  Effects.cpp
  GameLayer.cpp
  Player.cpp
  World.cpp
  main.cpp
  Generating Code...
  MegaX.vcxproj -> G:\Dev\HBE\MegaX\bin\x64\Debug\MegaX.exe
  MegaX: copying runtime deps to G:\Dev\HBE\MegaX\bin\x64\Debug\
```

Exit code `0`.

## Run

```powershell
& "G:\Dev\HBE\MegaX\bin\x64\Debug\MegaX.exe"
```

---

## Verify checklist

- [ ] **Walk dust** — hold `A` or `D` on any solid floor: a small stream
      of dust puffs kicks up from the feet at a steady cadence
      (~5–6 per second, `walkDustPeriod = 0.18`), tinted the same tan/grey
      as the tile you're on. Stopping instantly stops the stream.
- [ ] **Different floor colours** — walk from one tileset patch to another
      (e.g. grass/dirt tiles vs. metal tiles): the dust colour swaps to
      match, because `Effects::colourForTile` re-samples per spawn.
- [ ] **Landing burst** — jump and land on a solid floor: a wider fan of
      dust puffs under both feet, coloured like that floor. It fires
      **only** on the frame you touch down (single burst, not a stream).
- [ ] **Muzzle flash** — tap the fire key (`E`) while standing: a bright
      yellow-white puff appears **at the gun tip**, not the player's
      centre. It fades in ~0.1s and doesn't linger.
- [ ] **Muzzle direction** — face left (walked into a wall on the left
      side and turned around) and fire: the muzzle sparks fan **to the
      left** of the player, not to the right.
- [ ] **Bullet casing** — every shot ejects a small brass rectangle
      **up and back** relative to the gun, tumbles under gravity, and
      falls to the ground. Facing left, casings eject up-and-to-the-right;
      facing right, up-and-to-the-left.
- [ ] **Impact burst** — shoot into a wall / floor tile: on the bullet's
      last frame you see a two-layer burst — tile-coloured chunks + a
      short yellow additive spark cluster (the bullet "exploding").
- [ ] **Impact colour matches tile** — shoot two different tile types:
      the chunk colour changes with each, the yellow sparks stay yellow.
- [ ] **Off-screen despawn = no impact** — fire across an open gap so the
      bullet leaves the view; **no** impact burst is spawned (only tile
      hits generate impacts).
- [ ] **Ghost mode is inert** — press `G`, fly around: no walk dust, no
      landing dust, no shooting (item 06 already blocks it), so **no**
      muzzle, casing, or impact effects fire either.
- [ ] **Layering** — effects don't clip the player weirdly: walk / land
      dust draw **behind** the player (sortLayer 95 vs. 100); muzzle /
      casing / impact draw in **front** of the bullets (sortLayer 102 vs.
      101).

---

## Tuning cheat‑sheet

All of these are game-code constants in `src/Game/Effects.cpp` unless
otherwise noted. Change and rebuild.

### Walk dust — `makeWalkDust()`

| Field | Default | Effect |
|-------|---------|--------|
| `bursts[0].count` | 4 | Particles per puff. |
| `shapeWidth` | 10 | Horizontal spread of the burst under the feet. |
| `speedMin / speedMax` | 20 / 55 | Puff velocity. Higher = flies further. |
| `dirMin / dirMax` | 60 / 120 | Fan angle upward. 90/90 = straight up. |
| `gravityY` | −180 | How hard particles fall back down. |
| `drag` | 3.5 | Air resistance. Higher = puffs stop faster. |
| `startSizeMin / Max` | 3 / 5 | Puff size in world px. |
| `startA / endA` | 0.75 / 0 | Alpha in/out (fade). |
| `Effects::walkDustPeriod` (`Effects.h`) | 0.18s | Cadence between puffs while walking. |

### Landing dust — `makeLandDust()`

Same fields as walk dust, tuned wider / bigger. Common tweaks:

| Field | Default | Effect |
|-------|---------|--------|
| `bursts[0].count` | 12 | Total puffs in the landing burst. |
| `shapeWidth` | 22 | Fan width under both feet. |
| `speedMax` | 140 | Splashiness. |
| `gravityY` | −260 | Snap-back to the ground. |

### Muzzle flash — `makeMuzzleFlash()`

Two layers: `core` (bright white-yellow puff, no velocity) and `sparks`
(narrow forward cone).

| Field | Default | Effect |
|-------|---------|--------|
| `core.startSizeMin/Max` | 10 / 14 | Flash puff size. |
| `core.lifetimeMax` | 0.09s | Flash duration. Short = snappy. |
| `sparks.bursts[0].count` | 6 | Number of spark streaks. |
| `sparks.speedMin/Max` | 160 / 300 | Streak reach. |
| `sparks.dirMin/Max` | −18 / +18 | Cone width (facing +1 direction). |

`Effects::spawnMuzzleFlash` mirrors the cone across the vertical axis
for facing = −1.

### Bullet casing — `makeCasing()`

| Field | Default | Effect |
|-------|---------|--------|
| `bursts[0].count` | 1 | Casings per shot. |
| `speedMin / speedMax` | 90 / 130 | Ejection speed. |
| `dirMin / dirMax` | 105 / 130 | Up + back cone (facing +1). |
| `gravityY` | −900 | Heavy: casings fall fast. |
| `rotVelMin / Max` | −12 / +12 | Tumble spin. |
| `startSizeMin / Max` | 3 / 4 | Casing "quad" size. |

`Effects::spawnCasing` uses the `(x, y)` coord you pass in directly, so the
caller (GameLayer) is responsible for supplying the ejection-port world
position — typically `player.x() + facing * 5` at gun height (`by`).

### Bullet impact — `makeBulletImpact()`

Two layers: `chunks` (tile-tinted, arcing) and `sparks` (yellow additive).

| Field | Default | Effect |
|-------|---------|--------|
| `chunks.bursts[0].count` | 10 | Tile-coloured chunks per hit. |
| `chunks.speedMax` | 260 | Chunk spread. |
| `chunks.gravityY` | −420 | Chunks arc down after the burst. |
| `sparks.bursts[0].count` | 12 | Metal spark count. |
| `sparks.speedMax` | 340 | Spark reach. |
| `sparks.lifetimeMax` | 0.22s | How long the yellow flash lingers. |

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| Unresolved `Effects::*` symbols at link time | `.vcxproj` missing the new file entries | Add `Effects.h` / `Effects.cpp` to `ClInclude` / `ClCompile` (doc 03). |
| `C2011: SpriteRenderer2D redefinition` when compiling `GameLayer.cpp` or `main.cpp` | You included `ParticleSystem.h` (directly or transitively) in `Effects.h`, so the TU now sees both `Sprite2D.h`'s and `SpriteRenderer2D.h`'s copy of the class. | Keep `ParticleSystem.h` in `Effects.cpp` only. `Effects.h` must forward-declare `HBE::Renderer::ParticleSystem` and hold it via `std::unique_ptr` (see doc 01). |
| `Effects::init` returns false | `ResourceCache` doesn't have the `"sprite"` shader registered yet | Ensure `buildSpritePipeline()` runs before `m_effects.init(...)`. In our layout it already does. |
| All tile-tinted effects look identical tan | `sampleTileTopColors` failed or the map's Ground tileset isn't at index 0 | Check the `LogInfo` output at startup. Currently `level_01.json` uses tileset 0 for both `Ground` and `Background`, so this should succeed. If a future map moves Ground to a different index, the fallback tan is still safe. |
| Muzzle flash appears at player centre | Missing muzzle coords | Confirm `consumeShot()` writes `m_shotX/Y` (already true in item 06). The layer must pass **the same** `bx, by, bdir` into `m_effects.spawnMuzzleFlash`. |
| Muzzle sparks always fan right, even when facing left | Missing `dir` mirror | Confirm `spawnMuzzleFlash` checks `dir < 0` and reflects `dirMin/Max` (doc 01). |
| Casing lands with no gravity | Wrong sign on `gravityY` | Engine uses **+Y = up**, so `gravityY = -900` pulls the casing down. A positive value would fling it into orbit. |
| Impact burst never fires | Bullets are going off-screen, not hitting tiles | Try shooting straight into a floor tile. Off-screen despawn is deliberately silent (see doc 02). |
| Impact burst fires but chunk colour is wrong | Layer index confusion | `Effects::spawnBulletImpact` patches `def[0]` (chunks). If you reorder layers in `makeBulletImpact`, patch the same index. |
| Walk dust never stops after releasing A/D | `moving` computed off `Player::velX()` (which lingers with recoil) instead of the input | Use the input axis `ix != 0.0f` for `moving` — recoil doesn't move the feet. |
| Walk dust puffs stream in Ghost mode | `groundTileId()` reporting non-zero in Ghost | Ghost mode never assigns `m_groundTileId`, so it stays 0 after mode-swap. If you added a `grounded()` accessor, gate on that instead. |
| Particle system feels laggy after many shots | `maxParticles` too low for the emitter, new particles being dropped silently | Raise `maxParticles` on the affected emitter (usually `bullet_impact_chunks/sparks`). |
| CPU spike at scene start | `sampleTileTopColors` on huge tilesets | One-time cost only; if it becomes an issue, cache the result on disk. Out of scope for item 07. |

---

## Result

The game now feels tactile:

- The player kicks up dust that **matches whatever ground he's on**.
- Landing after a jump adds a satisfying puff under his feet.
- Every shot has an obvious **muzzle flash** at the gun tip, ejects a
  **brass casing**, and — if it hits a tile — produces a **coloured chunk
  burst plus a bright yellow metal spark** at the impact point.
- Ghost mode stays clean and silent, so the map-building loop is
  unchanged.
- **All changes are MegaX-only.** No engine, no Sandbox edits. The three
  small game-side additions to `Player` and `BulletManager` (getters +
  an impact queue) are surfaced signals only; they don't alter any
  existing behaviour.

**Next work item:** Item 08 (first enemy — Robot Soldier with hit/hurt
boxes and health).
