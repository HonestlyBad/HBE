# Item 06 · Doc 04 — Build, Run & Verify

## Build

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "G:\Dev\HBE\HonestlyBadEngine.slnx" `
  /t:MegaX /p:Configuration=Debug /p:Platform=x64 /m /nologo /v:minimal
```

Expected tail:

```
  GameLayer.cpp
  Player.cpp
  Bullet.cpp
  main.cpp
  Generating Code...
  MegaX.vcxproj -> G:\Dev\HBE\MegaX\bin\x64\Debug\MegaX.exe
  MegaX: copying runtime deps to G:\Dev\HBE\MegaX\bin\x64\Debug\
```

Exit code `0`. (Verified clean on this machine.)

## Run

```powershell
& "G:\Dev\HBE\MegaX\bin\x64\Debug\MegaX.exe"
```

---

## Verify checklist

- [ ] **Shoot pose** — tap `J` standing: player snaps to the standing shoot pose
      (row 10) briefly, then returns to idle.
- [ ] **Run‑and‑gun** — hold `D`/`A` and tap `J`: the run‑and‑gun pose (row 5)
      plays while moving.
- [ ] **Air shoot** — jump and tap `J`: the airborne shoot pose (row 8) plays.
- [ ] **Auto‑fire** — hold `J`: bullets stream out at a steady cadence
      (~7/sec, `kFireCooldown = 0.15`).
- [ ] **Muzzle** — the bullet leaves the **gun tip**, not the player's center, in
      both facing directions.
- [ ] **Tile hit** — fire into a wall/floor: the bullet vanishes on contact with
      a solid `Ground` tile.
- [ ] **Off‑screen despawn** — fire across open space: the bullet disappears ~4
      tiles past the screen edge (it doesn't live forever).
- [ ] **Air recoil** — shoot in mid‑air: the player is nudged **backward**
      (opposite the way the gun points); on the ground there's no knockback.
- [ ] **Ghost mode** — press `G`, then `J`: **no** bullets (shooting is Play‑only).

---

## Tuning cheat‑sheet

### Player (`src/Game/Player.cpp`, top constants)

| Constant | Default | Effect |
|----------|---------|--------|
| `kFireCooldown` | 0.15 s | Time between auto‑fire shots (lower = faster). |
| `kShootHold` | 0.22 s | How long the shoot pose stays up per shot. |
| `kShootFps` | 14 | Shoot animation playback speed. |
| `kRecoilImpulse` | 120 px/s | Air knockback strength. |
| `kRecoilDamp` | 7 /s | How fast the knockback fades. |
| `kMuzzleFwdStand/Air/Crouch` | 30/30/28 | Forward gun‑tip offset per pose. |
| `kMuzzleUpStand/Air/Crouch` | 1/2/−14 | Vertical gun‑tip offset per pose. |
| `kShootStandRow` etc. | 10/5/8 | Which sheet rows the shoot poses use. |

### Bullets (`include/Game/Bullet.h`, public tunables)

| Member | Default | Effect |
|--------|---------|--------|
| `speed` | 640 px/s | Bullet travel speed. |
| `offscreenTiles` | 4 | Tiles past the view before despawn. |
| `length` / `height` | 16 / 4 px | Visual size of the bullet quad. |

Bullet colour: `m_item.tint = {1.0, 0.95, 0.4, 1.0}` in `Bullet::init`.

### Fire key

`GameLayer.cpp`: `SDL_SCANCODE_J`. Change both the `IsKeyPressed` and `IsKeyDown`
lines to remap.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| Unresolved `BulletManager` symbols | `.vcxproj` missing the new files | Add the `ClInclude`/`ClCompile` entries (doc 03). |
| Bullet spawns from player center | Muzzle offset ignored | Confirm `consumeShot` outputs `m_shotX/Y` and layer uses them in `spawn`. |
| Bullet passes through walls | Wrong solid layer | `m_ground` must be the `Ground` layer; check the map has it. |
| Bullets never despawn | Camera viewport 0 or map null | Ensure `m_bullets.update` gets the live camera and `&m_world.map()`. |
| No knockback in air | `onGroundPrev` true when it shouldn't be | Recoil only sets when `airborne`; verify jump leaves the ground. |
| Recoil pushes on the ground | Grounded reset missing | Keep `if (onGroundPrev) m_recoilVx = 0.0f;`. |
| Shooting works in Ghost mode | Block outside `updatePlay` | Shooting must live in `updatePlay`. |

---

## Result

The player now shoots. Tapping `J` fires a single sleek bullet from the gun tip
with a matching pose; holding `J` auto‑fires; bullets die on solid tiles or ~4
tiles off‑screen; and firing in mid‑air knocks the player back. All changes are
MegaX‑only — no engine or Sandbox edits.

**Next work item:** Item 07 (particle effects — muzzle flash, impact sparks,
etc.).
