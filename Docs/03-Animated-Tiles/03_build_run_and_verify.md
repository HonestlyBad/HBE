# Item 03 · Doc 03 — Build, run & verify

Everything in this item is MegaX‑only and needs **no project/build‑config change**:

- The 3 changed source files were already registered in `MegaX.vcxproj` (item 02).
- The animation sheets under `assets/tilesets/Military/Animated/` are copied to
  the output automatically by the asset glob (`assets\**\*`) on build.
- `GameLayer` already calls `m_world.update(dt)` and `m_world.render(r2d)`.

---

## 1. Build the solution

Build from Visual Studio (Build → Build Solution, `Debug | x64`), **or** from a
Developer PowerShell:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' `
  'G:\Dev\HBE\HonestlyBadEngine.slnx' `
  /t:Build /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal
```

Expect a clean build (exit code 0). This item adds no new warnings.

> If MSBuild reports it can't find the project, build from inside Visual Studio —
> the `.slnx` solution filter opens cleanly there.

---

## 2. Run

Launch **MegaX** (F5 in Visual Studio, or run
`G:\Dev\HBE\MegaX\bin\x64\Debug\MegaX.exe`).

On startup the log (stderr / VS Output) should show:

```
World loaded 'maps/level_01.json' (3 tilesets, 4 layers, 5 animated tiles, 5 instances).
```

- **4 layers** (was 3 in item 02) — the new `FG_Machines` layer.
- **5 animated tiles, 5 instances** — the 5 definitions, each placed once.

---

## 3. Verify the animation

You should see, on the floor near the center of the screen, a **row of 5
machines** that are **moving**: blinking screens, sweeping lights, running
circuits — cycling smoothly and looping ~once per second (at `fps: 8`).

Checklist:

- [ ] Console prints `5 animated tiles, 5 instances`.
- [ ] The 5 machine tiles on the floor visibly cycle (not frozen).
- [ ] They loop cleanly with no flicker or one‑frame "pop" at the wrap.
- [ ] The player still renders **in front of** the animated tiles.
- [ ] Frame edges are crisp (no bleeding from adjacent frames).

---

## 4. Troubleshooting

| Symptom | Likely cause | Fix |
|--------|--------------|-----|
| `0 animated tiles` in log | `"animatedTiles"` still `[]`, or JSON typo above it | Apply Edit 1 in doc 02; validate the JSON. |
| `N animated tiles, 0 instances` | The animated ids aren't placed in a tileset‑0 layer | Apply Edit 2 (the `FG_Machines` layer); confirm `"tileset": 0`. |
| `failed to load animated sheet '…'` | Wrong `sheet` path or missing file | Path is relative to the **map file** → `../tilesets/Military/Animated/tile-N_…png`. Confirm the file exists. |
| Tiles show but **don't move** | `World::update(dt)` not advancing, or `fps: 0` | Confirm `GameLayer` calls `m_world.update(dt)`; set a non‑zero `fps`. |
| All animated tiles show the **same wrong texture** | Sharing/mutating one `Material` | Each `AnimatedTile` must own its `material` (doc 01). Don't refactor to a shared material. |
| Animated tile drawn **behind** the static tile / not visible | Wrong render layer | `renderAnimatedTiles` must use `item.layer = 1` (above static 0). |
| Wrong frame region / neighbouring frame bleeds in | UV math altered | Keep `computeFrameUV` identical to doc 01 (vertical flip + half‑texel inset). |
| Animation **too fast/slow** | `fps` | Tune each entry's `fps` in `"animatedTiles"`. |
| Player renders **behind** the machines | Player layer changed | Player must stay at layer 100 (> 1). |

---

## 5. What you've built

- A data‑driven animated‑tile system living entirely in MegaX: define animations
  in the map JSON, place their ids like normal tiles, and they animate.
- Zero engine edits, no `HBE.Sandbox` reference, no build‑config churn.
- A reusable pattern (persistent material per animation + per‑frame `uvRect`) that
  the rest of the game — hazards, pickups, doors — can lean on later.

Item 03 complete. ✅
