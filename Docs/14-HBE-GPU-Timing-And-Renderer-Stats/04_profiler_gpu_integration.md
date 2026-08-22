# 04 — Profiler GPU + Renderer stats integration

This doc extends the HBE.Core Profiler from Item 13 with:

* `Snapshot::GpuTimings gpu`
* `Snapshot::RendererStats renderer`
* 5 new publish APIs:
  * `SetGpuSupported(bool)`
  * `PublishGpuSection(const char* name, uint64_t ns, int depth)`
  * `PublishGpuFrame(uint64_t ns)`
  * `PublishRendererStats(const RendererStats&)`
  * `PublishLightStats(int active, int shadow)` (game-side hook)
  * `PublishParticleStats(int live)` (game-side hook)

Two files edited:

* `HBE.Core/include/HBE/Core/Profiler.h`
* `HBE.Core/src/Core/Profiler.cpp`

No new files. No vcxproj edits. No new dependencies.

---

## 1. `Profiler.h` — add new structs + declarations

Open `G:\Dev\HBE\HBE.Core\include\HBE\Core\Profiler.h`.

### 1.1  Add `RendererStats` + `GpuTimings` structs

Find the existing `struct Sample { ... };` definition. Right
AFTER it (before `struct Snapshot`), add:

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
		bool                supported        = false;
		double              frameMs          = 0.0;
		double              frameAvgMs       = 0.0;
		double              frameMinMs       = 0.0;
		double              frameMaxMs       = 0.0;
		std::size_t         frameSampleCount = 0;
		std::vector<Sample> sections;
	};
```

### 1.2  Extend `Snapshot`

Find the existing `Snapshot` struct. Just before its closing
`};`, add two members:

```cpp
	struct Snapshot {
		double                frameMs      = 0.0;
		double                frameAvgMs   = 0.0;
		double                frameMinMs   = 0.0;
		double                frameMaxMs   = 0.0;
		std::size_t           frameSampleCount = 0;
		std::uint64_t         frameIndex   = 0;
		std::vector<Sample>   sections;

		// -------- Item 14 additions --------
		GpuTimings            gpu;
		RendererStats         renderer;
	};
```

### 1.3  Add new function declarations

Below the existing `int BeginScope(const char* name);` /
`void EndScope(int index);` declarations, before the
`ScopeTimer` class, add:

```cpp
	// -------------------------------------------------------------------------
	// Item 14: GPU + renderer publish APIs.
	// Called by HBE.Renderer.GL (GpuTimer / GLRenderer) and by games with
	// lighting / particle systems. All are safe when Profiler is disabled.
	// -------------------------------------------------------------------------
	void SetGpuSupported(bool supported);

	// Called from GpuTimer::NewFrame() every time a query result becomes ready.
	// 'ns' is nanoseconds elapsed on the GPU. 'depth' is the display depth
	// (currently 0 for both GpuScene and GpuPostProcess since they are serial).
	void PublishGpuSection(const char* name, std::uint64_t ns, int depth);

	// Optional: publish the outer frame time separately. Not used when frame
	// time is derived from the sum of top-level sections (default). Provided
	// for future use.
	void PublishGpuFrame(std::uint64_t ns);

	// Called by Application::run every frame after all layers have rendered.
	void PublishRendererStats(const RendererStats& stats);

	// Game-side. Called every frame by any game that has a lighting system.
	// Default 0/0 if never called.
	void PublishLightStats(int activeLights, int shadowCastingLights);

	// Game-side. Called every frame by any game that has a particle system.
	// MegaX plugs Effects::liveParticles() into this. Default 0 if never called.
	void PublishParticleStats(int liveParticles);
```

That's the full header extension.

---

## 2. `Profiler.cpp` — add storage + publish functions

Open `G:\Dev\HBE\HBE.Core\src\Core\Profiler.cpp`.

### 2.1  Extend `SectionStats` for GPU sections

We reuse the existing `SectionStats` layout for GPU sections
because it's already the exact right shape (rolling ring, avg
/ min / max, currentMs).

Inside the anonymous namespace at the top, right below the
existing `SectionStats` struct, add:

```cpp
		struct GpuSectionStats {
			const char*                              name          = nullptr;
			int                                      depth         = 0;
			std::size_t                              writeCursor   = 0;
			std::array<double, kRollingWindowFrames> samplesMs{};
			std::size_t                              count         = 0;
			double                                   currentMs     = 0.0;
			double                                   avgMs         = 0.0;
			double                                   minMs         = 0.0;
			double                                   maxMs         = 0.0;
			std::size_t                              opensThisFrame = 0;
			bool                                     seenThisFrame  = false;
		};
```

(Identical layout to `SectionStats` — separate type so we
don't grow the CPU table when a GPU-only name appears.)

### 2.2  Extend `State` with GPU + renderer fields

Inside `struct State { ... }`, right below the existing frame
rolling ring block, add:

```cpp
			// ---- Item 14 additions ----
			bool                                       gpuSupported = false;
			std::array<GpuSectionStats, kMaxSections>  gpuSections{};
			std::size_t                                gpuSectionCount = 0;
			// Rolling ring for outer GPU frame time (sum of top-level sections).
			std::array<double, kRollingWindowFrames>   gpuFrameSamplesMs{};
			std::size_t                                gpuFrameCursor  = 0;
			std::size_t                                gpuFrameSampleCount = 0;
			double                                     gpuFrameAvgMs   = 0.0;
			double                                     gpuFrameMinMs   = 0.0;
			double                                     gpuFrameMaxMs   = 0.0;

			RendererStats                              renderer{};
			bool                                       rendererValid   = false;
```

### 2.3  Add helper: `findOrAddGpuSection`

Right below the existing `findOrAddSection` helper, add:

```cpp
		int findOrAddGpuSection(const char* name, int depth) {
			State& s = S();
			for (std::size_t i = 0; i < s.gpuSectionCount; ++i) {
				if (s.gpuSections[i].name == name) {
					return static_cast<int>(i);
				}
			}
			if (s.gpuSectionCount >= kMaxSections) return -1;

			GpuSectionStats& st = s.gpuSections[s.gpuSectionCount];
			st = GpuSectionStats{};
			st.name  = name;
			st.depth = depth;

			const int idx = static_cast<int>(s.gpuSectionCount);
			++s.gpuSectionCount;
			return idx;
		}

		void recomputeGpuRollingStats(GpuSectionStats& st) {
			if (st.count == 0) {
				st.avgMs = 0.0; st.minMs = 0.0; st.maxMs = 0.0;
				return;
			}
			double sum = 0.0;
			double mn = std::numeric_limits<double>::infinity();
			double mx = -std::numeric_limits<double>::infinity();
			for (std::size_t i = 0; i < st.count; ++i) {
				const double v = st.samplesMs[i];
				sum += v;
				if (v < mn) mn = v;
				if (v > mx) mx = v;
			}
			st.avgMs = sum / static_cast<double>(st.count);
			st.minMs = mn;
			st.maxMs = mx;
		}

		void recomputeGpuFrameStats(State& s) {
			if (s.gpuFrameSampleCount == 0) {
				s.gpuFrameAvgMs = 0.0; s.gpuFrameMinMs = 0.0; s.gpuFrameMaxMs = 0.0;
				return;
			}
			double sum = 0.0;
			double mn = std::numeric_limits<double>::infinity();
			double mx = -std::numeric_limits<double>::infinity();
			for (std::size_t i = 0; i < s.gpuFrameSampleCount; ++i) {
				const double v = s.gpuFrameSamplesMs[i];
				sum += v;
				if (v < mn) mn = v;
				if (v > mx) mx = v;
			}
			s.gpuFrameAvgMs = sum / static_cast<double>(s.gpuFrameSampleCount);
			s.gpuFrameMinMs = mn;
			s.gpuFrameMaxMs = mx;
		}
```

### 2.4  Update `BeginFrame` to reset GPU per-frame counters

Find the existing `BeginFrame()` implementation. It already
resets CPU per-frame counters at the top. Add a matching
GPU reset block **inside** the same function, right after
the CPU reset loop:

```cpp
	void BeginFrame() {
		State& s = S();
		if (!s.enabled) return;

		// Reset per-frame counters on every section.
		for (std::size_t i = 0; i < s.sectionCount; ++i) {
			s.sections[i].currentMs      = 0.0;
			s.sections[i].opensThisFrame = 0;
			s.sections[i].seenThisFrame  = false;
		}

		// -------- Item 14: reset GPU per-frame counters --------
		for (std::size_t i = 0; i < s.gpuSectionCount; ++i) {
			s.gpuSections[i].currentMs      = 0.0;
			s.gpuSections[i].opensThisFrame = 0;
			s.gpuSections[i].seenThisFrame  = false;
		}
		s.rendererValid = false;

		s.scopeStackSize = 0;
		s.currentDepth   = 0;
		s.frameStartNs   = NowNs();
		s.inFrame        = true;
	}
```

**IMPORTANT:** we do NOT reset `renderer.activeLights`,
`renderer.shadowCastingLights`, or `renderer.liveParticles`
here. Games publish those from their `onUpdate`, which runs
BEFORE `BeginFrame()`? — Actually no, `BeginFrame()` runs at
the very top of the loop, before layer `onUpdate`. So the
game's publish call **overwrites** them after BeginFrame
resets. To keep publishes idempotent, we skip resetting
those game fields here — the `PublishRendererStats` call
handles them last.

Actually, cleaner rule: `PublishRendererStats` merges (doesn't
replace) with light + particle values already set by the
game earlier this frame. See §2.6 for the merge logic.

### 2.5  Extend `EndFrame` — build GPU rolling stats + snapshot

Find the existing `EndFrame()` implementation. It ends with:

```cpp
		s.snapshot.frameMs       = frameMs;
		s.snapshot.frameAvgMs    = s.frameAvgMs;
		...
		s.snapshot.sections.push_back(smp);
		}

		s.inFrame = false;
	}
```

Insert (right before the closing `s.inFrame = false;`) the
GPU rolling-window bookkeeping + snapshot copy:

```cpp
		// -------- Item 14: GPU rolling window + snapshot --------
		double gpuFrameSumMs = 0.0;
		for (std::size_t i = 0; i < s.gpuSectionCount; ++i) {
			GpuSectionStats& st = s.gpuSections[i];
			if (st.opensThisFrame == 0) continue;

			// Ring insert
			st.samplesMs[st.writeCursor] = st.currentMs;
			st.writeCursor = (st.writeCursor + 1) % kRollingWindowFrames;
			if (st.count < kRollingWindowFrames) ++st.count;
			recomputeGpuRollingStats(st);

			if (st.depth == 0) gpuFrameSumMs += st.currentMs;
		}

		// Roll outer GPU frame ring only if we got any samples this frame.
		if (s.gpuSectionCount > 0 && s.gpuSupported) {
			s.gpuFrameSamplesMs[s.gpuFrameCursor] = gpuFrameSumMs;
			s.gpuFrameCursor = (s.gpuFrameCursor + 1) % kRollingWindowFrames;
			if (s.gpuFrameSampleCount < kRollingWindowFrames) ++s.gpuFrameSampleCount;
			recomputeGpuFrameStats(s);
		}

		s.snapshot.gpu.supported        = s.gpuSupported;
		s.snapshot.gpu.frameMs          = gpuFrameSumMs;
		s.snapshot.gpu.frameAvgMs       = s.gpuFrameAvgMs;
		s.snapshot.gpu.frameMinMs       = s.gpuFrameMinMs;
		s.snapshot.gpu.frameMaxMs       = s.gpuFrameMaxMs;
		s.snapshot.gpu.frameSampleCount = s.gpuFrameSampleCount;
		s.snapshot.gpu.sections.clear();
		s.snapshot.gpu.sections.reserve(s.gpuSectionCount);
		for (std::size_t i = 0; i < s.gpuSectionCount; ++i) {
			const GpuSectionStats& st = s.gpuSections[i];
			Sample g{};
			g.name           = st.name;
			g.depth          = st.depth;
			g.currentMs      = st.currentMs;
			g.avgMs          = st.avgMs;
			g.minMs          = st.minMs;
			g.maxMs          = st.maxMs;
			g.sampleCount    = st.count;
			g.opensThisFrame = st.opensThisFrame;
			s.snapshot.gpu.sections.push_back(g);
		}

		s.snapshot.renderer = s.renderer;
```

### 2.6  Add the publish functions at the end of the file

Right above the closing `} // namespace HBE::Core::Profiler`,
add:

```cpp
	// -------------------------------------------------------------------------
	// Item 14: publish functions
	// -------------------------------------------------------------------------

	void SetGpuSupported(bool supported) {
		S().gpuSupported = supported;
	}

	void PublishGpuSection(const char* name, std::uint64_t ns, int depth) {
		State& s = S();
		if (!s.enabled) return;
		if (name == nullptr) return;

		const int idx = findOrAddGpuSection(name, depth);
		if (idx < 0) return;

		GpuSectionStats& st = s.gpuSections[idx];
		const double ms = static_cast<double>(ns) / 1'000'000.0;
		st.currentMs += ms;
		++st.opensThisFrame;
		if (!st.seenThisFrame) {
			st.depth = depth;
			st.seenThisFrame = true;
		}
	}

	void PublishGpuFrame(std::uint64_t /*ns*/) {
		// Reserved for future use; currently gpu.frameMs is computed as the
		// sum of depth-0 GPU sections inside EndFrame().
	}

	void PublishRendererStats(const RendererStats& stats) {
		State& s = S();
		if (!s.enabled) return;

		// Merge: renderer-owned fields take the incoming value, game-owned
		// fields (lights, particles) preserve whatever a game already published
		// this frame. If the game hasn't published yet, those stay at 0 or
		// their previous frame's value.
		const int prevActiveLights   = s.renderer.activeLights;
		const int prevShadowLights   = s.renderer.shadowCastingLights;
		const int prevLiveParticles  = s.renderer.liveParticles;
		const int prevTileChunks     = s.renderer.visibleTileChunks;
		const int prevPPPasses       = s.renderer.postProcessPasses;

		s.renderer = stats;

		// Preserve game-owned fields if the caller didn't set them (default 0).
		if (stats.activeLights == 0)        s.renderer.activeLights = prevActiveLights;
		if (stats.shadowCastingLights == 0) s.renderer.shadowCastingLights = prevShadowLights;
		if (stats.liveParticles == 0)       s.renderer.liveParticles = prevLiveParticles;
		if (stats.visibleTileChunks == 0)   s.renderer.visibleTileChunks = prevTileChunks;
		if (stats.postProcessPasses == 0)   s.renderer.postProcessPasses = prevPPPasses;

		s.rendererValid = true;
	}

	void PublishLightStats(int activeLights, int shadowCastingLights) {
		State& s = S();
		if (!s.enabled) return;
		s.renderer.activeLights = activeLights;
		s.renderer.shadowCastingLights = shadowCastingLights;
	}

	void PublishParticleStats(int liveParticles) {
		State& s = S();
		if (!s.enabled) return;
		s.renderer.liveParticles = liveParticles;
	}
```

### 2.7  Update `Reset()` to clear GPU + renderer state

Find `Reset()` and extend it. Currently:

```cpp
	void Reset() {
		State& s = S();
		s.sections     = {};
		s.sectionCount = 0;
		...
		s.snapshot     = Snapshot{};
		s.inFrame      = false;
		s.frameIndex   = 0;
	}
```

Right before the closing `}`:

```cpp
		s.gpuSections     = {};
		s.gpuSectionCount = 0;
		s.gpuFrameSamplesMs = {};
		s.gpuFrameCursor  = 0;
		s.gpuFrameSampleCount = 0;
		s.gpuFrameAvgMs   = 0.0;
		s.gpuFrameMinMs   = 0.0;
		s.gpuFrameMaxMs   = 0.0;
		// note: gpuSupported preserved — capability doesn't change on scene reload
		s.renderer        = RendererStats{};
		s.rendererValid   = false;
	}
```

---

## 3. `TileMapRenderer` + `PostProcessStack` publish

The two subsystems track `visibleTileChunks` /
`postProcessPasses` locally (doc `02` §5-§8). Renderer.GL
publishes them to the Profiler through the game — because
not every HBE game uses them.

Doc `05` §3 shows the exact MegaX line that publishes both.

For engine games that DO always use TileMapRenderer + a
`PostProcessStack`, the Application-level publish in doc
`03` §4.4 is the natural hook. The current wiring intentionally
leaves them at 0 unless the game overwrites — that keeps the
API composable.

---

## 4. Sanity check

```powershell
Select-String -Path G:\Dev\HBE\HBE.Core\include\HBE\Core\Profiler.h -Pattern "PublishGpuSection|PublishGpuFrame|PublishRendererStats|SetGpuSupported|PublishLightStats|PublishParticleStats|GpuTimings|RendererStats"
# -> expect 8+ matches

Select-String -Path G:\Dev\HBE\HBE.Core\src\Core\Profiler.cpp -Pattern "gpuSection|gpuFrame|rendererValid|PublishRendererStats"
# -> expect many matches (implementation)
```

Now try a partial build of just `HBE.Core`:

```
msbuild HonestlyBadEngine.slnx /p:Configuration=Debug /p:Platform=x64 /t:HBE_Core /m /nologo /v:m
```

Expected: **builds clean**. `Profiler.cpp` links to no new
symbols outside itself; it just added functions.

Then try `HBE.Renderer.GL`:

```
msbuild HonestlyBadEngine.slnx /p:Configuration=Debug /p:Platform=x64 /t:HBE_Renderer_GL /m /nologo /v:m
```

Expected: **builds clean**. `GpuTimer.cpp` links against
`HBE::Core::Profiler::SetGpuSupported` and
`PublishGpuSection`, both of which now exist.

If either target fails, the errors point you back at whichever
doc missed a step. Common ones:

* `error C2039: 'RendererStats': is not a member of 'HBE::Core::Profiler'`
  — doc §1.1 skipped.
* `error C2039: 'gpu': is not a member of 'HBE::Core::Profiler::Snapshot'`
  — doc §1.2 skipped.
* `error LNK2019: unresolved external symbol PublishGpuSection`
  — doc §2.6 skipped.

Next: `05_megax_verification.md`.
