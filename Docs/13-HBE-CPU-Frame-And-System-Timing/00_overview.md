# 13 — [HBE] CPU Frame and System Timing (overview)

Item 12 finished the "combat feel" arc (walk / land / muzzle
/ hit-spark / blood-splatter / explosion FX). Now it is time
to look under the hood: how long is each of those systems
actually taking on the CPU every frame? Item 13 adds a
lightweight profiling API **to HBE** so any game (MegaX,
Sandbox, or MapMaker) can time named CPU sections with a
RAII scope and read the results back after every frame.

Per the work-item tag `[HBE]`, **all API and implementation
edits live under `G:\Dev\HBE\HBE.Core\`**. The only MegaX
edits are the *verification* pass in doc `04_...`, where
MegaX consumes the API by wrapping five or more nested
sections with the new macro. Sandbox and MapMaker are **not**
touched.

---

## 1. What Item 13 delivers

* A new HBE header `HBE/Core/Profiler.h` and matching
  `Profiler.cpp` under `HBE.Core/`.
* A public data structure `HBE::Core::Profiler::Snapshot`
  that lists every named section for the current frame with:
  * `currentMs` — this frame's duration in milliseconds
  * `avgMs`     — rolling average over the last N frames
  * `minMs`     — smallest sample in the rolling window
  * `maxMs`     — largest sample in the rolling window
  * `sampleCount` — number of samples averaged
* A RAII scope type `HBE::Core::Profiler::ScopeTimer` that
  starts on construction and closes on destruction. Named
  sections may be **nested** (e.g. `SceneUpdate` wraps
  `Physics` which wraps `Combat`).
* Two macros:
  * `HBE_PROFILE_SCOPE("Name")` — RAII scoped timer
  * `HBE_PROFILE_FRAME()`       — call once per frame, closes
    all open frames and refreshes the snapshot
* Compile-time toggle `HBE_PROFILE_ENABLED` (defaults to `1`
  in Debug, `0` in Release). When `0` the macros expand to
  nothing — literally zero overhead.
* Runtime toggle `HBE::Core::Profiler::SetEnabled(bool)` so a
  game can disable it without a rebuild.
* Integration into `HBE::Core::Application::run()`:
  * `Profiler::BeginFrame()` at the top of every loop
    iteration
  * `HBE_PROFILE_SCOPE("ApplicationUpdate")` around the
    layer `onUpdate` loop
  * `HBE_PROFILE_SCOPE("Audio")` around `m_audio.update(dt)`
  * `Profiler::EndFrame()` right before `m_platform.delayMillis(1)`
    (updates rolling stats + publishes snapshot).
* Verification-only edits in MegaX `GameLayer.cpp`
  (doc `04`) that wrap five sections:
  `SceneUpdate`, `Physics`, `AI`, `Combat`, `Particles`,
  `TileRendering`, `SpriteRendering`. That satisfies the
  "at least five nested systems" done criterion.

> **NOTE ON `[HBE]` SCOPE.** The engine changes above are the
> full deliverable. The MegaX edits in doc `04` are strictly
> a **consumer** of the new HBE API — they don't add any
> game-specific logic to HBE and they don't change gameplay.
> If you strip them out, HBE still builds and still ships the
> profiler; MegaX just wouldn't produce visible timing
> output. Sandbox and MapMaker are untouched.

---

## 2. Success criteria

Item 13 is complete when **all** of the following hold:

1. `HBE.Core` builds clean with `HBE_PROFILE_ENABLED=1` in
   the Debug config.
2. `HBE.Core` builds clean with `HBE_PROFILE_ENABLED=0` in
   the Release config (macro expansions produce no symbols
   in disassembly).
3. Running MegaX (Debug) prints a per-second line to the
   console with **at least 5** nested section names and
   millisecond values. Example format:
   ```
   [Profiler] Frame=16.42ms (avg 16.51 min 15.98 max 18.20)
     ApplicationUpdate      cur=8.31  avg=8.42  min=7.90  max=9.55
       SceneUpdate          cur=8.10  avg=8.20  min=7.72  max=9.30
         Physics            cur=1.20  avg=1.18  min=1.05  max=1.42
         AI                 cur=2.40  avg=2.35  min=2.10  max=2.71
         Combat             cur=0.55  avg=0.51  min=0.35  max=0.83
         Particles          cur=1.10  avg=1.03  min=0.88  max=1.42
     TileRendering          cur=2.20  avg=2.15  min=1.98  max=2.60
     SpriteRendering        cur=4.00  avg=3.98  min=3.60  max=4.42
     Audio                  cur=0.09  avg=0.08  min=0.05  max=0.15
   ```
4. The `Profiler::Snapshot` structure is populated **after
   every frame** and is publicly readable via
   `Profiler::GetSnapshot()`; no engine window / UI is added.
5. Nested scopes report their own timings, and the timing
   for a parent scope is `>=` the sum of its immediate
   children (roughly — measurement noise permitting).
6. Turning off the runtime flag (`Profiler::SetEnabled(false)`)
   causes new samples to be skipped without crashing and the
   snapshot to freeze at its last valid state.

---

## 3. Golden rules (read before touching code)

1. **`[HBE]` scope.** Only touch files under
   `G:\Dev\HBE\HBE.Core\`. The **only** exception is the
   MegaX verification pass in doc `04`, which is required to
   fulfill the "MegaX can time at least five nested systems"
   done criterion. Do **not** open any file under
   `HBE.Sandbox\` or `HBMapMaker\`.
2. **RAII, not manual start/stop.** All timing goes through
   `HBE_PROFILE_SCOPE("Name")`. Do **not** expose a
   `beginNamed / endNamed` pair to game code — early returns
   would leak scopes.
3. **Nesting is by *scope life-cycle*, not by string
   parenthood.** The profiler infers hierarchy from the
   order/lifetime of RAII scopes on a per-frame stack. Never
   pass a "parent" string; the stack does that automatically.
4. **Zero allocations per scope in the hot path.** The scope
   type takes `const char*` (compile-time string) and pushes
   to a preallocated fixed-capacity ring. No `std::string`,
   no `std::vector` growth inside the frame. The rolling
   window uses a fixed-size ring (default 120 frames).
5. **Thread affinity: main thread only.** The API is
   documented as main-thread only. Do not sprinkle
   `HBE_PROFILE_SCOPE` inside worker threads or audio
   callbacks — behavior is undefined.
6. **`Profiler::BeginFrame()` must run before any scope
   opens in that frame.** Similarly `EndFrame()` must run
   after the last scope closes. `Application::run` guarantees
   both.
7. **Snapshot is a value copy.** `GetSnapshot()` returns by
   const-ref to internal storage. Callers that want a
   longer-lived copy must copy it. **Do not** cache the ref
   past the next `EndFrame()` — internal storage is reused.
8. **Compile-time off must be truly zero-cost.** When
   `HBE_PROFILE_ENABLED` is `0`, `HBE_PROFILE_SCOPE("X")`
   must expand to `((void)0)`, not to an empty inline
   function call. Verify by grepping the release preprocessor
   output if you're paranoid.
9. **No engine UI.** The work item explicitly forbids adding
   a profiler window. Output goes to `LogInfo` (once per
   second) in `Application::run` **only when a debug
   `#define` is on** (see doc `02` §4). Games can also read
   the snapshot silently and dump their own CSV — that is
   Item 15's scope, not this item's.
10. **Do not break Release builds.** The `Profiler.h` header
    is included from `Application.h` — it must be safe to
    include when `HBE_PROFILE_ENABLED=0`.

---

## 4. Files touched

| File | Item 12 | Item 13 |
|---|---|---|
| `HBE.Core/include/HBE/Core/Profiler.h` | — | **NEW** — public API, Snapshot struct, RAII scope, macros |
| `HBE.Core/src/Core/Profiler.cpp` | — | **NEW** — implementation (rolling ring, stack, snapshot build) |
| `HBE.Core/include/HBE/Core/Application.h` | — | `#include "HBE/Core/Profiler.h"` |
| `HBE.Core/src/Core/Application.cpp` | — | `BeginFrame/EndFrame` bracketed around the loop body, `HBE_PROFILE_SCOPE("ApplicationUpdate")` around the layer `onUpdate` loop, `HBE_PROFILE_SCOPE("Audio")` around `m_audio.update(dt)`, optional 1-Hz LogInfo dump |
| `HBE.Core/HBE.Core.vcxproj` | — | new `<ClInclude>` for `Profiler.h`, new `<ClCompile>` for `Profiler.cpp` |
| `HBE.Core/HBE.Core.vcxproj.filters` | — | matching entries so the files show up in Solution Explorer |
| `MegaX/src/Game/GameLayer.cpp` | shooting/FX wiring | **verification only**: 7 `HBE_PROFILE_SCOPE(...)` calls added to `onUpdate` and `onRender` to satisfy "at least five nested systems" done criterion |
| `MegaX/include/Game/GameLayer.h` | — | `#include "HBE/Core/Profiler.h"` if not already visible through `Application.h` |

Sandbox, MapMaker, and every other engine subsystem are
**not** touched. Renderer files remain untouched — GPU
timing is a separate work item (Item 14).

---

## 5. Design shape

### The scope stack

Every scope tracks:

```cpp
struct FrameSample {
    const char* name;        // pointer identity, no strcmp
    std::uint64_t startNs;   // Profiler::NowNs() when the scope opened
    std::uint64_t endNs;     // filled on ~ScopeTimer
    int depth;               // pushed by BeginScope, popped by EndScope
};
```

`BeginScope(name)` pushes onto a thread-local stack, records
`startNs`, and returns an index. `EndScope(index)` sets
`endNs` and decrements the stack depth. `EndFrame()` walks
the closed samples, merges them into the rolling ring by
name identity, and rebuilds the snapshot.

### The rolling ring

For each unique `const char*` we keep:

```cpp
struct SectionStats {
    const char* name;
    std::size_t writeCursor;
    std::array<double, 120> samplesMs;
    std::size_t count;   // <= 120
    double currentMs;    // last inserted sample
    double avgMs;
    double minMs;
    double maxMs;
};
```

The ring width is `kRollingWindowFrames` (default `120`,
equivalent to ~2 seconds at 60 FPS). Doc `02` §3 documents
how to change it.

### Snapshot publication

At `EndFrame()`:

1. For every closed section this frame, insert `durationMs`
   into its `samplesMs` ring.
2. Recompute `avg / min / max` over `count` samples.
3. Copy the section list (in insertion / first-seen order,
   with depth) into `Snapshot::sections`.
4. Update `Snapshot::frameMs = sum of top-level sections`
   (depth == 0 total) plus a "wall" reading from the frame
   `BeginFrame / EndFrame` timer.

### API surface (final)

Everything exposed to games lives in
`namespace HBE::Core::Profiler`:

```cpp
void BeginFrame();
void EndFrame();
void SetEnabled(bool on);
bool IsEnabled();

struct Sample {
    const char* name;
    int depth;
    double currentMs;
    double avgMs;
    double minMs;
    double maxMs;
    std::size_t sampleCount;
};

struct Snapshot {
    double frameMs;
    double frameAvgMs;
    double frameMinMs;
    double frameMaxMs;
    std::vector<Sample> sections;
    std::uint64_t frameIndex;
};

const Snapshot& GetSnapshot();

class ScopeTimer {
public:
    explicit ScopeTimer(const char* name);
    ~ScopeTimer();
    ScopeTimer(const ScopeTimer&) = delete;
    ScopeTimer& operator=(const ScopeTimer&) = delete;
private:
    int m_index; // -1 when disabled
};

#if HBE_PROFILE_ENABLED
    #define HBE_PROFILE_SCOPE(NAME) \
        ::HBE::Core::Profiler::ScopeTimer _hbe_prof_##__LINE__(NAME)
#else
    #define HBE_PROFILE_SCOPE(NAME) ((void)0)
#endif
```

That's the whole API. No factories, no builders, no ImGui.

---

## 6. Non-goals (deferred)

| Behavior | Deferred to |
|---|---|
| GPU timer queries | Item 14 |
| CSV / on-disk capture | Item 15 |
| Fixed-timestep interpolation stats | Item 16 |
| Deterministic RNG stream stats | Item 17 |
| Per-thread profiling | Later (multi-thread rewrite) |
| ImGui / engine profiler window | **Never** — item explicitly forbids it |
| Sampling profiler (statistical) | Never in this item — this is deterministic scope timing |
| Cross-process shared memory | Never |

---

## 7. Reading order

1. `00_overview.md`     — you are here
2. `01_profiler_api.md` — new HBE header
3. `02_profiler_impl.md`— new HBE .cpp
4. `03_application_integration.md` — Application.h/.cpp edits + vcxproj/filters wiring
5. `04_megax_instrumentation.md`   — verification pass in MegaX (5+ scopes)
6. `05_build_run_and_verify.md`    — build steps + checklist

Do them in that order. Each doc lists **exact** file
paths, exact insertion points ("right after line XXX", "in
the block that starts with `for (std::size_t i = 0; ...`"),
and full copy-paste code blocks.
