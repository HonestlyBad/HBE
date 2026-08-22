# 01 — The capture API (`Game/PerfCapture.h`)

This doc creates one new file: the public header for the
performance capture. Nothing is implemented yet — doc `02`
writes the `.cpp`. The project will **not** link after this
doc, and that is expected.

The header is included by `GameLayer.h` (doc `03`) and by
`main.cpp` (doc `04`), so keep its include list small: four
standard headers and nothing from HBE. In particular it must
**not** include `HBE/Core/Profiler.h` — the profiler types
appear only inside `PerfCapture.cpp`, which keeps the
snapshot's `std::vector<Sample>` out of every translation
unit that merely wants to start a capture.

---

## Key engine APIs used here

None yet — this header is pure data. For orientation, these
are the engine symbols doc `02` will compose, each verified
against its real header rather than the API reference:

- `HBE::Core::Profiler::GetSnapshot()` →
  `const Snapshot&` (`HBE.Core/include/HBE/Core/Profiler.h:83`).
  Rebuilt inside `EndFrame()`, so it always describes the
  **previous** completed frame when read from `onUpdate`.
- `Snapshot` carries `frameMs`, `frameAvgMs/Min/Max`,
  `frameSampleCount`, `frameIndex`, `std::vector<Sample>
  sections`, `GpuTimings gpu`, `RendererStats renderer`.
- `Sample` is `{ const char* name; int depth; double
  currentMs, avgMs, minMs, maxMs; std::size_t sampleCount,
  opensThisFrame; }`. **`name` is a raw pointer to the
  literal the scope was opened with** — match it with
  `std::strcmp`.
- `HBE::Core::AssetPaths::ResolveUser(std::string_view)` →
  `std::string` (`AssetPaths.h:32`). Joins a relative path
  onto the `SDL_GetPrefPath` user-data root. It does **not**
  create directories.
- `HBE::Core::LogInfo / LogWarn / LogError(std::string_view)`
  (`Log.h:20-22`).

---

## 1. Create the file

**Full path:** `/home/atulo/Projects/HBE/MegaX/include/Game/PerfCapture.h`

**Status:** the file must not exist yet. If it does, stop and
check — someone likely started this work item already.

Paste the following content **verbatim** (no reformatting).
Indentation is **4 spaces**, matching the newer MegaX headers
(`EnemyManager.h`, `EnemyBullet.h`) rather than the
tab-indented older ones:

```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace MegaX {

    // -----------------------------------------------------------------
    // Item 15 — repeatable performance capture (developer tool).
    //
    // Records the HBE profiler snapshot plus a few game-side counts into
    // an in-memory buffer for N seconds, then writes one CSV and one
    // human-readable ".meta.txt" summary next to it.
    //
    // This is a pure *consumer* of HBE::Core::Profiler::GetSnapshot().
    // Nothing in HBE.* changes for this item.
    // -----------------------------------------------------------------

    // A per-frame budget. Two are predefined: the 60 FPS shipping target
    // and the 120 FPS aspirational target.
    struct PerfBudget {
        const char* name      = "60fps";
        double frameMs        = 16.67;  // whole CPU frame (Application::run)
        double updateMs       = 6.00;   // "ApplicationUpdate" CPU section
        double renderMs       = 4.00;   // "GameRender" CPU section
        double gpuMs          = 8.00;   // GPU frame; ignored when unsupported
        int    drawCalls      = 64;
        int    submittedQuads = 4000;
        int    particles      = 1500;
    };

    PerfBudget MakeBudget60();
    PerfBudget MakeBudget120();

    // One CSV row == one completed frame. The field order here IS the
    // column order in the CSV — see 05_capture_csv_format.md.
    struct PerfSample {
        std::uint64_t frame = 0;      // Profiler::Snapshot::frameIndex
        double t            = 0.0;    // seconds since the capture started

        double frameMs      = 0.0;
        double updateMs     = 0.0;
        double renderMs     = 0.0;
        double gpuMs        = -1.0;   // -1 => GPU timing unsupported

        double sceneUpdateMs  = 0.0;
        double physicsMs      = 0.0;
        double combatMs       = 0.0;
        double aiMs           = 0.0;
        double particlesMs    = 0.0;
        double tileRenderMs   = 0.0;
        double spriteRenderMs = 0.0;

        int drawCalls       = 0;
        int passes          = 0;
        int submittedQuads  = 0;
        int renderedQuads   = 0;
        int culledSprites   = 0;
        int materialChanges = 0;
        int textureChanges  = 0;

        int entities     = 0;         // player + enemies + both bullet pools
        int enemies      = 0;
        int bullets      = 0;
        int enemyBullets = 0;
        int particles    = 0;
        int lights       = 0;

        long rssKB = 0;               // resident set size in KiB, 0 = unknown
    };

    // What the command line asked for. All-defaults means "no capture".
    struct PerfCaptureRequest {
        bool        enabled         = false;
        double      warmupSeconds   = 3.0;
        double      durationSeconds = 10.0;
        std::string label;            // free text, lands in the filename
        std::string outPath;          // explicit .csv path; empty = auto
        bool        quitWhenDone     = false;
        bool        useBudget120     = false;
        bool        disableVsync     = false;   // --no-vsync, applied in main
    };

    // Parses the --capture* switches out of argv. Never throws: a bad
    // value warns and keeps the default.
    PerfCaptureRequest ParseCaptureArgs(int argc, char** argv);
    void PrintCaptureUsage();

    class PerfCapture {
    public:
        // The counts the profiler cannot know about, gathered by
        // GameLayer once per frame.
        struct FrameCounts {
            int enemies      = 0;
            int bullets      = 0;
            int enemyBullets = 0;
            int particles    = 0;
            const char* difficulty = "?";   // string literal, not owned
        };

        // Roll-up written to the .meta.txt sidecar and the log.
        struct Summary {
            std::size_t frames      = 0;
            double durationSec      = 0.0;
            double avgFps           = 0.0;
            double frameAvgMs       = 0.0;
            double frameMinMs       = 0.0;
            double frameMaxMs       = 0.0;
            double frameP50Ms       = 0.0;
            double frameP95Ms       = 0.0;
            double frameP99Ms       = 0.0;
            double updateAvgMs      = 0.0;
            double renderAvgMs      = 0.0;
            double gpuAvgMs         = -1.0;
            int    maxDrawCalls     = 0;
            int    maxSubmittedQuads = 0;
            int    maxParticles     = 0;
            int    maxEntities      = 0;
            long   maxRssKB         = 0;
            std::size_t framesOverBudget = 0;
            double overBudgetPct    = 0.0;
            bool   pass             = false;
        };

        // ~5.5 minutes at 120 FPS. A hard stop so an unattended capture
        // cannot grow without bound.
        static constexpr std::size_t kMaxSamples = 40000;

        void configure(const PerfCaptureRequest& req);
        void setSceneLabel(const std::string& scenePath);

        // Call once per frame, at the very top of GameLayer::onUpdate,
        // outside every HBE_PROFILE_SCOPE.
        void tick(float dt, const FrameCounts& counts);

        // F9. Starts a capture immediately (no warmup), or stops the
        // running one early and writes what it has.
        void toggle();

        bool recording() const { return m_state == State::Recording; }
        bool shouldQuit() const { return m_quitRequested; }

        PerfBudget budget() const {
            return m_req.useBudget120 ? MakeBudget120() : MakeBudget60();
        }

        Summary summarize(const PerfBudget& budget) const;

        const std::string& lastCsvPath() const { return m_lastCsvPath; }

    private:
        enum class State { Idle, Warmup, Recording, Done };

        void start(const char* reason);
        void record(const FrameCounts& counts);
        void stopAndWrite();
        bool writeCsv(const std::string& path) const;
        bool writeMeta(const std::string& path, const Summary& sum) const;
        void logSummary(const Summary& sum) const;
        std::string buildOutputPath() const;

        PerfCaptureRequest m_req{};
        State  m_state        = State::Idle;
        double m_warmupClock  = 0.0;
        double m_captureClock = 0.0;

        std::vector<PerfSample> m_samples;
        std::string m_sceneLabel        = "unknown";
        std::string m_difficultyAtStart = "?";
        std::string m_startedBy         = "?";
        std::string m_lastCsvPath;

        bool m_quitRequested = false;
        std::uint64_t m_lastFrameIndex = 0;
    };
}
```

> **PASTE NOTE.** The column-aligned `=` signs in the struct
> bodies are cosmetic, but keeping them makes the CSV column
> order readable at a glance and matches the alignment style
> already used in `Player.h`'s tunables block. If your editor
> strips trailing whitespace, that is fine — there is none
> here that matters. There are no macro line-continuations
> in this file.

---

## 2. What each piece is for

### `PerfBudget` — the numbers we are holding ourselves to

The work item asks for "initial performance budgets for
60 FPS and an aspirational 120 FPS mode". These are those
budgets, expressed as a struct so a capture can be scored
automatically instead of eyeballed.

The frame numbers are just the period: `1000 / 60 = 16.67`
and `1000 / 120 = 8.33`. The sub-budgets carve that up with
deliberate slack — `updateMs + renderMs + gpuMs` is more
than `frameMs` on purpose, because CPU update, CPU render
and GPU work overlap in time and a per-subsystem ceiling is
a ceiling, not a share of a fixed pie.

The count budgets (`drawCalls`, `submittedQuads`,
`particles`) are the shape of a 2D scene that stays cheap
on any GL 3.3 desktop part. They are **initial** values, as
the work item asks — item 20 builds the real test room and
items 29–30 re-baseline them against production content.
Doc `06` §5 tells you which one to move when a capture
fails.

### `PerfSample` — one row, 27 columns

Field order in this struct is the column order in the CSV.
If you ever add a field, add it at the end of its group and
update **both** `kCsvHeader` and the `std::fprintf` format
string in doc `02` — they are hand-kept in sync, and doc
`06` §2 has a `grep` that checks the header has the right
number of commas.

`gpuMs` defaults to `-1.0` rather than `0.0` so a driver
without `ARB_timer_query` is distinguishable from a frame
that genuinely cost the GPU nothing. Every consumer should
treat a negative value as "no data".

### `PerfCaptureRequest` — the command line, as data

Parsed once in `main.cpp` and handed to the layer before it
is pushed. Keeping it a plain struct means `F9` and the
command line share one code path: `configure()` stores it,
and a manual `F9` capture uses the same `durationSeconds`,
`label`, `outPath` and budget that a `--capture` run would.

`disableVsync` is the odd one out — it is consumed by
`main.cpp` before the window exists, not by `PerfCapture`.
It rides along in the request so the recorder can print it
into the `.meta.txt`, which is the only way to tell two
otherwise identical captures apart later.

### `PerfCapture::FrameCounts` — what the profiler cannot see

`Profiler::Snapshot` knows nothing about enemies or
bullets, and MegaX does not put them in the ECS registry —
they live in `EnemyManager`, `BulletManager` and
`EnemyBulletManager`. `GameLayer` fills this struct each
frame from accessors that already exist:
`EnemyManager::aliveCount()`, `BulletManager::count()`,
`EnemyBulletManager::count()`, `Effects::liveParticles()`.

`difficulty` is a `const char*` pointing at
`DifficultyProfile::label`, which is a string literal —
copying it every frame would allocate, and the recorder only
reads it once, on the first recorded frame.

### `PerfCapture::Summary` — the part you actually read

The CSV is for diffing two builds; the summary is for
answering "did this run pass?" in one line. Percentiles
matter more than the average here: a 16.6 ms average with a
p99 of 45 ms is a stuttery game, and the average hides it.

---

## 3. What NOT to touch

* Do **not** add `#include "HBE/Core/Profiler.h"` to this
  header. It only needs to be visible inside
  `PerfCapture.cpp`; pulling it in here drags the profiler's
  `<vector>` of samples into `GameLayer.h` and `main.cpp`
  for no benefit.
* Do **not** give `PerfCapture` a constructor or destructor.
  The default ones are correct, and `GameLayer` holds it by
  value as `PerfCapture m_perf{}`.
* Do **not** make `tick()` take the `Profiler::Snapshot` as
  a parameter. Fetching it inside `record()` keeps
  `GameLayer` from having to know when the snapshot is
  valid — which is precisely the thing that is easy to get
  wrong.
* Do **not** add a `PerfCapture` member to `Player`,
  `Enemy`, or any other gameplay type. One instance, owned
  by `GameLayer`, is the whole design.

---

## 4. Sanity check before doc 02

```fish
test -f /home/atulo/Projects/HBE/MegaX/include/Game/PerfCapture.h; echo $status
# -> 0

grep -c "^\s*struct\|^\s*class" /home/atulo/Projects/HBE/MegaX/include/Game/PerfCapture.h
# -> 6  (PerfBudget, PerfSample, PerfCaptureRequest, PerfCapture,
#         FrameCounts, Summary)

grep -c "HBE/" /home/atulo/Projects/HBE/MegaX/include/Game/PerfCapture.h
# -> 0  (this header includes nothing from the engine)
```

Do **not** attempt to build yet. `PerfCapture.h` declares
`MakeBudget60`, `MakeBudget120`, `ParseCaptureArgs`,
`PrintCaptureUsage`, `configure`, `setSceneLabel`, `tick`,
`toggle`, `summarize` and six private members, none of which
are defined until doc `02`. Nothing includes this header
yet either, so a build right now would simply not notice it.

Next: `02_perf_capture_impl.md`.
