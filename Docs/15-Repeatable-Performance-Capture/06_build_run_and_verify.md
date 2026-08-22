# 06 — Build, run, verify

Final doc for Item 15. Docs `01`–`04` are all applied before
you start here; doc `05` is reference material you can read
at any time.

---

## 1. Files touched recap

* **NEW** `MegaX/include/Game/PerfCapture.h` — 178 lines
* **NEW** `MegaX/src/Game/PerfCapture.cpp` — 625 lines
* **EDIT** `MegaX/CMakeLists.txt` — one source entry, one
  `option()` + `if()` block
* **EDIT** `MegaX/include/Game/GameLayer.h` — 58 → 65 lines
* **EDIT** `MegaX/src/Game/GameLayer.cpp` — 579 → 608 lines
* **EDIT** `MegaX/src/main.cpp` — 36 → 50 lines

No changes anywhere under `HBE.Core/`,
`HBE.Platform.SDL/` or `HBE.Renderer.GL/`. No changes to
`HBE.Sandbox/` or `HBMapMaker/`. No changes to any other
file under `MegaX/`, including every asset, map and shader.
No `.vcxproj` / `.slnx` edits — CMake is the only build
system.

Confirm the blast radius before building:

```fish
cd /home/atulo/Projects/HBE
git status --short
```

Every line should start with `MegaX/` or `Docs/`. If
anything under `HBE.` appears, revert it — a `[MEGAX]` item
does not touch the engine.

---

## 2. Pre-build sanity check

Each of these is a "did I miss a step?" checkpoint. A wrong
number means going back to the doc named beside it.

```fish
cd /home/atulo/Projects/HBE

# doc 01 — the header exists and pulls in nothing from HBE
test -f MegaX/include/Game/PerfCapture.h; echo $status
# -> 0
grep -c "HBE/" MegaX/include/Game/PerfCapture.h
# -> 0

# doc 02 — the .cpp exists, is registered, and reads all 9 sections
test -f MegaX/src/Game/PerfCapture.cpp; echo $status
# -> 0
grep -c "PerfCapture.cpp" MegaX/CMakeLists.txt
# -> 1
grep -c "MEGAX_PROFILE_IN_RELEASE" MegaX/CMakeLists.txt
# -> 2
grep -c "sectionMs(snap" MegaX/src/Game/PerfCapture.cpp
# -> 9

# doc 03 — GameLayer is wired
grep -c "m_perf" MegaX/include/Game/GameLayer.h
# -> 2
grep -c "m_perf" MegaX/src/Game/GameLayer.cpp
# -> 4
grep -c "HBE_PROFILE_SCOPE" MegaX/src/Game/GameLayer.cpp
# -> 8   (item 14 had 7; GameRender is the new one)
grep -c "SDL_SCANCODE_F9" MegaX/src/Game/GameLayer.cpp
# -> 1
grep -c "tickPerfCapture" MegaX/src/Game/GameLayer.cpp
# -> 2

# doc 04 — main takes the command line
grep -c "argc" MegaX/src/main.cpp
# -> 3
grep -c "captureRequest" MegaX/src/main.cpp
# -> 3

# the CSV header really has 27 columns
grep '"frame,t,frameMs' MegaX/src/Game/PerfCapture.cpp | head -1
# -> the first of the six string-literal fragments that build kCsvHeader
```

And one negative check — the engine must be untouched:

```fish
git diff --name-only -- HBE.Core HBE.Platform.SDL HBE.Renderer.GL | wc -l
# -> 0
```

---

## 3. Build

```fish
cd /home/atulo/Projects/HBE
cmake --build --preset linux-clang-debug --target MegaX
```

Expected recompiles (indices vary):

```
[43/62] Building CXX object MegaX/CMakeFiles/MegaX.dir/Debug/src/Game/PerfCapture.cpp.o
[51/62] Building CXX object MegaX/CMakeFiles/MegaX.dir/Debug/src/main.cpp.o
[52/62] Building CXX object MegaX/CMakeFiles/MegaX.dir/Debug/src/Game/GameLayer.cpp.o
[62/62] Linking CXX executable bin/Debug/MegaX
```

Because `GameLayer.h` now includes `PerfCapture.h`, **every**
MegaX translation unit that includes `GameLayer.h`
recompiles — that is `main.cpp` and `GameLayer.cpp` only, so
the rebuild stays small.

Only pre-existing warnings should appear. MegaX already
emits one:
``warning: unused function 'makeCasing'`` from
`Effects.cpp:144`. Anything else is yours. In particular, if
you see:

* **``fatal error: 'Game/PerfCapture.h' file not found``**
  — the header from doc `01` §1 was not saved to
  `MegaX/include/Game/PerfCapture.h`.

* **``undefined reference to 'MegaX::ParseCaptureArgs(int, char**)'``**
  — `PerfCapture.cpp` is missing from the `add_executable`
  list (doc `02` §2).

* **``undefined reference to 'HBE::Core::Profiler::SetEnabled(bool)'``**
  — you added a call to `SetEnabled`/`IsEnabled`. Remove it;
  `Profiler.cpp` defines the lowercase spellings only
  (`00_overview.md` golden rule 5).

* **``error: no member named 'tickPerfCapture' in 'MegaX::GameLayer'``**
  — doc `03` §1.3 was skipped.

* **``error: no member named 'aliveCount' in 'MegaX::EnemyManager'``**
  — you are on an older `EnemyManager.h`; the accessor has
  existed since item 09 at `EnemyManager.h:86`.

* **``error: call to implicitly-deleted copy constructor of 'std::unique_ptr<...>'``**
  — the `std::move` in `app.pushLayer(std::move(gameLayer))`
  was dropped (doc `04` §2).

* **``warning: format specifies type 'long' but the argument has type ...``**
  — the `%ld` for `rssKB` was changed, or `readResidentKB`
  no longer returns `long`.

* **``ninja: error: 'src/Game/PerfCapture.cpp', needed by ..., missing``**
  — CMake has the entry but the file is not on disk. Check
  the path, then re-run `cmake --preset linux-clang`.

### Release build check

```fish
cmake --build --preset linux-clang-release --target MegaX
```

Expected: builds clean. `HBE_PROFILE_SCOPE` expands to
`((void)0)`, so the new `"GameRender"` scope disappears
along with the other seven. `PerfCapture` still compiles and
runs — it does not use the macro anywhere. A Release capture
writes real `frameMs`, `gpuMs`, `passes` and count columns,
and `0.0000` for every per-section column.

### Release with MegaX's scopes kept

```fish
cmake --preset linux-clang -DMEGAX_PROFILE_IN_RELEASE=ON
cmake --build --preset linux-clang-release --target MegaX
```

Expected: builds clean, and a Release capture now fills
`renderMs`, `sceneUpdateMs`, `physicsMs`, `combatMs`,
`aiMs`, `particlesMs`, `tileRenderMs` and
`spriteRenderMs`. `updateMs` stays `0.0000` — that scope is
opened inside `HBE.Core`, which is still compiled without
it.

Turn it back off when you are done:

```fish
cmake --preset linux-clang -DMEGAX_PROFILE_IN_RELEASE=OFF
```

---

## 4. Run

```fish
cd /home/atulo/Projects/HBE
./build/linux-clang/bin/Debug/MegaX --help
```

Expected — ten lines and an immediate exit, no window:

```
[INFO]MegaX performance capture options:
[INFO]  --capture                 start a capture automatically after the warmup
[INFO]  --capture-seconds <N>     how long to record (default 10)
[INFO]  --capture-warmup <N>      seconds to run before recording (default 3)
[INFO]  --capture-label <text>    label folded into the output filename
[INFO]  --capture-out <path>      explicit .csv path (default: user data captures/)
[INFO]  --capture-120             score the run against the 120 FPS budget
[INFO]  --capture-quit            quit once the capture is written
[INFO]  --no-vsync                run uncapped so the capture measures real headroom
[INFO]  --help                    print this list and exit
[INFO]In-game, F9 starts/stops a capture with the same settings.
```

Then the unattended baseline:

```fish
./build/linux-clang/bin/Debug/MegaX --capture --capture-seconds 10 \
    --capture-warmup 3 --no-vsync --capture-label baseline --capture-quit
```

Startup is unchanged from item 14 except for one new line —
`[PerfCapture] armed`, which appears right after
`Application initialized.` because `setCaptureRequest` runs
before `pushLayer`:

```
[INFO]OpenGL context created successfully.
[INFO]SDLPlatform initialized successfully.
[INFO]AssetPaths: asset root    = /home/atulo/Projects/HBE/build/linux-clang/bin/Debug/assets
[INFO]AssetPaths: user data root = /home/atulo/.local/share/MegaX/MegaX/
[INFO]OpenGL initialized via GLAD. Version 3.3
[INFO]Audio initialized (SDL3_mixer MIX_* API).
[INFO]Application initialized.
[INFO][PerfCapture] armed: 3.0 s warmup, then 10.0 s of capture (60fps budget).
[INFO]World loaded 'maps/level_01.json' (2 tilesets, 2 layers, 5 animated tiles, 245 instances).
[INFO]MegaX GameLayer attached (Play mode; press G for Ghost).
[INFO]MegaX GameLayer: scene watches active (maps/level_01.json, sprite shader).
```

Three seconds later:

```
[INFO][PerfCapture] recording 10.0 s (command line) — scene 'maps/level_01.json'.
```

and ten seconds after that:

```
[INFO][PerfCapture] wrote 5002 rows -> /home/atulo/.local/share/MegaX/MegaX/captures/megax_perf_debug_baseline_20260815-234110.csv
[INFO][PerfCapture] wrote summary -> /home/atulo/.local/share/MegaX/MegaX/captures/megax_perf_debug_baseline_20260815-234110.meta.txt
[INFO]========== [PerfCapture summary] ==========
[INFO]frames 5002 over 10.00 s  (avg 1107.6 FPS, budget 60fps)
[INFO]frame ms  avg 0.903  p50 0.780  p95 1.734  p99 1.846  max 4.148
[INFO]update avg 0.049  render avg 0.808  gpu avg 0.172
[INFO]peak draws 0  quads 0  particles 8  entities 3  rss 183128 KiB
[INFO]over budget 0 frames (0.0 %)  ->  PASS
[INFO]===========================================
[INFO]MegaX: capture finished, --capture-quit requested. Exiting.
[INFO]Application exiting run loop.
```

Exact numbers vary by machine, driver and how busy the
desktop is. What must match structurally:

* Two `wrote ...` lines, one `.csv` and one `.meta.txt`,
  same stem, both under the user-data root.
* Row count roughly `duration × avgFps`, never `0`.
* `frame ms` line has all five statistics and
  `min ≤ p50 ≤ p95 ≤ p99 ≤ max`.
* `update avg` + `render avg` is less than the frame
  average — they are components of it, not additions.
* `peak draws 0  quads 0` — expected today, see
  `00_overview.md` golden rule 7.
* `over budget 0 frames (0.0 %)  ->  PASS` on any machine
  that can run MegaX at all, given how empty the test room
  is.
* The process exits on its own with status 0.

Those numbers are from a real Debug run of the current test
room, uncapped: about 1100 FPS and a p95 of 1.73 ms against
a 16.67 ms budget. An almost-empty room *should* look like
that.

With **vsync on** (drop `--no-vsync`) the same run instead
reports roughly `frame ms avg 16.6` and `avg 60 FPS`, and
about 600 rows for ten seconds. That is the display, not the
game — see golden rule 8.

---

## 5. Verify checklist

### Group A — item 14 regression (nothing new pressed)

* [ ] Game launches, the room draws, the player walks with
      `A`/`D` and jumps with `SPACE`.
* [ ] `E` fires, muzzle flash and casings spawn, bullets
      impact tiles.
* [ ] `G` toggles Ghost mode; `H` toggles the helmet; `B`
      toggles the hitbox overlay.
* [ ] `F1`/`F2`/`F3` still switch difficulty and refill HP.
* [ ] `F5` full reload, `F6` shader reload, `F7` soft
      respawn all still work.
* [ ] `F8` still prints exactly one
      `========== [Profiler F8 Snapshot] ==========` block.
* [ ] The 1-Hz `[Profiler]` block still appears, with the
      same CPU sections as item 14 **plus** `GameRender`
      between `Audio` and `TileRendering`, and with
      `TileRendering` / `SpriteRendering` now indented one
      level deeper as its children.
* [ ] Frame times in Debug are unchanged from item 14 — the
      capture adds nothing measurable while idle.

### Group B — `F9` works

* [ ] Press `F9`. The log prints
      `[PerfCapture] recording 10.0 s (F9) — scene 'maps/level_01.json'.`
* [ ] Ten seconds later two files are written and a summary
      block prints.
* [ ] The `.meta.txt` says `started by      : F9`.
* [ ] Press `F9`, then `F9` again after two seconds: the log
      prints `[PerfCapture] stopped early (F9).` and the CSV
      has roughly two seconds of rows, not ten.
* [ ] Press `F9` again after a capture finished: a second,
      independent capture starts and writes its own pair of
      files with a new timestamp.
* [ ] Pressing `F9` changes nothing on screen — no hitch, no
      pause, no HUD change.

### Group C — the CSV is well formed

* [ ] `head -1` of the CSV is the 27-column header, exactly
      as in doc `05` §1.
* [ ] `awk -F, 'NR>1 && NF!=27' capture.csv` prints nothing.
* [ ] `frame` increases monotonically with no duplicates:
      `awk -F, 'NR>2 && $1<=p {print NR} {p=$1}' capture.csv`
      prints nothing.
* [ ] `t` starts at `0.0000` and ends within ~1 frame of the
      requested duration.
* [ ] `frameMs` is never `0.0000` and never negative.
* [ ] `passes` is `1` on every row.
* [ ] `entities` equals `1 + enemies + bullets + enemyBullets`
      on every row:
      `awk -F, 'NR>1 && $21 != 1+$22+$23+$24' capture.csv`
      prints nothing.

### Group D — the numbers describe reality

* [ ] `renderMs ≥ tileRenderMs + spriteRenderMs` on
      essentially every row (they are nested inside it).
* [ ] `frameMs ≥ updateMs + renderMs` on essentially every
      row.
* [ ] `sceneUpdateMs ≥ physicsMs + combatMs + aiMs + particlesMs`.
* [ ] Shooting during a capture makes `particles` and
      `bullets` climb in the CSV.
* [ ] Pressing `F5` mid-capture produces one clear `frameMs`
      spike, and the capture keeps recording through it.
* [ ] `gpuMs` is `≥ 0` on a desktop GL 3.3 driver. If it is
      `-1.0000` on every row, the driver has no
      `ARB_timer_query` — that is item 14's documented
      fallback, and a pass.
* [ ] `rssKB` is stable to within a few hundred KiB across a
      ten-second capture.

### Group E — the command line

* [ ] `--help` exits 0 without opening a window.
* [ ] `--capture-seconds 5` alone starts a capture (it
      implies `--capture`).
* [ ] `--capture-seconds=5` works identically to
      `--capture-seconds 5`.
* [ ] `--capture-out /tmp/x.csv` writes `/tmp/x.csv` and
      `/tmp/x.meta.txt`, and nothing lands in the user-data
      root.
* [ ] `--capture-quit` exits on its own, status 0:
      `echo $status` → `0`.
* [ ] `--capture-120` changes the `budget :` line of the
      `.meta.txt` to `120fps` and the log's
      `budget 120fps`.
* [ ] `--no-vsync` changes the `vsync :` line to `off` and
      drops the average frame time far below 16.6 ms.
* [ ] `--capture-bogus` warns
      `unknown option '--capture-bogus' — ignored.` and the
      game runs normally.
* [ ] `--capture-seconds abc` warns
      `bad value for --capture-seconds ('abc') — keeping the default.`

### Group F — repeatability

* [ ] Run the baseline command twice, hands off the
      keyboard. The two `.meta.txt` files agree on `p50` to
      within a few percent.
* [ ] Both runs report the same `scene`, `difficulty`,
      `build config`, `vsync` and `megax scopes` lines.
* [ ] The two CSVs have similar row counts (within a few
      percent).

### Group G — Release

* [ ] Release builds and runs.
* [ ] A Release capture writes a CSV with real `frameMs`,
      `gpuMs` and `passes`, and `0.0000` in every
      per-section column.
* [ ] The `.meta.txt` says
      `megax scopes    : off (HBE_PROFILE_ENABLED=0)` and
      `build config    : release`.
* [ ] The output filename contains `_release_`, not
      `_debug_`.
* [ ] With `-DMEGAX_PROFILE_IN_RELEASE=ON`, `renderMs` and
      the seven MegaX section columns fill in, `updateMs`
      stays `0.0000`, and the `.meta.txt` says
      `megax scopes    : on`.

---

## 6. Budgets and capture knobs

The budgets live in `MakeBudget60()` and `MakeBudget120()`
in `MegaX/src/Game/PerfCapture.cpp` (doc `02` §1.2). Edit
and rebuild — there is no data file and no hot reload for
them, deliberately: a budget that changes without a rebuild
is a budget nobody trusts.

The capture switches need no rebuild at all; change the
command line and run again.

### When a capture says `OVER BUDGET`

The verdict is `PASS` only when **all four** hold: `p95
frameMs ≤ budget.frameMs`, `peak drawCalls ≤
budget.drawCalls`, `peak submittedQuads ≤
budget.submittedQuads`, and `peak particles ≤
budget.particles`. Find which one failed in the
`.meta.txt`, then:

| Symptom | Knob | Direction |
|---|---|---|
| p95 is over but p50 is fine — occasional stutter, not a slow game | investigate first; do not move the budget. Look for the `frameMs` spikes in the CSV and what `entities` / `particles` were doing on those rows | — |
| Everything is over and the room genuinely got heavier (item 20 landed) | `MakeBudget60().frameMs` | `16.67 → 16.67` (do **not** move it; 60 FPS is 16.67 ms by definition — cut work instead) |
| `renderMs` is the whole frame and `tileRenderMs` dominates it | not a budget knob — this is the tile chunk culling item 30 is scheduled to do | — |
| `particles` peaks over 1500 during a firefight | `MakeBudget60().particles` | `1500 → 2500`, only after confirming `particlesMs` is still small |
| Draw calls exceed 64 once real content lands | `MakeBudget60().drawCalls` | `64 → 96` |
| You want the aspirational bar for a specific run | add `--capture-120` | — |
| The 120 FPS sub-budgets feel arbitrarily tight | `MakeBudget120().updateMs` / `.renderMs` | `3.00 / 2.00 → 3.50 / 2.50` |

### Capture shape

| Symptom | Knob | Direction |
|---|---|---|
| The first second of every capture is full of spikes | `--capture-warmup` | `3 → 5` |
| Ten seconds is too short to catch a rare hitch | `--capture-seconds` | `10 → 30` |
| A long capture stops early with `sample cap reached` | `PerfCapture::kMaxSamples` in `PerfCapture.h` | `40000 → 120000` (about 17 MB of samples) |
| Two runs disagree wildly and you cannot see why | check the `vsync :` and `megax scopes :` lines in both `.meta.txt` first — they explain most of it | — |
| Captures pile up and you cannot tell them apart | `--capture-label` | always pass one |
| You want the capture next to a script instead of in the user-data root | `--capture-out` | — |

### Ordering constraint

`--no-vsync` must be on the command line, not toggled later:
`main.cpp` applies it to `WindowConfig::vsync` **before**
`app.initialize`, which is when SDL sets the GL swap
interval. There is no in-game vsync toggle, and adding one
would be an engine change.

---

## 7. Troubleshooting matrix

| Symptom | Likely cause | Fix |
|---|---|---|
| `F9` does nothing at all | `SDL_SCANCODE_F9` block landed inside another `if` or inside a profile scope block | Doc `03` §2.3 — it must be a top-level statement in `onUpdate`, between the `F8` block and the `Physics` block |
| `[PerfCapture] nothing recorded` | the capture stopped in the same frame it started, or `Profiler::GetSnapshot().frameIndex` never advanced | Confirm `--capture-seconds` is > 0 and that `tickPerfCapture(dt)` is called every frame (doc `03` §2.2) |
| Log says `wrote 0 rows` | impossible via `stopAndWrite` — you are looking at a stale binary | Rebuild; check the timestamp on `build/linux-clang/bin/Debug/MegaX` |
| `FAILED to write ...` | the directory could not be created, or the path is not writable | Check the `could not create` line just above it; try `--capture-out /tmp/x.csv` to confirm the code path works |
| CSV appears but is empty apart from the header | `m_samples` was cleared between recording and writing — only possible if `start()` is being called from somewhere new | Confirm `start()` is called only from `tick()`'s warmup branch and `toggle()` |
| Every `*Ms` column is `0.0000` in a **Debug** build | `HBE_PROFILE_ENABLED` got defined to `0`, or a stray `-DNDEBUG` reached the Debug build | `grep -rn "HBE_PROFILE_ENABLED" MegaX/` — only the `option()` block in `CMakeLists.txt` should mention it |
| Only `updateMs` is `0.0000`, others are fine | expected in a Release build with `MEGAX_PROFILE_IN_RELEASE=ON` — `"ApplicationUpdate"` lives in `HBE.Core` | Nothing to fix; use `sceneUpdateMs` instead |
| `renderMs` is `0.0000` but `tileRenderMs` is not | the `"GameRender"` scope name was mistyped, in either `GameLayer.cpp` or the `sectionMs(snap, "GameRender")` call | The two literals must match exactly — they are compared with `strcmp` |
| `renderMs` is *smaller* than `tileRenderMs` | `HBE_PROFILE_SCOPE("GameRender")` was placed after `beginScene` instead of as the first statement | Doc `03` §2.4 |
| `drawCalls` / `submittedQuads` are always `0` | expected — `Renderer2D::drawDirect` resets the batch counters before `endScene` reads them | `00_overview.md` golden rule 7. Not fixable from MegaX; a future `[HBE]` item |
| `gpuMs` is `-1.0000` on every row | the driver has no `ARB_timer_query`, or `HBE_FORCE_NO_GPU_TIMER=1` is still set in the environment | `set -e HBE_FORCE_NO_GPU_TIMER`, then re-run |
| Average frame time is exactly ~16.6 ms and never moves | vsync is on — you are measuring the display | Add `--no-vsync`; confirm the `.meta.txt` says `vsync : off` |
| A few `frameMs` outliers of 50-200 ms | alt-tab, compositor, or the shader/tilemap `FileWatcher` firing | Use p50/p95, not max; keep the window focused and hands off during a capture |
| `rssKB` is `0` on every row | not running on Linux, or `/proc` is not mounted | Expected off Linux — the column is documented as `0 = unknown` |
| Capture starts immediately with no warmup even with `--capture-warmup 3` | the value was consumed as a positional argument — check for a typo like `--capture-warmup3` | Both `--capture-warmup 3` and `--capture-warmup=3` work; `--capture-warmup3` does not |
| `--capture-quit` never exits | `m_perf.shouldQuit()` is checked but `m_app` is null, or the check was omitted | Doc `03` §2.5 — the `if (m_perf.shouldQuit() && m_app)` block |
| Two captures of the same build differ by more than ~10% | something moved: player input, a different map, vsync, background load, or a thermal-throttled CPU | Re-run both back to back with hands off; compare the `.meta.txt` headers line by line |
| `git status` shows changes under `HBE.Core/` | an edit went into the wrong file | Revert it — this is a `[MEGAX]` item |

---

## 8. Done-criterion repro

Item 15, `Docs/WorkItems.txt`: *"This item is complete when
the same test room can produce a CSV that makes regressions
between builds easy to compare."*

You have satisfied it when this sequence works:

1. Build the current tree and capture a baseline:

   ```fish
   cd /home/atulo/Projects/HBE
   cmake --build --preset linux-clang-debug --target MegaX
   ./build/linux-clang/bin/Debug/MegaX --capture-seconds 10 \
       --capture-warmup 3 --no-vsync --capture-label before --capture-quit
   ```

2. Make a change you *know* costs something — for a
   deliberate test, temporarily raise the demo enemy count
   in `GameLayer::spawnDemoEnemies` — rebuild, and capture
   again with `--capture-label after`.

3. Compare the summaries:

   ```fish
   cd ~/.local/share/MegaX/MegaX/captures
   diff (ls -t *before*.meta.txt | head -1) (ls -t *after*.meta.txt | head -1)
   ```

   The `frame ms`, `update ms`, `render ms` and
   `peak counts` lines differ; `scene`, `difficulty`,
   `build config`, `vsync` and `budget` are identical,
   which is what proves the two runs are comparable.

4. Localize it in the CSVs — the per-section columns say
   *which* system grew:

   ```fish
   python3 -c "
   import pandas as pd, sys
   a = pd.read_csv(sys.argv[1]); b = pd.read_csv(sys.argv[2])
   cols = ['frameMs','updateMs','renderMs','gpuMs','aiMs','tileRenderMs','spriteRenderMs']
   print(pd.DataFrame({'before': a[cols].median(), 'after': b[cols].median()}))
   " before.csv after.csv
   ```

5. Undo the deliberate change and confirm a third capture
   lands back on the baseline.

The bar is cleared comfortably: the same room produces a
27-column CSV plus a scored summary, the run's conditions
are recorded so two captures can be shown to be comparable,
and the per-section columns localize a regression rather
than only detecting one. The parts the work item asked for
that MegaX cannot currently supply — non-zero draw-call
counters, and any memory number beyond process RSS — are
named in `00_overview.md` §6 with the reason and the owner.

---

## 9. What comes after this item

Item 16 (*Add a Fixed Gameplay Timestep*) is the first
consumer. Its done criterion is "nearly identical results at
30, 60, 120, and uncapped rendering rates", and a capture
per rate is how you show that: run four times with
`--no-vsync` and different caps, and compare `p50`/`p99`
across the four `.meta.txt` files.

Item 20 (*Build the Golden Visual Benchmark Room*) replaces
`level_01.json` as the test room and adds the fixed spawn
points and repeatable seed that make captures comparable
across content changes, not just code changes. When it
lands, the `scene :` line in the `.meta.txt` starts naming
the golden room, and the budgets in §6 get their first real
re-baseline.

Item 30 (*Stress-Test and Optimize the Golden Room*) is the
item this one exists for. It adds stress presets and says
plainly: "find the first real bottleneck using Items 13-15"
and "optimize only measured problems". Its likely
candidates — tile chunk culling, texture/material state
changes, particle limits — map one-to-one onto columns this
CSV already carries.

That is why the API here is as small as it is: no comparison
tooling, no plotting, no capture-of-a-capture. Items 16, 20
and 30 need one honest row per frame in a file they can
diff, and nothing else.

---

## 10. Cleanup

Nothing to turn off. The capture is inert until you press
`F9` or pass `--capture`, and `MEGAX_PROFILE_IN_RELEASE`
defaults to `OFF`, so a plain build behaves exactly as it
did at the end of item 14.

Two things to leave alone:

* The `"GameRender"` scope stays in `onRender` permanently.
  It is now part of the 1-Hz profiler block and item 30 will
  read it.
* The `captures/` directory under
  `~/.local/share/MegaX/MegaX/` accumulates a file pair per
  run. It is outside the repo, so nothing will be committed
  by accident, but it is worth clearing occasionally:

  ```fish
  ls -lh ~/.local/share/MegaX/MegaX/captures/
  # keep the labelled baselines; delete the unlabelled 'run' ones
  ```

Item 15 is complete.
