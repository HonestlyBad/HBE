# 05 — Build, run, verify

You've now touched every file listed in `00_overview.md` §4:

* **NEW** `HBE.Core\include\HBE\Core\Profiler.h`
* **NEW** `HBE.Core\src\Core\Profiler.cpp`
* **EDIT** `HBE.Core\include\HBE\Core\Application.h`
* **EDIT** `HBE.Core\src\Core\Application.cpp`
* **EDIT** `HBE.Core\HBE.Core.vcxproj`
* **EDIT** `HBE.Core\HBE.Core.vcxproj.filters`
* **EDIT** `MegaX\src\Game\GameLayer.cpp` (verification only)

No new files in MegaX, no new files under
`HBE.Platform.SDL`, `HBE.Renderer.GL`, Sandbox, or MapMaker.
No project (`.vcxproj`) edits in MegaX.

---

## 1. Project file sanity check

Before building, verify the project registrations match:

```powershell
Get-Content G:\Dev\HBE\HBE.Core\HBE.Core.vcxproj | Select-String "Profiler"
# Expect EXACTLY 2 matches:
#   <ClInclude Include="include\HBE\Core\Profiler.h" />
#   <ClCompile Include="src\Core\Profiler.cpp" />

Get-Content G:\Dev\HBE\HBE.Core\HBE.Core.vcxproj.filters | Select-String "Profiler"
# Same — 2 matches, matching entries.

Get-Content G:\Dev\HBE\MegaX\MegaX.vcxproj | Select-String "GameLayer.cpp"
# Expect EXACTLY 1 match — MegaX vcxproj is UNCHANGED from Item 12.
```

If you touched `MegaX.vcxproj` by mistake, revert it — Item
13 must not modify it (rule 1 in `00_overview.md`).

---

## 2. Build

From a `vcvars64.bat`-initialized cmd prompt:

```
msbuild HonestlyBadEngine.slnx /p:Configuration=Debug /p:Platform=x64 /t:MegaX /m /nologo /v:m
```

Expected new lines (touched TUs recompile — plus the new
`Profiler.cpp` from scratch):

```
  Profiler.cpp
  Application.cpp
  GameLayer.cpp
  HBE.Core.vcxproj -> G:\Dev\HBE\...\HBE.Core.lib
  MegaX.vcxproj -> G:\Dev\HBE\MegaX\bin\x64\Debug\MegaX.exe
```

Only pre-existing warnings should appear
(`C4099 TileMap`, `C4244 InputMap`, `C4305` in `Effects.cpp`
from prior items). If you see:

* **`error C1083: Cannot open include file: 'HBE/Core/Profiler.h'`**
  — Profiler.h wasn't saved to the right path, OR the
  `<ClInclude>` entry in `HBE.Core.vcxproj` is missing.

* **`error LNK2019: unresolved external symbol "public: __cdecl HBE::Core::Profiler::ScopeTimer::ScopeTimer(...)"`**
  — `Profiler.cpp` isn't listed in `<ClCompile>` in
  `HBE.Core.vcxproj` (doc `02` §2).

* **`error C2039: 'Profiler': is not a member of 'HBE::Core'`**
  — you forgot `#include "HBE/Core/Profiler.h"` in
  `Application.h` (doc `03` §1) or `GameLayer.cpp` (doc `04`
  §1).

* **`warning C4189: '_hbe_prof_...' local variable is initialized but not referenced`**
  in a Release build — `HBE_PROFILE_ENABLED` isn't being
  defined to `0`. Check that neither Option A nor Option B
  from doc `03` §3 has "forced on in Release" by accident.

### Release build check

Also run the Release build to prove zero-overhead expansion:

```
msbuild HonestlyBadEngine.slnx /p:Configuration=Release /p:Platform=x64 /t:MegaX /m /nologo /v:m
```

Expected:

* Builds clean.
* No output from the 1-Hz Profiler log (the block is guarded
  by `HBE_PROFILER_LOG_ONCE_PER_SECOND` which is only
  defined in Debug).
* Roughly the same frame timings as Item 12's Release build
  — the profiler macros expanded to `((void)0)`.

---

## 3. Run (Debug)

```
G:\Dev\HBE\MegaX\bin\x64\Debug\MegaX.exe
```

Expected startup log lines (unchanged from Item 12):

```
[INFO ] World loaded 'maps/level_01.json' ...
[INFO ] MegaX GameLayer attached ...
```

After ~1 second of runtime, a **new** log block should
appear once per second, driven by the
`HBE_PROFILER_LOG_ONCE_PER_SECOND` block added in
`Application::run` (doc `03` §2.2):

```
[INFO ] [Profiler] Frame=16.42ms (avg 16.51 min 15.98 max 18.20)
[INFO ]   ApplicationUpdate  cur=  8.31 avg=  8.42 min=  7.90 max=  9.55 (n=120)
[INFO ]     SceneUpdate      cur=  8.10 avg=  8.20 min=  7.72 max=  9.30 (n=120)
[INFO ]       Physics        cur=  1.20 avg=  1.18 min=  1.05 max=  1.42 (n=120)
[INFO ]       Combat         cur=  0.55 avg=  0.51 min=  0.35 max=  0.83 (n=120)
[INFO ]       AI             cur=  2.40 avg=  2.35 min=  2.10 max=  2.71 (n=120)
[INFO ]       Particles      cur=  1.10 avg=  1.03 min=  0.88 max=  1.42 (n=120)
[INFO ]   Audio              cur=  0.09 avg=  0.08 min=  0.05 max=  0.15 (n=120)
[INFO ]   TileRendering      cur=  2.20 avg=  2.15 min=  1.98 max=  2.60 (n=120)
[INFO ]   SpriteRendering    cur=  4.00 avg=  3.98 min=  3.60 max=  4.42 (n=120)
```

Exact numbers vary by machine. Structure must match:

* **9 named sections** total.
* `SceneUpdate` indented under `ApplicationUpdate` (depth
  1 = 2 spaces).
* `Physics`, `Combat`, `AI`, `Particles` indented under
  `SceneUpdate` (depth 2 = 4 spaces).
* `Audio`, `TileRendering`, `SpriteRendering` at depth 0
  (no indent).
* Every row's `avgMs` is in millisecond units.

---

## 4. Verify checklist

### Group A — Item 12 regression (no new keys pressed)

* [ ] Walk, jump, shoot — same feel as Item 12.
* [ ] Enemy walk dust, muzzle flash, bullet impact, blood
      splatter, hit spark, explosion — all still fire.
* [ ] Difficulty pill retints on F1/F2/F3.
* [ ] F5 reload still works (Item 11 behavior unchanged).

### Group B — Profiler basics

* [ ] The `[Profiler]` block prints once per ~1 second in the
      Debug run, never more, never less.
* [ ] The nine named sections listed in §3 all appear.
* [ ] `avgMs` values are stable to within ~10% frame-to-
      frame after the first two seconds of runtime (the
      rolling window is 120 frames; after ~2s it's saturated).
* [ ] `minMs` / `maxMs` bracket every `currentMs` reading
      you observe.

### Group C — Nesting sanity

* [ ] Ballpark check:
      `SceneUpdate.currentMs >= Physics + Combat + AI + Particles`
      (allow ~10-20% "residual" for the small things
      intentionally not wrapped — input polling, camera
      update, walk-dust burst).
* [ ] `ApplicationUpdate.currentMs >= SceneUpdate.currentMs`
      (approximately equal, since MegaX only pushes one
      layer).
* [ ] `frameMs >= ApplicationUpdate + Audio + TileRendering + SpriteRendering`
      (larger by whatever the GL command submission + SDL
      swap takes — that time is not currently scoped).

### Group D — Runtime toggle

* [ ] From a debugger, set a breakpoint after
      `Application::run`'s `while (m_running)` loop begins
      and call `HBE::Core::Profiler::SetEnabled(false)` in
      the Immediate window.
* [ ] Continue. The 1-Hz log line still prints but the
      `currentMs` values freeze at their last valid
      readings, `avgMs / minMs / maxMs` stop moving.
* [ ] Call `SetEnabled(true)` — the values resume updating.

### Group E — Scene reload interaction (Item 11)

* [ ] Press F5. Item 11's reload log line fires.
* [ ] The next `[Profiler]` line shows sane values — no
      NaN, no zero. The rolling averages **do NOT reset**
      by default (see rule 4 in `00_overview.md`).
* [ ] If you want a reset, doc `01` §1 exposes
      `HBE::Core::Profiler::Reset()`. Add a call inside
      `GameLayer::reloadScene(...)` if you like — but that
      change is optional and outside Item 13's scope.

### Group F — Release build

* [ ] Release EXE runs. No `[Profiler]` log lines appear.
* [ ] `GetSnapshot()` still returns a valid `Snapshot`
      (empty `sections`) if any code path outside the
      macros calls it. Verified via runtime debug attach
      only — not something you'd hit in normal play.

---

## 5. Troubleshooting matrix

| Symptom | Likely cause | Fix |
|---|---|---|
| No `[Profiler]` block ever prints | `HBE_PROFILER_LOG_ONCE_PER_SECOND` not defined | Re-do doc 03 §3 Option A or B; recompile Debug |
| Only `Frame=...` line, no sections | `HBE_PROFILE_ENABLED` expanded to 0 | You're likely in a Release build even though you thought Debug — check `Configuration=Debug` in msbuild |
| `SceneUpdate` shows up but not `Physics/Combat/AI/Particles` | One of the `{ HBE_PROFILE_SCOPE(...); }` wraps in doc 04 §2.2 was skipped | Re-open `GameLayer.cpp` and count `HBE_PROFILE_SCOPE` — must be exactly 7 |
| `ApplicationUpdate` missing | `Application::run` change from doc 03 §2.2 didn't take | Re-open `Application.cpp` at line 320-366, re-apply |
| `Audio` missing but `Application/Scene` present | Scope wrap around `m_audio.update(dt)` didn't take | Re-do doc 03 §2.2 |
| `TileRendering/SpriteRendering` missing | `onRender` edits didn't take | Re-do doc 04 §3 |
| A section shows extreme `maxMs` (e.g. 500ms) | Alt-tab / lost-focus stall recorded in the rolling window | Play uninterrupted for 5+ seconds — `maxMs` will decay naturally as the ring fills with clean samples |
| `Physics.currentMs` = 0.00 but the game is running | `Physics` scope wraps only the shot-consume block by mistake, not `m_player.update(dt)` | Re-check doc 04 §2.2 "Physics" — the scope opening brace must be **before** `m_world.update(dt);` |
| Duplicate sections in the printout (e.g. `Physics` twice) | You added the scope wrap AND kept a bare `HBE_PROFILE_SCOPE("Physics")` on a different line | Grep for `HBE_PROFILE_SCOPE(\"Physics\")` — must be exactly 1 hit |
| Link error `unresolved external symbol NowNs` | `Profiler.cpp` not added to project | Re-do doc 02 §2 |
| Header include recursion / re-definition | `HBE_PROFILE_ENABLED` defined **before** the `#pragma once` in a `.cpp` — that's fine — or defined twice with different values across TUs (undefined behavior) | Grep for `HBE_PROFILE_ENABLED` and consolidate to project-level preprocessor definition |
| Release build spams profiler logs | The 1-Hz log define leaked into Release configuration | Doc 03 §3 Option A — remove `HBE_PROFILER_LOG_ONCE_PER_SECOND` from the `Release|x64` `<ItemDefinitionGroup>` |

---

## 6. Manual repro for the "5+ nested systems" done-criterion

Item 13 line 3: *"MegaX can time at least five nested
systems and read accurate millisecond values after every
frame."*

You have satisfied it if the Debug run prints, every second,
a section list that includes:

1. `ApplicationUpdate` (depth 0)
2. `SceneUpdate` (depth 1 — nested inside 1)
3. `Physics` (depth 2 — nested inside 2)
4. `AI` (depth 2)
5. `Combat` (depth 2)
6. `Particles` (depth 2)

That's **6 nested sections** across three levels of
nesting, plus the three "sibling" sections (`Audio`,
`TileRendering`, `SpriteRendering`) at depth 0. Well past
the bar of "at least five nested systems".

---

## 7. What comes after this item

Item 14 (`[HBE] Add GPU Timing and Expanded Renderer
Statistics`) extends this API with:

* `GpuTimer` — a companion RAII scope for GL timer queries.
* Renderer statistics (draw calls, quads, culled sprites, etc.).
* A combined per-frame snapshot that pairs a CPU section
  list with a GPU section list.

Item 15 (`[MEGAX] Create a Repeatable Performance Capture`)
consumes both the CPU snapshot from Item 13 and the GPU
snapshot from Item 14, plus game-side counters (enemies,
bullets, particles), and dumps them to a CSV.

Item 13's `GetSnapshot()` is intentionally minimal so Item
15 can layer on top without further engine changes.

---

## 8. Cleanup

Once verified, decide the long-term fate of the 1-Hz log:

* Keep it on for the whole Item 13/14/15 arc (recommended
  — it's the primary way to eyeball timing during
  development).
* Turn it off later by removing the preprocessor define (or
  the local `#define` in Option B).
* Do **not** delete the `#if defined(HBE_PROFILER_LOG_ONCE_PER_SECOND)`
  block from `Application.cpp` — a future work item will
  need the same pattern.

Item 13 is complete.
