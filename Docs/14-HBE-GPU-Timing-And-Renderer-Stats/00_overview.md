# 14 — [HBE] GPU Timing and Expanded Renderer Statistics (overview)

Item 13 gave us a per-frame CPU profiler. Item 14 completes
the picture: **GPU frame time** measured via OpenGL timer
queries, plus a much richer set of **renderer statistics**
(draw calls, submitted vs. rendered quads, culled sprites,
material/texture changes, visible tile chunks, active lights,
shadow-casting lights, post-process passes).

The `[HBE]` tag says the same thing it did for Item 13:
**all API and implementation edits live under
`G:\Dev\HBE\HBE.Core\` and `G:\Dev\HBE\HBE.Renderer.GL\`**.
The only MegaX edit is doc `05` — verification only — where
MegaX consumes the new snapshot and prints a combined
CPU+GPU line to the console. Sandbox and MapMaker are not
touched.

---

## 1. What Item 14 delivers

### Renderer.GL — new GPU timing infrastructure

* A new header `HBE/Renderer/GpuTimer.h` and matching
  `GpuTimer.cpp` under `HBE.Renderer.GL/`.
* A RAII wrapper `HBE::Renderer::GpuTimerScope` that opens a
  GL timer query on construction and closes it on
  destruction. Nested scopes are legal; scope results are
  aggregated by name.
* An internal query ring buffer (3 frames deep) so the CPU
  never blocks waiting on the GPU. Results are read
  `kResultLatencyFrames` frames later, when the GPU is
  guaranteed done.
* Graceful fallback: if the GL context does not expose
  `glGenQueries` / `GL_TIME_ELAPSED` (i.e. `GL_ARB_timer_query`
  is not present, GLES 2 desktop drivers, etc.), the whole
  system reports `0.0 ms` for every GPU section and never
  issues a query. Ordinary rendering continues.

### Renderer.GL — expanded statistics

* `Renderer2D::Renderer2DStats` gains: `submittedQuads`,
  `renderedQuads`, `culledSprites`, `materialChanges`,
  `textureChanges`. It already had
  `drawCalls`, `quads`, `stateChanges`, `passes`.
* `TileMapRenderer` publishes `visibleTileChunks` per frame
  (currently one "chunk" per drawn layer — every layer whose
  culled AABB intersects the camera).
* `PostProcessStack::present` publishes `postProcessPasses`
  per frame (enabled effect count).
* `Renderer2D::resetFrameStats()` — new public method a game
  must call once per frame (typically from
  `Application::run` — see doc `03`).

### HBE.Core — Profiler extension

* `HBE::Core::Profiler::Snapshot` grows a **new nested
  struct** `Snapshot::gpu` with:
  * `frameMs`, `avgMs`, `minMs`, `maxMs`, `sampleCount`
  * `supported` (`false` if the driver can't do timer queries)
  * `std::vector<Sample> sections;` — GPU-side named
    sections, same shape as CPU `Sample`
* `Snapshot` grows a **new nested struct**
  `Snapshot::renderer` with every extended stat listed
  above, plus two game-populated fields:
  `activeLights`, `shadowCastingLights`.
  Games that don't have a lighting system leave both at 0.
* New publish APIs (called by the renderer, not by games):
  * `Profiler::PublishGpuFrame(uint64_t ns)` — the wall GPU
    time as measured by the outermost timer query
  * `Profiler::PublishGpuSection(const char* name, uint64_t ns, int depth)`
    — one call per closed GpuTimerScope
  * `Profiler::PublishRendererStats(const RendererStats& s)`
    — one call from `Application::run` after all layers have
    rendered
  * `Profiler::SetGpuSupported(bool)` — one call at
    `GLRenderer::initialize` time
  * `Profiler::PublishLightStats(int active, int shadow)`
    — optional; called by games that have a lighting system
* All new snapshot fields are populated **every frame**
  from within `Profiler::EndFrame()`. The rolling-window
  logic reuses the existing 120-frame ring from Item 13.

### HBE.Core — Application integration

* `Application::run` calls the new
  `Renderer2D::resetFrameStats()` right after `beginFrame`
  and calls `PublishRendererStats(...)` right before
  `Profiler::EndFrame()`.
* The 1-Hz `LogInfo` block from Item 13 gains a new
  formatted section listing GPU timings + renderer stats.

### MegaX — verification only

* `GameLayer::onUpdate` reads the merged snapshot after
  every frame and logs one combined line on demand (via a
  hotkey — see doc `05`). No gameplay logic changes.

---

## 2. Success criteria

Item 14 is complete when **all** of the following hold:

1. `HBE.Renderer.GL` and `HBE.Core` build clean in Debug and
   Release.
2. On a driver that supports `GL_ARB_timer_query` (all
   desktop GL 3.3+ vendors — you have this), running MegaX
   Debug prints per second:
   ```
   [Profiler] CPU=2.94ms (avg 3.00) GPU=1.82ms (avg 1.85)
     -- CPU sections (same as Item 13) --
     ApplicationUpdate  ...
     SceneUpdate        ...
       Physics / Combat / AI / Particles
     Audio / TileRendering / SpriteRendering
     -- GPU sections --
     GpuFrame           cur=  1.82 avg=  1.85 min=  1.60 max=  2.30
       GpuScene         cur=  1.42 avg=  1.45 min=  1.30 max=  1.90
       GpuPostProcess   cur=  0.35 avg=  0.32 min=  0.28 max=  0.50
     -- Renderer stats --
     drawCalls=6  passes=1  submittedQuads=182  renderedQuads=182
     culledSprites=0  materialChanges=3  textureChanges=3
     visibleTileChunks=3  postProcessPasses=1
     activeLights=0  shadowCastingLights=0  liveParticles=42
   ```
3. On a driver that does **not** support timer queries
   (forced via the env var
   `HBE_FORCE_NO_GPU_TIMER=1` — see doc `01` §6), running
   MegaX prints `GPU=0.00ms (unsupported)` and every GPU
   section shows `0.00`. **CPU rendering continues**.
4. GPU sample latency is bounded: no more than
   `kResultLatencyFrames` (default 3) frames elapse between a
   scope closing and its ms value appearing in `Snapshot::gpu`.
5. MegaX can log **one combined snapshot line** containing:
   CPU frame time, GPU frame time, draw calls, quads,
   particles, and lights — from a single call to
   `Profiler::GetSnapshot()` (the "one combined performance
   snapshot" clause of the work item).

---

## 3. Golden rules (read before touching code)

1. **`[HBE]` scope.** Only files under
   `G:\Dev\HBE\HBE.Core\` and `G:\Dev\HBE\HBE.Renderer.GL\`
   plus the single MegaX verification file in doc `05`.
   Do **not** open Sandbox, MapMaker, `HBE.Platform.SDL`, or
   any other Renderer subsystem not listed in §4.
2. **Never call `glGetQueryObjectui64v(..., GL_QUERY_RESULT, ...)`
   without checking `GL_QUERY_RESULT_AVAILABLE` first.** A
   blocking read on the current frame's query kills the whole
   point of async GPU timing. The ring in doc `01` always
   reads a query that was submitted N frames ago.
3. **GPU timer queries are main-thread only.** Same rule as
   the CPU profiler (Item 13 golden rule 5).
4. **Graceful degrade is mandatory.** Every GL call in
   `GpuTimer.cpp` must be preceded by an "is supported"
   check. Rendering must NOT break if timer queries are
   unavailable.
5. **Do not add any new GL state changes on the hot path.**
   `glGenQueries` / `glDeleteQueries` happen once at
   initialize. `glBeginQuery` / `glEndQuery` are cheap. Do
   **not** call `glFinish` anywhere in Item 14.
6. **`renderedQuads` == `submittedQuads` minus batch-culled
   quads.** In the current codebase the batch culls nothing
   at flush time, so these two should equal each other for
   now. Doc `02` §5 explains the field so a future frustum
   pass can differ.
7. **`culledSprites` is game-populated.** Renderer.GL only
   sets it to 0 by default. Games with sprite culling (Item
   20+ Golden Room) will publish via `PublishCulledSprites`.
8. **`visibleTileChunks` counts drawn layers, not tiles.**
   The current TileMapRenderer draws whole layers; each
   layer whose culled tile range is non-empty counts as one
   chunk. If a later item adds true chunking, the field
   scales up naturally.
9. **`activeLights` / `shadowCastingLights` are game-side.**
   HBE has no lighting system yet. The Profiler snapshot
   fields default to 0 and stay 0 unless a game calls
   `Profiler::PublishLightStats(active, shadow)`. This is
   the intended future extension point.
10. **`liveParticles` is game-side too.** MegaX gets the
    value from `Effects::liveParticles()` (already exists)
    and passes it through `Profiler::PublishParticleStats(int live)`
    each frame. Renderer.GL does not know about the game's
    particle systems.
11. **Do not repeat Item 13.** The CPU profiler API is
    frozen. This item **adds** fields to `Snapshot` and
    publishes them from Renderer.GL. Do NOT change the
    existing CPU RAII scope, macro, or snapshot layout.

---

## 4. Files touched

| File | Item 13 | Item 14 |
|---|---|---|
| `HBE.Renderer.GL/include/HBE/Renderer/GpuTimer.h` | — | **NEW** — GL timer query ring + RAII scope |
| `HBE.Renderer.GL/src/Renderer/GpuTimer.cpp` | — | **NEW** — implementation, capability probe, fallback |
| `HBE.Renderer.GL/HBE.Renderer.GL.vcxproj` (+ `.filters`) | — | register `GpuTimer.h/.cpp` |
| `HBE.Renderer.GL/include/HBE/Renderer/Renderer2D.h` | — | extend `Renderer2DStats`; add `resetFrameStats()` |
| `HBE.Renderer.GL/src/Renderer/Renderer2D.cpp` | — | populate new stats; implement `resetFrameStats()` |
| `HBE.Renderer.GL/include/HBE/Renderer/SpriteBatch2D.h` | — | add `materialChanges()`, `textureChanges()` accessors |
| `HBE.Renderer.GL/src/Renderer/SpriteBatch2D.cpp` | — | count material vs. texture state changes separately |
| `HBE.Renderer.GL/include/HBE/Renderer/TileMapRenderer.h` | — | add `visibleTileChunks()` accessor |
| `HBE.Renderer.GL/src/Renderer/TileMapRenderer.cpp` | — | increment `m_visibleChunks` per drawn layer; publish to Profiler |
| `HBE.Renderer.GL/include/HBE/Renderer/PostProcessStack.h` | — | add `lastPassCount()` accessor |
| `HBE.Renderer.GL/src/Renderer/PostProcessStack.cpp` | — | store `m_lastPassCount` in `present(...)` |
| `HBE.Renderer.GL/src/Renderer/GLRenderer.cpp` | — | wrap `beginFrameInViewport` -> `endFrame` in `GpuTimerScope("GpuFrame")` with a nested `GpuScene`/`GpuPostProcess` |
| `HBE.Core/include/HBE/Core/Profiler.h` | new API | extend `Snapshot` with `gpu` + `renderer` sub-structs; declare 5 new publish APIs |
| `HBE.Core/src/Core/Profiler.cpp` | new impl | implement the 5 new publish APIs + rolling GPU ring |
| `HBE.Core/src/Core/Application.cpp` | 1-Hz log + BeginFrame/EndFrame | call `renderer2D().resetFrameStats()` per frame; call `Profiler::PublishRendererStats(...)` before `EndFrame()`; extend 1-Hz LogInfo block |
| `MegaX/src/Game/GameLayer.cpp` | 7 CPU scopes | **verification only** — hotkey `F8` prints one combined snapshot line; publish `liveParticles` via Profiler each frame |

**Not touched:** `HBE.Platform.SDL`, Sandbox, MapMaker,
Renderer2D's `Camera2D`, all of ECS, all of Input.

---

## 5. Design shape

### 5.1  GL timer query ring

```
Frame N   : glBeginQuery("GpuFrame") -> id = ringSlot[N%3].outer
Frame N+1 : slot N might not be ready yet
Frame N+2 : we peek at slot N; QUERY_RESULT_AVAILABLE?
Frame N+3 : if not ready by now, we drop the sample and log a warning
```

Every named GPU scope has its **own** small ring
(`kResultLatencyFrames + 1 = 4` slots) so nested scopes
don't stomp each other. The outer `GpuFrame` scope wraps all
inner ones — its ring is the "authoritative" frame timing.

### 5.2  Where scopes are opened

| Scope | Where |
|---|---|
| `GpuFrame` | RAII in `GLRenderer::beginFrameInViewport` → `endFrame` |
| `GpuScene` | Wraps the layer render for-loop (from `Application::run`) |
| `GpuPostProcess` | Wraps `m_postProcess->present(...)` inside `GLRenderer::endFrame` |

Games can add more (`GpuUI`, `GpuBloom`, etc.) with
`HBE_GPU_SCOPE("Name")` — same macro pattern as CPU.

### 5.3  Snapshot extension

Cheap summary of the new struct layout inside
`Profiler::Snapshot`:

```cpp
struct RendererStats {
    int drawCalls          = 0;
    int passes             = 0;
    int submittedQuads     = 0;
    int renderedQuads      = 0;
    int culledSprites      = 0;
    int materialChanges    = 0;
    int textureChanges     = 0;
    int visibleTileChunks  = 0;
    int postProcessPasses  = 0;
    int activeLights       = 0;
    int shadowCastingLights = 0;
    int liveParticles      = 0;
};

struct GpuTimings {
    bool                supported     = false;
    double              frameMs       = 0.0;
    double              frameAvgMs    = 0.0;
    double              frameMinMs    = 0.0;
    double              frameMaxMs    = 0.0;
    std::size_t         frameSampleCount = 0;
    std::vector<Sample> sections;   // same Sample struct as CPU
};

struct Snapshot {
    // ... (existing Item 13 CPU fields) ...
    GpuTimings     gpu;
    RendererStats  renderer;
};
```

### 5.4  Publish flow

```
GLRenderer::beginFrameInViewport
  ├── open GpuFrame timer query
  └── open GpuScene timer query
Application::run render loop
  ├── each layer's onRender()
  └── layer submits draws -> SpriteBatch2D counts stateChanges
GLRenderer::endFrame
  ├── close GpuScene
  ├── open GpuPostProcess
  ├── PostProcessStack::present() -> m_lastPassCount = enabledCount
  ├── close GpuPostProcess
  ├── close GpuFrame
  └── GpuTimer::ReadReady() -> Profiler::PublishGpuFrame/GpuSection
Application::run
  ├── Profiler::PublishRendererStats(renderer2D().getStats() + TileMapRenderer::visibleTileChunks() + PostProcessStack::lastPassCount())
  └── Profiler::EndFrame()
```

`Profiler::EndFrame()` builds the final snapshot as usual
(Item 13 logic) plus copies the new gpu/renderer sub-structs.

---

## 6. Non-goals (deferred)

| Behavior | Deferred to |
|---|---|
| CSV / on-disk capture of the snapshot | Item 15 |
| Fixed timestep + interpolation stats | Item 16 |
| Deterministic RNG stats | Item 17 |
| Game-defined input action stats | Item 18 |
| Component serializer stats | Item 19 |
| GPU memory usage tracking | Later — no cross-vendor API |
| Per-shader / per-material breakdown | Later — one flat "materialChanges" is enough for now |
| Vulkan / Metal backends | Never in this repo (renderer is `HBE.Renderer.GL` only) |
| A profiler window / ImGui integration | Never — item explicitly forbids UI |

---

## 7. Reading order

1. `00_overview.md`                       — you are here
2. `01_gpu_timer_ring.md`                 — new `GpuTimer.h/.cpp` in Renderer.GL
3. `02_renderer_stats.md`                 — extend Renderer2D / SpriteBatch2D / TileMapRenderer / PostProcessStack stats
4. `03_glrenderer_frame_scope.md`         — wire GpuTimer scopes into GLRenderer
5. `04_profiler_gpu_integration.md`       — extend HBE.Core Profiler snapshot + publish APIs + Application 1-Hz log
6. `05_megax_verification.md`             — MegaX consumer edits (single file, verification only)
7. `06_build_run_and_verify.md`           — build/run/verify + troubleshooting

Do them in order. Each doc lists exact file paths, exact
insertion points, and full copy-paste code blocks — same
style as Item 13.
