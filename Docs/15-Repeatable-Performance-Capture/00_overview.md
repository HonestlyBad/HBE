# 15 — [MEGAX] Create a Repeatable Performance Capture (overview)

Item 13 gave us CPU section timing, item 14 added GPU timer
queries and expanded renderer statistics. Both stop at the
same place: the numbers exist for exactly one frame and are
printed to a log you cannot diff. Item 15 turns that live
snapshot into an **artifact** — a CSV of one row per frame,
written for a configurable number of seconds, plus a small
`.meta.txt` summary that scores the run against a stated
frame budget.

Per the work-item tag `[MEGAX]`, **every edit in this item
lives under `/home/atulo/Projects/HBE/MegaX/`**. No file in
`HBE.Core/`, `HBE.Platform.SDL/`, or `HBE.Renderer.GL/` is
touched — the capture is a pure consumer of
`HBE::Core::Profiler::GetSnapshot()`, which item 14 already
finished. Sandbox and MapMaker are not touched either.

---

## 1. What Item 15 delivers

### New files — the capture itself

* A new header `MegaX/include/Game/PerfCapture.h` and a
  matching `MegaX/src/Game/PerfCapture.cpp`.
* `MegaX::PerfSample` — one CSV row, 27 fields: frame index,
  time offset, frame/update/render/GPU milliseconds, seven
  per-system CPU section times, seven renderer counters,
  five game-side entity counts, light count, and resident
  memory.
* `MegaX::PerfBudget` — a per-frame budget, with two
  factories:
  * `MakeBudget60()` — the shipping target: 16.67 ms frame,
    6.00 ms update, 4.00 ms render, 8.00 ms GPU, ≤64 draw
    calls, ≤4000 submitted quads, ≤1500 particles.
  * `MakeBudget120()` — the aspirational target: 8.33 /
    3.00 / 2.00 / 4.00 ms, ≤48 draws, ≤3000 quads, ≤1000
    particles.
* `MegaX::PerfCaptureRequest` — what the command line asked
  for: `enabled`, `warmupSeconds`, `durationSeconds`,
  `label`, `outPath`, `quitWhenDone`, `useBudget120`,
  `disableVsync`.
* `MegaX::PerfCapture` — the recorder. A four-state machine
  (`Idle → Warmup → Recording → Done`), a pre-reserved
  `std::vector<PerfSample>`, and three outputs when it
  stops: the CSV, the `.meta.txt` sidecar, and a summary
  block in the log.
  * `PerfCapture::FrameCounts` — the counts the profiler
    cannot know: enemies, bullets, enemy bullets,
    particles, difficulty label.
  * `PerfCapture::Summary` — frames, duration, average FPS,
    frame-time avg/min/max/p50/p95/p99, average update /
    render / GPU ms, peak counts, peak RSS, frames over
    budget, and a `pass` verdict.
* `MegaX::ParseCaptureArgs(argc, argv)` and
  `MegaX::PrintCaptureUsage()` — the command-line surface,
  kept out of `main.cpp` so `main` stays eleven lines of
  setup.

### Edits — wiring it into the game

* `MegaX/src/Game/GameLayer.cpp` gains three things: a
  `tickPerfCapture(dt)` call at the very top of `onUpdate`,
  an `F9` toggle in the existing F-key block, and a new
  `HBE_PROFILE_SCOPE("GameRender")` wrapping the whole body
  of `onRender`.
* `MegaX/src/main.cpp` grows `argc`/`argv`, parses the
  capture switches, and hands the request to the layer
  before pushing it.
* `MegaX/CMakeLists.txt` registers `PerfCapture.cpp` and
  adds an opt-in `MEGAX_PROFILE_IN_RELEASE` option.

> **WHY A NEW `"GameRender"` SCOPE?** There is no CPU render
> timing today. `Application::run` deliberately does **not**
> profile its layer `onRender()` loop — item 13 left that to
> games so each game can name its own render scopes. MegaX
> only has `TileRendering` and `SpriteRendering`, which miss
> `drawHud` and, more importantly, miss `r2d.endScene()`,
> where the batch actually flushes. Without a scope around
> the whole function the CSV's `renderMs` column would be a
> lie. This is a MegaX scope in MegaX code — no engine
> change.

---

## 2. Success criteria

Item 15 is complete when **all** of the following hold:

1. `MegaX` builds clean in Debug and in Release, with no new
   warnings.
2. `./build/linux-clang/bin/Debug/MegaX --help` prints the
   capture options and exits 0 without opening a window.
3. Pressing `F9` in-game starts a capture, and either a
   second `F9` or the configured duration stops it, writing
   two files and logging a summary block of this shape:

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
   ```

   (that is a real Debug capture of the current test room
   with `--no-vsync`; with vsync on the same run reads
   ~16.6 ms and ~600 rows)

4. The CSV's first line is exactly the 27-column header in
   doc `05` §1, and every subsequent line has 27 comma-
   separated fields.
5. A headless, unattended run produces a capture and exits
   by itself:

   ```fish
   ./build/linux-clang/bin/Debug/MegaX --capture --capture-seconds 10 \
       --capture-warmup 3 --capture-label baseline --capture-quit
   ```

6. Two captures of the same room, same build, same
   `--no-vsync` setting, agree on `p50` frame time to within
   a few percent — that is what "repeatable" means here.
7. Running the exact same command against a Release build
   produces a CSV whose `frameMs` column is populated even
   though the per-section columns are zero (see golden rule
   6).
8. Nothing under `HBE.Core/`, `HBE.Platform.SDL/`, or
   `HBE.Renderer.GL/` has been modified. `git status` shows
   changes only under `MegaX/` and `Docs/`.

---

## 3. Golden rules (read before touching code)

1. **`[MEGAX]` scope is a hard wall.** Only files under
   `/home/atulo/Projects/HBE/MegaX/`. If something looks
   like it needs an engine tweak to work, it does not get
   one in this item — it gets written down in §6 as a
   future `[HBE]` item. Two such gaps are already known
   (rules 5 and 7); neither blocks the deliverable.
2. **Sample at the top of `onUpdate`, outside every profile
   scope.** `Profiler::EndFrame()` publishes the snapshot at
   the *end* of the frame, so the value you read at the top
   of `onUpdate` is the **previous, fully completed** frame
   — CPU sections, GPU timings, and renderer stats all
   consistent with each other. Reading it anywhere later
   mixes a finished snapshot with half-updated game state.
   Placing the call outside `HBE_PROFILE_SCOPE("SceneUpdate")`
   also keeps the capture's own cost out of the numbers it
   reports.
3. **Zero allocation while recording.** `start()` reserves
   the whole sample vector up front (1000 Hz of headroom, so
   even an uncapped Debug run never reallocates). A
   `std::vector` growth mid-capture is a `memcpy` of the
   entire buffer, and it lands inside the very frame time
   you are measuring. The `.csv` and `.meta.txt` are written
   once, after recording stops.
4. **Compare section names with `std::strcmp`, never pointer
   identity.** `Profiler::Sample::name` is the `const char*`
   the scope was opened with, and `HBE_PROFILE_SCOPE("Physics")`
   in `GameLayer.cpp` and a `"Physics"` literal in
   `PerfCapture.cpp` are two different addresses. The
   profiler itself dedupes sections by pointer — that is
   correct for it, and wrong for us.
5. **Do not call `Profiler::SetEnabled` or
   `Profiler::IsEnabled`.** `Profiler.h` declares them with
   capital letters; `Profiler.cpp` defines `setEnabled` /
   `isEnabled` with lowercase ones. Calling either
   declaration links with
   ``undefined reference to 'HBE::Core::Profiler::SetEnabled(bool)'``.
   The capture never needs them. Fixing the mismatch is an
   engine change and therefore a separate `[HBE]` item.
6. **Release compiles MegaX's scopes out; the frame, GPU and
   renderer numbers survive.** `HBE_PROFILE_SCOPE` expands
   to `((void)0)` when `NDEBUG` is set, so in a stock
   Release build `updateMs`, `renderMs` and all seven
   per-system columns read `0.0000`. `frameMs`, `gpuMs` and
   the renderer counters keep working, because
   `Application::run` calls `Profiler::BeginFrame()` /
   `EndFrame()` **directly**, not through the macro. The
   opt-in `MEGAX_PROFILE_IN_RELEASE` CMake option in doc
   `02` §3 brings MegaX's own scopes back for a Release
   capture.
7. **`drawCalls`, `submittedQuads`, `renderedQuads`,
   `materialChanges` and `textureChanges` read `0` in MegaX
   today, and that is not your bug.** `Renderer2D` harvests
   those counters from `SpriteBatch2D` in `endScene()`, but
   `SpriteBatch2D::begin()` zeroes them, and
   `Renderer2D::drawDirect()` calls `flush()` then `begin()`
   on every single call. `DebugDraw2D::rect` uses
   `drawDirect`, and `GameLayer::drawHud` runs immediately
   before `endScene()` — so the counters are always reset to
   zero just before they are read. Emit the columns anyway:
   they are named in the work item, they cost nothing, and
   they light up the moment the engine harvests those
   counters at flush time instead of at `endScene`. **Do
   not** restructure `onRender` into extra scenes to work
   around it; that is instrumentation dictating rendering.
8. **`--no-vsync` matters more than any other switch.**
   `main.cpp` sets `cfg.vsync = true`, and `swapBuffers()`
   blocks inside `GLRenderer::endFrame`, which is inside the
   measured window. With vsync on every build on every
   machine reports ~16.6 ms and passes the 60 FPS budget
   trivially — the capture measures the display, not the
   game. Every headroom comparison must be run with
   `--no-vsync`. The setting is recorded in the `.meta.txt`
   so a mislabelled run is visible later.
9. **The capture must not touch simulation.** `F9` changes
   no gameplay state, spawns nothing, and pauses nothing.
   Two captures of the same room differ only in
   measurement noise. `--capture-quit` calls the existing
   `Application::requestQuit()`; it does not `exit()`.
10. **No new UI.** The work item says so explicitly, and
    items 13 and 14 said the same. Output is a file plus
    log lines. There is no in-game overlay, no ImGui, no
    graph.
11. **Default output goes to the user-data root, not the
    repo.** `AssetPaths::ResolveUser("captures/...")`
    resolves under `SDL_GetPrefPath` — on CachyOS that is
    `~/.local/share/MegaX/MegaX/captures/`. Captures are run
    artifacts; they must never land in the source tree and
    get committed. `AssetPaths::ResolveUser` does **not**
    create directories, so `PerfCapture` calls
    `std::filesystem::create_directories` itself.

---

## 4. Files touched

| File | Item 14 | Item 15 |
|---|---|---|
| `MegaX/include/Game/PerfCapture.h` | — | **NEW** — `PerfBudget`, `PerfSample`, `PerfCaptureRequest`, `PerfCapture`, `ParseCaptureArgs` |
| `MegaX/src/Game/PerfCapture.cpp` | — | **NEW** — state machine, sampling, CSV + `.meta.txt` writers, summary stats, argv parsing |
| `MegaX/CMakeLists.txt` | source list | add `src/Game/PerfCapture.cpp`; add the `MEGAX_PROFILE_IN_RELEASE` option |
| `MegaX/include/Game/GameLayer.h` | layer members | include `PerfCapture.h`; add `setCaptureRequest()`, `tickPerfCapture()`, member `m_perf` |
| `MegaX/src/Game/GameLayer.cpp` | 7 CPU scopes, `F8` snapshot dump | `m_perf.setSceneLabel` in `onAttach`; `tickPerfCapture(dt)` at the top of `onUpdate`; `F9` toggle; `HBE_PROFILE_SCOPE("GameRender")` in `onRender`; the `tickPerfCapture` definition |
| `MegaX/src/main.cpp` | `int main()` | `int main(int, char**)`; `--help`; `ParseCaptureArgs`; `cfg.vsync` from `--no-vsync`; configure the layer before pushing it |

**Not touched:** every file under `HBE.Core/`,
`HBE.Platform.SDL/` and `HBE.Renderer.GL/`; `HBE.Sandbox/`;
`HBMapMaker/`; every other file under `MegaX/src/` and
`MegaX/include/`; all assets, maps and shaders. No
`.vcxproj` / `.slnx` edits — this project builds through
CMake only.

---

## 5. Design shape

### The state machine

```
                configure(req with enabled=true)
   Idle ─────────────────────────────────────────► Warmup
     │                                                │
     │ toggle() [F9]                                  │ warmupClock >= warmupSeconds
     ▼                                                ▼
  Recording ◄───────────────────────────────────── (start)
     │
     │ captureClock >= durationSeconds, or toggle() [F9], or kMaxSamples
     ▼
   Done ── writes .csv + .meta.txt, logs summary, optionally requestQuit()
     │
     │ toggle() [F9]
     └──────────────────────► Recording (a second, independent capture)
```

`Idle` is the default: with no `--capture*` switch the game
behaves exactly as it did in item 14 until you press `F9`.
The warmup exists because the first seconds of a run are
shader compiles, texture uploads and page faults, and
folding those into a baseline makes every later comparison
noisy.

### Where the sample comes from

```
Application::run, frame N
  Profiler::BeginFrame()            <- frame N's timing window opens
  layer->onUpdate(dt)
    GameLayer::onUpdate
      tickPerfCapture(dt)           <- READS THE SNAPSHOT (frame N-1)
      HBE_PROFILE_SCOPE("SceneUpdate")
      ... Physics / Combat / AI / Particles ...
      Profiler::PublishParticleStats(...)
  layer->onRender()
    HBE_PROFILE_SCOPE("GameRender") <- new in this item
      TileRendering / SpriteRendering / drawHud / endScene
  Profiler::PublishRendererStats(...)
  Profiler::EndFrame()              <- frame N's snapshot is published here
```

So a row written during frame N describes frame N-1. The
`frame` column carries `Snapshot::frameIndex`, so the lag is
visible in the data rather than implied; the first row of a
capture typically starts at a frame index in the hundreds
because the warmup already ran.

### Where the numbers come from

| CSV column group | Source |
|---|---|
| `frameMs` | `Snapshot::frameMs` — `BeginFrame` → `EndFrame` wall time |
| `updateMs` | CPU section `"ApplicationUpdate"` (opened in `Application::run`) |
| `renderMs` | CPU section `"GameRender"` (opened by this item) |
| `gpuMs` | `Snapshot::gpu.frameMs`, or `-1` when `gpu.supported` is false |
| seven `*Ms` section columns | `Snapshot::sections`, matched by `std::strcmp` |
| seven renderer counters | `Snapshot::renderer` |
| `entities` … `particles` | `PerfCapture::FrameCounts`, gathered by `GameLayer` |
| `lights` | `Snapshot::renderer.activeLights` — always `0` until item 24 |
| `rssKB` | `/proc/self/statm` field 2 × page size; `0` off Linux |

### API surface (final)

Everything new lives in `namespace MegaX`:

```cpp
struct PerfBudget {
    const char* name; double frameMs, updateMs, renderMs, gpuMs;
    int drawCalls, submittedQuads, particles;
};
PerfBudget MakeBudget60();
PerfBudget MakeBudget120();

struct PerfSample { /* 27 fields — see doc 01 */ };

struct PerfCaptureRequest {
    bool enabled; double warmupSeconds, durationSeconds;
    std::string label, outPath;
    bool quitWhenDone, useBudget120, disableVsync;
};
PerfCaptureRequest ParseCaptureArgs(int argc, char** argv);
void PrintCaptureUsage();

class PerfCapture {
public:
    struct FrameCounts { int enemies, bullets, enemyBullets, particles;
                         const char* difficulty; };
    struct Summary    { /* 19 roll-up fields — see doc 01 */ };
    static constexpr std::size_t kMaxSamples = 40000;

    void configure(const PerfCaptureRequest& req);
    void setSceneLabel(const std::string& scenePath);
    void tick(float dt, const FrameCounts& counts);
    void toggle();
    bool recording() const;
    bool shouldQuit() const;
    PerfBudget budget() const;
    Summary summarize(const PerfBudget& budget) const;
    const std::string& lastCsvPath() const;
};
```

Deliberately absent: any way to *read back* a CSV, any
comparison of two captures, and any streaming/incremental
write. Comparing runs is a job for a spreadsheet or three
lines of pandas, and buffering in RAM is what keeps the
per-frame cost at one `push_back`.

---

## 6. Non-goals (deferred)

| Behavior | Deferred to |
|---|---|
| A handcrafted, fixed-density test room with repeatable spawns | Item 20 — until then `maps/level_01.json` with one demo enemy is the test room |
| Stress presets that scale enemies / bullets / particles / lights, and acting on the first measured bottleneck | Item 30 — this item produces the measurements it will consume |
| Re-baselining the budgets against production-quality content | Items 29–30; the values here are initial, deliberately generous |
| Frame-time stability that survives a variable timestep | Item 16 (fixed gameplay timestep) |
| Reproducible spawn/AI randomness between captures | Item 17 (deterministic named random streams) |
| Non-zero `drawCalls` / quad / state-change columns | A future `[HBE]` item — `Renderer2D::drawDirect` resets the batch counters via `SpriteBatch2D::begin()` before `endScene()` can read them (golden rule 7). The columns are already in the CSV and will populate with no MegaX change |
| `Profiler::SetEnabled` / `IsEnabled` link errors | A future `[HBE]` item — declaration/definition case mismatch (golden rule 5) |
| `visibleTileChunks` and `postProcessPasses` columns | Not emitted. `TileMapRenderer::resetFrameStats()` is never called by anything, so the counter accumulates for the life of the process, and nothing publishes either field to the profiler. Reporting a growing garbage number is worse than omitting the column |
| GPU memory / allocation counts | Later — the work item says "if HBE exposes it", and HBE exposes neither. `rssKB` from `/proc/self/statm` is the honest game-side substitute |
| An in-game graph, overlay, or profiler window | **Never** — the work item explicitly forbids new engine UI, as items 13 and 14 did |

---

## 7. Controls added in Item 15

| Key | Effect |
|---|---|
| `F9` | Start a performance capture immediately (no warmup), or stop a running one early and write it. Duration comes from `--capture-seconds` (default 10 s). Changes no gameplay state — the player, enemies, difficulty, mode, helmet and camera are untouched. |

All existing controls keep exactly the same behavior: `A`/`D`
move, `SPACE` jump (and fly in Ghost mode), `S` crouch (and
descend in Ghost mode), `E` shoot, `G` Play/Ghost toggle, `H`
helmet toggle, `B` hit/hurt box overlay, `R` refill HP,
`F1`/`F2`/`F3` difficulty, `F5` full scene reload, `F6`
sprite shader hot reload, `F7` soft respawn, `F8` one-shot
profiler dump to the log, `F11` fullscreen (engine level).

`F8` stays a **one-shot log dump** and does **not** become
"start capture" even though the two are related — a capture
is a ten-second commitment and deserves its own key, and
muscle memory built over items 13 and 14 should keep
working. `F5` also keeps its meaning during a capture: it
reloads the scene mid-recording, which is a legitimate way
to measure a reload spike, and the capture keeps running
through it.

---

## 8. Reading order

1. `00_overview.md` — you are here
2. `01_perf_capture_api.md` — the new `PerfCapture.h` header
3. `02_perf_capture_impl.md` — `PerfCapture.cpp` plus its
   `CMakeLists.txt` registration and the Release-profiling
   option
4. `03_gamelayer_wiring.md` — `GameLayer.h` / `GameLayer.cpp`:
   the sample call, `F9`, and the `"GameRender"` scope
5. `04_main_command_line.md` — `main.cpp`: `argc`/`argv`,
   `--help`, `--no-vsync`, handing the request to the layer
6. `05_capture_csv_format.md` — every CSV column, the
   `.meta.txt` sidecar, the budgets, and how to read a
   capture
7. `06_build_run_and_verify.md` — build, run, verify
   checklist, budget knobs, troubleshooting

Do them in that order. Each doc lists **exact** file paths,
exact insertion points, and full copy-paste code blocks. If
you would rather know what the output looks like before you
write the code that emits it, read `05` first — it is a
reference doc and depends on nothing.
