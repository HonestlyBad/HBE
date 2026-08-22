# Item 04 · Doc 03 — Build, Run & Verify

## 1. Apply the changes

| File | From doc |
|------|----------|
| `MegaX/include/Game/Player.h` | `01_player_physics_code.md` (full replacement) |
| `MegaX/src/Game/Player.cpp`   | `01_player_physics_code.md` (full replacement) |
| `MegaX/src/Game/GameLayer.cpp`| `02_gamelayer_wiring.md` (two edits) |

No other files change. `player-no-helm.png` must already sit next to `player.png`
in `MegaX/assets/sprites/Player/` (from item 01).

---

## 2. Build

In Visual Studio: set **MegaX** as the startup project, configuration
**Debug | x64**, then **Build → Rebuild MegaX** (or `Ctrl+B`).

Command line (VS 2026 "Developer PowerShell"), from the repo root
`G:\Dev\HBE`:

```powershell
MSBuild HonestlyBadEngine.slnx /t:MegaX /p:Configuration=Debug /p:Platform=x64 /m
```

Expect `Build succeeded. 0 Error(s)`. The output binary is
`G:\Dev\HBE\MegaX\bin\x64\Debug\MegaX.exe` (assets are mirrored next to it by the
post‑build step from item 00).

---

## 3. Run

Press **F5** (or run `MegaX.exe`). On launch:

- The player sprite appears at the map center and **falls** until it lands on the
  first solid `Ground` tile, then stands in the **idle** animation.

---

## 4. Verification checklist

| # | Test | Expected |
|---|------|----------|
| 1 | Launch | Player falls and **lands** on solid ground, then idles. |
| 2 | Hold `D` / `A` | Runs right / left with the **walk** animation, facing flips. |
| 3 | Run into a wall of solid tiles | Stops at the wall, doesn't pass through. |
| 4 | Tap `Space` | Jumps up ~3 tiles: **rise** (frames 1‑3) → **fall** (frame 4) → **landing** (frames 5‑6) on touchdown. |
| 5 | Tap vs hold `Space` | Quick tap = short hop; hold = full‑height jump (variable height). |
| 6 | Jump into a solid ceiling | Head stops, player falls back down (no clipping). |
| 7 | Walk off a ledge | Falls; a jump pressed within ~0.08 s of leaving still works (coyote). |
| 8 | Press `Space` just before landing | Jumps immediately on touchdown (jump buffer). |
| 9 | Hold `S` on the ground | **Prone crawl** animation, hitbox shrinks; moving with `A`/`D` runs at **half speed**. |
| 10 | Crouch under a 1‑tile gap, release `S` | Stays crouched while a solid is overhead; stands only when clear. |
| 11 | Tap `H` | Helmet toggles on/off across **every** animation (idle/walk/jump/crouch). |
| 12 | Move around | Camera follows smoothly (unchanged from item 01). |

---

## 5. Tuning (in `Player.cpp` / `Player.h`)

All values are world pixels & seconds. Start here, then taste‑test:

| Knob | Where | Default | Effect |
|------|-------|---------|--------|
| `gravity` | `Player.h` | 2100 | ↑ = heavier, faster fall |
| `jumpSpeed` | `Player.h` | 640 | ↑ = higher jump (~3 tiles) |
| `maxFall` | `Player.h` | 900 | terminal fall speed |
| `moveSpeed` | `Player.h` | 200 | run speed |
| `kJumpCut` | `Player.cpp` | 180 | lower = shorter min‑hop on early release |
| `kCoyote` | `Player.cpp` | 0.08 | ledge‑jump grace window |
| `kJumpBuf` | `Player.cpp` | 0.10 | pre‑land jump grace window |
| `kCrouchSpeedMul` | `Player.cpp` | 0.5 | run‑speed multiplier while crouched |
| `kLandTime` | `Player.cpp` | 0.16 | how long the landing animation shows |
| `kBoxW / kBoxStandH / kBoxCrouchH` | `Player.cpp` | 24 / 40 / 24 | collision box size |

**Jump height math:** peak ≈ `jumpSpeed² / (2·gravity)`. At 640/2100 ≈ **97 px ≈
3 tiles** (32 px tiles).

---

## 6. Troubleshooting

**Player falls straight through the world.**
- The `"Ground"` layer wasn't found (check the console for the warning) — verify
  the layer name in `maps/level_01.json` and the string in `onAttach`.
- The tiles under the spawn aren't in the tileset's `solidTiles`. `Tileset1H`
  lists `[1,2,3,4,7,8,9,10,14,15]`; make sure the floor uses those ids.

**Player floats above the floor or sinks into it.**
- The feet‑alignment formula is off. Keep `feetToCenterOffset()` and both
  `syncRenderFromBox()` / `setPosition()` exactly as written — the frame bottom
  must map to the box bottom.
- If your art has a different foot gap, nudge `kBoxStandH` (taller box lifts the
  sprite) rather than hacking the formula.

**Wrong animation plays for jump/crouch.**
- The row constants are read off the sheet by eye. Adjust `kJumpRow` (7) /
  `kCrouchRow` (9), or the `kRiseCol*/kFallCol*/kCrouchCol*` ranges in
  `Player.cpp` to match your `player.png`.

**Player spawns in mid‑air and falls a long way / out of the map.**
- The map center has no floor. Set `startX/startY` in `onAttach` to a spot above
  solid ground (e.g. lower `startY`, or pick an X over a platform).

**Jump feels mushy / too floaty.**
- Raise `gravity` and/or lower `kJumpCut`. Increase `jumpSpeed` if the peak is too
  low after raising gravity.

**Player jitters between jump and idle while standing.**
- That's what the `m_coyote`‑based `onGround` debounce prevents; make sure the
  animation selection uses `m_grounded || m_coyote > 0.0f`, not raw `m_grounded`.

**Can't stand up after crouching.**
- Intended when a solid tile is directly overhead. Move out from under it first.
  If it happens in the open, `kBoxStandH` may be taller than the ceiling gap —
  reduce it or check the tile above isn't wrongly flagged solid.

---

## 7. What's next

Item 04 is complete: gravity, run, variable jump, crouch, and solid‑tile
collisions, all MegaX‑only. The Ghost free‑fly path is preserved behind
`Player::Mode`; **item 05** adds the hot‑key that calls `m_player.toggleMode()`
to switch between Play and Ghost — no further physics work required.
