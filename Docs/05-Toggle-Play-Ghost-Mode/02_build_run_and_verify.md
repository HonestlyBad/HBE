# Item 05 · Doc 02 — Build, Run & Verify

## 1. Apply the changes

| File | From doc |
|------|----------|
| `MegaX/src/Game/Player.cpp` | `01_code_changes.md` §1 (tint by mode) |
| `MegaX/src/Game/GameLayer.cpp` | `01_code_changes.md` §2 (`G` hot‑key + log) |

No header, engine, or project‑file changes.

---

## 2. Build

Visual Studio: **MegaX** startup project, **Debug | x64**, **Build → Rebuild
MegaX** (`Ctrl+B`).

Command line (VS 2026 Developer PowerShell), from `G:\Dev\HBE`:

```powershell
MSBuild HonestlyBadEngine.slnx /t:MegaX /p:Configuration=Debug /p:Platform=x64 /m
```

Expect `Build succeeded. 0 Error(s)`.

---

## 3. Run & verify

Press **F5**. The player starts in **Play** mode (opaque, falls to the ground).

| # | Test | Expected |
|---|------|----------|
| 1 | Launch | Player is **opaque** and lands on the ground (Play mode). |
| 2 | Press `G` | Player turns **translucent blue**; console logs "Ghost mode". |
| 3 | In Ghost, hold `A`/`D` | Flies left/right, **passes through** solid tiles. |
| 4 | In Ghost, hold `Space` / `S` | Flies **up** / **down**; **no gravity** (holds height when keys released). |
| 5 | Press `G` again | Player turns **opaque**; console logs "Play mode". |
| 6 | Back in Play | Gravity + collisions resume; jump/crouch work as in item 04. |
| 7 | Toggle mid‑air | No teleport/camera jump; camera keeps following smoothly. |
| 8 | `H` in either mode | Helmet still toggles (tint applies over both sheets). |

---

## 4. Tuning

| Knob | Where | Default | Effect |
|------|-------|---------|--------|
| Toggle key | `GameLayer.cpp` | `SDL_SCANCODE_G` | change to taste |
| Ghost tint | `Player.cpp` | `{0.6, 0.8, 1.0, 0.5}` | RGBA; lower A = more see‑through |
| Ghost fly speed | `Player.h` | `moveSpeed` (200) | shared with Play run speed |

> Want a distinct fly speed for Ghost? Add a separate `ghostSpeed` field and use
> it in `updateGhost` instead of `moveSpeed`. Not needed for this item.

---

## 5. Troubleshooting

**`G` does nothing.**
- Confirm the block is inside `onUpdate` and uses `Input::IsKeyPressed`
  (edge‑triggered), not `IsKeyDown` (which would flip every frame while held).

**Player is translucent in Play mode (or opaque in Ghost).**
- The tint condition is inverted, or it's set *before* the Play/Ghost dispatch.
  It must read `m_mode` in the shared block **after** `updatePlay/updateGhost`.

**No transparency at all in Ghost mode.**
- The material blend isn't `Alpha`. `Player`'s material uses the default
  `BlendMode::Alpha`; don't override it.

**Ghost still collides / falls.**
- You're still in Play mode — check the console log after pressing `G`. If it
  never logs, the key block wasn't added or the key is remapped.

---

## 6. What's next

Item 05 done: `G` toggles Play ⇄ Ghost, with a translucent‑blue tell and no
input rewiring. Item 06 adds **player shooting** — bullets, muzzle origin at the
gun tip, and in‑air knockback when firing during a jump.
