# 05 — The capture file format (`captures/*.csv` + `*.meta.txt`)

This doc defines nothing you have to type. It documents the
two files a capture writes, so you can read one six months
from now without opening `PerfCapture.cpp`.

A capture produces a **pair** of files with the same stem:

```
~/.local/share/MegaX/MegaX/captures/
    megax_perf_debug_baseline_20260815-231939.csv        <- 1 row per frame
    megax_perf_debug_baseline_20260815-231939.meta.txt   <- the run's summary
```

The stem is
`megax_perf_<config>_<label>_<YYYYmmdd-HHMMSS>`, where
`<config>` is `debug` or `release` from `NDEBUG`, and
`<label>` is `--capture-label` folded to
`[A-Za-z0-9_-]` (anything else becomes `-`, and an empty
label becomes `run`). `--capture-out <path>` overrides the
whole thing; the `.meta.txt` is then written next to it with
the same stem.

The CSV is deliberately **pure**: one header row, then data.
No comment lines, no blank lines, no units row. It opens
cleanly in LibreOffice, `pandas.read_csv`, `gnuplot` and
`csvlook` with no arguments. Everything a human needs to
know about the run lives in the `.meta.txt` instead.

---

## 1. Schema — the 27 columns

Column order is the field order of `PerfSample` in
`MegaX/include/Game/PerfCapture.h`. Every millisecond column
is printed `%.4f`; every count is `%d`; `frame` is `%llu`
and `rssKB` is `%ld`.

```jsonc
// one row = one completed frame, in capture order
// (the values are a real row — frame 542 of the capture in §2)
"frame":          542,       // Profiler::Snapshot::frameIndex — monotonic
                             // since process start, NOT since capture start
"t":              0.1040,    // seconds since the first recorded frame; row 1 is 0.0000

"frameMs":        1.8644,    // BeginFrame -> EndFrame wall time. INCLUDES the
                             // vsync wait inside swapBuffers — see rule 3
"updateMs":       0.2064,    // CPU section "ApplicationUpdate" (all layers' onUpdate)
"renderMs":       1.5600,    // CPU section "GameRender" (all of GameLayer::onRender)
"gpuMs":          0.0389,    // GPU frame time; -1.0000 means the driver has no
                             // ARB_timer_query. 3-frame result latency (item 14)

"sceneUpdateMs":  0.1150,    // "SceneUpdate" — GameLayer::onUpdate's own body
"physicsMs":      0.0121,    // "Physics"  — world, player, bullet integration
"combatMs":       0.0019,    // "Combat"   — bullet/enemy hits, impact FX dispatch
"aiMs":           0.0733,    // "AI"       — enemies, enemy bullets, player hits
"particlesMs":    0.0183,    // "Particles"— Effects::update
"tileRenderMs":   0.8717,    // "TileRendering"   — nested inside GameRender
"spriteRenderMs": 0.4853,    // "SpriteRendering" — nested inside GameRender

"drawCalls":      0,         // reads 0 in MegaX today — see rule 4
"passes":         1,         // beginScene/endScene pairs this frame
"submittedQuads": 0,         // reads 0 today — rule 4
"renderedQuads":  0,         // reads 0 today — rule 4
"culledSprites":  0,         // nothing publishes this yet; game-side, item 20+
"materialChanges":0,         // reads 0 today — rule 4
"textureChanges": 0,         // reads 0 today — rule 4

"entities":       3,         // 1 (player) + enemies + bullets + enemyBullets
"enemies":        1,         // EnemyManager::aliveCount()
"bullets":        0,         // BulletManager::count() — vector size, dead included
"enemyBullets":   1,         // EnemyBulletManager::count()
"particles":      8,         // Effects::liveParticles() = particles + shell casings
"lights":         0,         // Snapshot::renderer.activeLights — 0 until item 24

"rssKB":          183148     // /proc/self/statm resident pages x page size, KiB.
                             // 0 on non-Linux. Whole-process, not per-subsystem
```

### Critical reading rules

- **A row describes the frame *before* the one it was
  written in.** The snapshot is published in
  `Profiler::EndFrame()`; the capture samples at the top of
  `onUpdate`. The `frame` column carries the real index so
  the offset is visible rather than assumed. Never join a
  capture against anything by row number — join by `frame`.
- **`frame` does not start at 0 or 1.** The warmup already
  ran, so a 3-second warmup at 300 FPS starts you near frame
  900. Gaps in `frame` are possible in principle (a repeated
  index is skipped, never duplicated) and mean the loop ran
  a frame without publishing.
- **`frameMs` includes the vsync wait.** `swapBuffers()`
  happens inside `GLRenderer::endFrame`, which is inside the
  measured window. With vsync on, every build on every
  machine reports ~16.6 ms and passes the 60 FPS budget
  regardless of how much work it did. **Any capture used to
  compare builds must be run with `--no-vsync`.** The
  `.meta.txt` records which it was.
- **`drawCalls`, `submittedQuads`, `renderedQuads`,
  `materialChanges` and `textureChanges` read `0` in MegaX
  today, and the data is not wrong — the counters are.**
  `Renderer2D` harvests them from `SpriteBatch2D` in
  `endScene()`, but `SpriteBatch2D::begin()` zeroes them and
  `Renderer2D::drawDirect()` calls `flush()` then `begin()`
  on every call. `DebugDraw2D::rect` uses `drawDirect`, and
  `GameLayer::drawHud` runs immediately before `endScene()`,
  so the counters are reset to zero just before they are
  read. The columns stay in the schema because the work item
  names them and because they will populate with no MegaX
  change once the engine harvests at flush time. Until then,
  do not read anything into a `0`.
- **`gpuMs` of `-1.0000` means "no data", not "free".**
  Filter negatives out before averaging. A `0.0000` is a
  real measurement of a frame whose timer query returned
  zero — it happens on the first frames of a run, before the
  3-frame query ring has filled.
- **Per-section columns are all `0.0000` in a stock Release
  build.** `HBE_PROFILE_SCOPE` compiles to `((void)0)` under
  `NDEBUG`. `frameMs`, `gpuMs`, `passes` and every count
  keep working because `Application::run` calls
  `Profiler::BeginFrame/EndFrame` directly. Configure with
  `-DMEGAX_PROFILE_IN_RELEASE=ON` to get MegaX's own scopes
  back; `updateMs` stays `0.0000` even then, because
  `"ApplicationUpdate"` is opened inside `HBE.Core`.
- **`renderMs` is not `tileRenderMs + spriteRenderMs`.**
  `GameRender` also covers `beginScene`, the hitbox overlay,
  `drawHud`, and `endScene` — where the batch actually
  flushes. The difference between them is real work, and
  usually most of the frame.
- **`updateMs` is not `sceneUpdateMs`.**
  `"ApplicationUpdate"` wraps the whole layer loop in
  `Application::run`; `"SceneUpdate"` is `GameLayer`'s own
  body inside it. With one layer they are close, and the gap
  is the loop overhead plus this capture's own sampling.
- **`bullets` counts vector size, not live bullets.**
  `BulletManager::count()` returns `m_bullets.size()`, which
  includes entries flagged dead this frame but not yet
  culled. `enemies` is the opposite — `aliveCount()` walks
  the vector and skips the dead.
- **`rssKB` is the whole process.** It includes the GL
  driver, SDL, textures and every allocation MegaX has ever
  made and not returned to the OS. It is useful as a *trend*
  across a capture (a leak shows as a rising line) and
  nearly useless as an absolute number.

---

## 2. A real capture

The first six lines of a 2-second Debug capture of
`maps/level_01.json`, one demo enemy, no player input, run
with `--no-vsync`:

```csv
frame,t,frameMs,updateMs,renderMs,gpuMs,sceneUpdateMs,physicsMs,combatMs,aiMs,particlesMs,tileRenderMs,spriteRenderMs,drawCalls,passes,submittedQuads,renderedQuads,culledSprites,materialChanges,textureChanges,entities,enemies,bullets,enemyBullets,particles,lights,rssKB
501,0.0000,0.8160,0.0380,0.7624,0.0420,0.0084,0.0024,0.0003,0.0032,0.0005,0.4460,0.2347,0,1,0,0,0,0,0,2,1,0,0,0,0,183144
502,0.0019,1.6475,0.0682,1.5274,0.0379,0.0087,0.0022,0.0004,0.0035,0.0005,0.8236,0.5345,0,1,0,0,0,0,0,2,1,0,0,0,0,183144
503,0.0047,1.8777,0.1895,1.5699,0.0420,0.1012,0.0131,0.0019,0.0146,0.0027,0.8952,0.5007,0,1,0,0,0,0,0,2,1,0,0,0,0,183144
504,0.0078,1.8119,0.1190,1.5494,0.0000,0.0395,0.0131,0.0018,0.0142,0.0026,0.8804,0.4937,0,1,0,0,0,0,0,2,1,0,0,0,0,183144
505,0.0108,1.8113,0.1181,1.5743,0.0420,0.0391,0.0127,0.0018,0.0145,0.0025,0.9041,0.4928,0,1,0,0,0,0,0,2,1,0,0,0,0,183144
```

and three lines from later in the same run, after the enemy
has fired:

```csv
542,0.1040,1.8644,0.2064,1.5600,0.0389,0.1150,0.0121,0.0019,0.0733,0.0183,0.8717,0.4853,0,1,0,0,0,0,0,3,1,0,1,8,0,183148
543,0.1070,1.8169,0.1239,1.5865,0.0420,0.0397,0.0107,0.0016,0.0132,0.0062,0.8943,0.4902,0,1,0,0,0,0,0,3,1,0,1,8,0,183152
544,0.1101,0.9913,0.0344,0.8126,0.0000,0.0151,0.0024,0.0004,0.0044,0.0053,0.4445,0.2717,0,1,0,0,0,0,0,3,1,0,1,8,0,183152
```

Its `.meta.txt`, in full:

```
MegaX performance capture
=========================

started by      : command line
build config    : debug
megax scopes    : on
vsync           : off
scene           : maps/level_01.json
difficulty      : Difficult
label           : smoke2
timestamp       : 20260815-233314
frames          : 992 over 2.00 s (avg 1086.4 FPS)

budget          : 60fps (frame 16.67 ms, update 6.00, render 4.00, gpu 8.00)
                  drawCalls <= 64, submittedQuads <= 4000, particles <= 1500

frame ms        : avg 0.920  min 0.156  max 2.010
                  p50 0.819  p95 1.812  p99 1.939
update ms       : avg 0.049  (budget 6.00)
render ms       : avg 0.825  (budget 4.00)
gpu ms          : avg 0.035  (budget 8.00)
peak counts     : drawCalls 0  submittedQuads 0  particles 8  entities 3
peak rss        : 183244 KiB
over budget     : 0 frames (0.0 %)

verdict         : PASS
```

---

## 3. What this capture says

Reading the numbers above in plain language:

* **The game is nowhere near the 60 FPS budget, in Debug.**
  p95 is 1.81 ms against a 16.67 ms budget — about 9× of
  headroom. That is what an empty room with one enemy should
  look like, and it is why item 20 exists: there is nothing
  to measure yet.
* **Rendering dominates.** `renderMs` averages 0.825 of a
  0.920 ms frame; `updateMs` is 0.049. Inside rendering,
  `tileRenderMs` (0.87–0.90 in the sampled rows) is roughly
  twice `spriteRenderMs` (0.49) — the tilemap draws every
  visible layer every frame with no chunk culling. That is
  the first thing item 30 will look at.
* **`renderMs` is bigger than `tileRenderMs +
  spriteRenderMs`** (1.5600 vs 1.3570 in row `542`). The
  0.20 ms gap is `drawHud`, the `beginScene`/`endScene`
  bookkeeping, and the batch flush.
* **The GPU is idle.** 0.035 ms average against an 8 ms
  budget. A 2D scene of a few hundred quads is CPU-bound by
  a wide margin, which is the expected shape.
* **`entities` steps from 2 to 3 and `particles` from 0 to
  8** around `t = 0.10` — the demo enemy fired, adding one
  enemy bullet and a muzzle-flash burst. `aiMs` jumps with
  them (0.0032 → 0.0733 in row `542`). Counts moving with
  visible events is the cheapest sanity check that the
  capture is recording the frame it claims to.
* **`rssKB` climbs 183144 → 183244 over two seconds** — 100
  KiB, which is the sample vector growing and the odd
  allocation. Flat-ish is what you want; a line that climbs
  steadily for thirty seconds is a leak.
* **`drawCalls` and the quad columns are 0** for the reason
  in rule 4. `passes` is 1, which confirms the render path
  did run — one `beginScene`/`endScene` pair per frame.

---

## 4. Using captures to catch regressions

The whole point of the pair of files is diffing two runs.
The procedure that makes them comparable:

1. Same room. Today that is `maps/level_01.json` with
   `spawnDemoEnemies()`'s single enemy. Item 20 replaces it
   with the golden room.
2. Same build config, and the same
   `MEGAX_PROFILE_IN_RELEASE` setting.
3. `--no-vsync`, always.
4. Same warmup and duration. The defaults (3 s / 10 s) are
   fine; just do not change them between the two runs.
5. Do not touch the keyboard during the capture. Player
   input changes the workload, and "I moved more in run B"
   is not a regression.
6. Label the runs so the filenames say what they are:
   `--capture-label before-lighting`,
   `--capture-label after-lighting`.

Then compare. The `.meta.txt` files answer most questions on
their own:

```fish
cd ~/.local/share/MegaX/MegaX/captures
diff (ls -t *before*.meta.txt | head -1) (ls -t *after*.meta.txt | head -1)
```

For anything the summary does not cover, the CSV is three
lines of pandas:

```fish
python3 -c "
import pandas as pd, sys
a = pd.read_csv(sys.argv[1]); b = pd.read_csv(sys.argv[2])
cols = ['frameMs','renderMs','updateMs','gpuMs','tileRenderMs','spriteRenderMs']
print(pd.DataFrame({'before': a[cols].median(), 'after': b[cols].median()}))
" before.csv after.csv
```

Medians, not means — one alt-tab stall skews a mean and
leaves a median alone.

To find *where* a regression is rather than *whether* there
is one, the per-section columns are the answer: if
`frameMs` grew by 2 ms and `tileRenderMs` grew by 1.9 ms,
you are done looking.

### Adding a column later

If a future item needs another number in the CSV:

1. Add the field to `PerfSample` in `PerfCapture.h`, at the
   end of its group.
2. Add its name to `kCsvHeader` in the same position.
3. Add its format specifier and argument to the
   `std::fprintf` in `PerfCapture::writeCsv`, in the same
   position.
4. Fill it in `PerfCapture::record`.

All four are in two files, and doc `06` §2's comma count
catches three of the four ways to get it wrong. Old CSVs
stay readable — they simply have fewer columns, and every
tool that reads a header row copes.

The columns already earmarked for later items:
`culledSprites` (a game-side sprite cull, item 20+),
`lights` (item 24 onward), and the five renderer counters
that light up when the batch stats are harvested correctly.

---

## 5. Sanity check

After running a capture:

```fish
cd ~/.local/share/MegaX/MegaX/captures

# The newest capture, whatever it is called
set -l csv (ls -t *.csv | head -1)

# Header has 27 columns?
head -1 $csv | tr ',' '\n' | wc -l
# -> 27

# Every data row has 27 fields too?
awk -F, 'NR>1 && NF!=27 {bad++} END {print bad+0}' $csv
# -> 0

# How many frames, and does the count match the .meta.txt?
math (wc -l < $csv) - 1
grep "^frames" (string replace '.csv' '.meta.txt' $csv)

# Valid CSV as far as python is concerned?
python3 -c "import csv,sys; r=list(csv.DictReader(open(sys.argv[1]))); print(len(r), 'rows,', len(r[0]), 'cols')" $csv
# -> e.g. "999 rows, 27 cols"
```

A capture with 0 rows means the recording stopped before a
frame was published — check the log for
`[PerfCapture] nothing recorded`.

Next: `06_build_run_and_verify.md`.
