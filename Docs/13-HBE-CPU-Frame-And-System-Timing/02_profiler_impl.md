# 02 — Profiler implementation (`src/Core/Profiler.cpp`)

This doc creates the **new** file
`G:\Dev\HBE\HBE.Core\src\Core\Profiler.cpp`. It implements
every symbol declared in doc `01`.

---

## 1. Create the file

**Full path:** `G:\Dev\HBE\HBE.Core\src\Core\Profiler.cpp`

**Status:** must not exist yet.

Paste the following content **verbatim**:

```cpp
#include "HBE/Core/Profiler.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <limits>

namespace HBE::Core::Profiler {

    // -------------------------------------------------------------------------
    // Storage
    // -------------------------------------------------------------------------
    namespace {

        struct SectionStats {
            const char*                              name          = nullptr;
            int                                      depth         = 0;    // depth on first open in current frame
            std::size_t                              writeCursor   = 0;
            std::array<double, kRollingWindowFrames> samplesMs{};
            std::size_t                              count         = 0;
            double                                   currentMs     = 0.0;  // sum of opens this frame
            double                                   avgMs         = 0.0;
            double                                   minMs         = 0.0;
            double                                   maxMs         = 0.0;
            std::size_t                              opensThisFrame = 0;
            bool                                     seenThisFrame = false;
        };

        struct OpenScope {
            const char*    name    = nullptr;
            std::uint64_t  startNs = 0;
            int            depth   = 0;
            int            sectionIndex = -1;
        };

        struct State {
            bool                                       enabled       = true;
            bool                                       inFrame       = false;
            std::uint64_t                              frameStartNs  = 0;
            std::uint64_t                              frameIndex    = 0;

            // Named sections in stable first-seen order.
            std::array<SectionStats, kMaxSections>     sections{};
            std::size_t                                sectionCount  = 0;

            // Open scope stack for the current frame.
            std::array<OpenScope, kMaxScopesPerFrame>  scopeStack{};
            std::size_t                                scopeStackSize = 0;
            int                                        currentDepth  = 0;

            // Rolling stats for total frame wall-time.
            std::array<double, kRollingWindowFrames>   frameSamplesMs{};
            std::size_t                                frameCursor   = 0;
            std::size_t                                frameSampleCount = 0;
            double                                     frameAvgMs    = 0.0;
            double                                     frameMinMs    = 0.0;
            double                                     frameMaxMs    = 0.0;

            // Published snapshot.
            Snapshot                                   snapshot{};
        };

        State& S() {
            static State s;
            return s;
        }

        // Locate an existing section by pointer identity, or create a new one.
        int findOrAddSection(const char* name, int depth) {
            State& s = S();
            for (std::size_t i = 0; i < s.sectionCount; ++i) {
                if (s.sections[i].name == name) {
                    return static_cast<int>(i);
                }
            }
            if (s.sectionCount >= kMaxSections) {
                return -1;   // silently drop past capacity
            }
            SectionStats& st = s.sections[s.sectionCount];
            st            = SectionStats{};
            st.name       = name;
            st.depth      = depth;
            const int idx = static_cast<int>(s.sectionCount);
            ++s.sectionCount;
            return idx;
        }

        void recomputeRollingStats(SectionStats& st) {
            if (st.count == 0) {
                st.avgMs = 0.0;
                st.minMs = 0.0;
                st.maxMs = 0.0;
                return;
            }
            double sum = 0.0;
            double mn  = std::numeric_limits<double>::infinity();
            double mx  = -std::numeric_limits<double>::infinity();
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

        void recomputeFrameStats(State& s) {
            if (s.frameSampleCount == 0) {
                s.frameAvgMs = 0.0;
                s.frameMinMs = 0.0;
                s.frameMaxMs = 0.0;
                return;
            }
            double sum = 0.0;
            double mn  = std::numeric_limits<double>::infinity();
            double mx  = -std::numeric_limits<double>::infinity();
            for (std::size_t i = 0; i < s.frameSampleCount; ++i) {
                const double v = s.frameSamplesMs[i];
                sum += v;
                if (v < mn) mn = v;
                if (v > mx) mx = v;
            }
            s.frameAvgMs = sum / static_cast<double>(s.frameSampleCount);
            s.frameMinMs = mn;
            s.frameMaxMs = mx;
        }

    } // anonymous namespace

    // -------------------------------------------------------------------------
    // Public API
    // -------------------------------------------------------------------------

    std::uint64_t NowNs() {
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(now).count()
        );
    }

    void SetEnabled(bool on) { S().enabled = on; }
    bool IsEnabled()         { return S().enabled; }

    void Reset() {
        State& s = S();
        s.sections     = {};
        s.sectionCount = 0;
        s.scopeStack   = {};
        s.scopeStackSize = 0;
        s.currentDepth = 0;
        s.frameSamplesMs = {};
        s.frameCursor  = 0;
        s.frameSampleCount = 0;
        s.frameAvgMs   = 0.0;
        s.frameMinMs   = 0.0;
        s.frameMaxMs   = 0.0;
        s.snapshot     = Snapshot{};
        s.inFrame      = false;
        s.frameIndex   = 0;
    }

    void BeginFrame() {
        State& s = S();
        if (!s.enabled) return;

        // Reset per-frame counters on every section.
        for (std::size_t i = 0; i < s.sectionCount; ++i) {
            s.sections[i].currentMs      = 0.0;
            s.sections[i].opensThisFrame = 0;
            s.sections[i].seenThisFrame  = false;
        }
        s.scopeStackSize = 0;
        s.currentDepth   = 0;
        s.frameStartNs   = NowNs();
        s.inFrame        = true;
    }

    int BeginScope(const char* name) {
        State& s = S();
        if (!s.enabled || !s.inFrame || name == nullptr) return -1;
        if (s.scopeStackSize >= kMaxScopesPerFrame) return -1;

        const int sectionIdx = findOrAddSection(name, s.currentDepth);
        if (sectionIdx < 0) return -1;

        OpenScope& os = s.scopeStack[s.scopeStackSize];
        os.name         = name;
        os.startNs      = NowNs();
        os.depth        = s.currentDepth;
        os.sectionIndex = sectionIdx;

        // Preserve first-seen depth for the section (used only for display).
        SectionStats& st = s.sections[sectionIdx];
        if (!st.seenThisFrame) {
            st.depth = s.currentDepth;
            st.seenThisFrame = true;
        }

        ++s.scopeStackSize;
        ++s.currentDepth;
        return static_cast<int>(s.scopeStackSize) - 1;
    }

    void EndScope(int index) {
        State& s = S();
        if (index < 0) return;
        if (!s.enabled || !s.inFrame) return;
        if (index >= static_cast<int>(s.scopeStackSize)) return;

        // Close the scope at 'index'. In well-formed RAII usage the closing
        // scope is always the top of the stack, but tolerate mismatched close
        // by walking down.
        while (s.scopeStackSize > 0 &&
               static_cast<int>(s.scopeStackSize) - 1 >= index) {
            const std::size_t top = s.scopeStackSize - 1;
            OpenScope& os = s.scopeStack[top];

            const std::uint64_t endNs = NowNs();
            const std::uint64_t deltaNs = (endNs > os.startNs) ? (endNs - os.startNs) : 0;
            const double deltaMs = static_cast<double>(deltaNs) / 1'000'000.0;

            if (os.sectionIndex >= 0 &&
                static_cast<std::size_t>(os.sectionIndex) < s.sectionCount) {
                SectionStats& st = s.sections[os.sectionIndex];
                st.currentMs += deltaMs;
                ++st.opensThisFrame;
            }

            os = OpenScope{};
            --s.scopeStackSize;
            if (s.currentDepth > 0) --s.currentDepth;

            if (static_cast<int>(s.scopeStackSize) <= index) break;
        }
    }

    void EndFrame() {
        State& s = S();
        if (!s.enabled) return;
        if (!s.inFrame) return;

        // Force-close anything the caller forgot to close (should be zero in
        // practice — RAII guarantees it in Application::run).
        while (s.scopeStackSize > 0) {
            EndScope(static_cast<int>(s.scopeStackSize) - 1);
        }

        const std::uint64_t endNs = NowNs();
        const std::uint64_t deltaNs = (endNs > s.frameStartNs) ? (endNs - s.frameStartNs) : 0;
        const double frameMs = static_cast<double>(deltaNs) / 1'000'000.0;

        // Frame rolling ring
        s.frameSamplesMs[s.frameCursor] = frameMs;
        s.frameCursor = (s.frameCursor + 1) % kRollingWindowFrames;
        if (s.frameSampleCount < kRollingWindowFrames) ++s.frameSampleCount;
        recomputeFrameStats(s);

        // Per-section rolling rings — only for sections that were opened this
        // frame. Sections that weren't touched keep their previous rolling
        // window; a zero sample would falsely lower their averages.
        for (std::size_t i = 0; i < s.sectionCount; ++i) {
            SectionStats& st = s.sections[i];
            if (st.opensThisFrame == 0) continue;
            st.samplesMs[st.writeCursor] = st.currentMs;
            st.writeCursor = (st.writeCursor + 1) % kRollingWindowFrames;
            if (st.count < kRollingWindowFrames) ++st.count;
            recomputeRollingStats(st);
        }

        // Publish snapshot
        s.snapshot.frameMs       = frameMs;
        s.snapshot.frameAvgMs    = s.frameAvgMs;
        s.snapshot.frameMinMs    = s.frameMinMs;
        s.snapshot.frameMaxMs    = s.frameMaxMs;
        s.snapshot.frameSampleCount = s.frameSampleCount;
        s.snapshot.frameIndex    = ++s.frameIndex;
        s.snapshot.sections.clear();
        s.snapshot.sections.reserve(s.sectionCount);
        for (std::size_t i = 0; i < s.sectionCount; ++i) {
            const SectionStats& st = s.sections[i];
            Sample smp{};
            smp.name           = st.name;
            smp.depth          = st.depth;
            smp.currentMs      = st.currentMs;
            smp.avgMs          = st.avgMs;
            smp.minMs          = st.minMs;
            smp.maxMs          = st.maxMs;
            smp.sampleCount    = st.count;
            smp.opensThisFrame = st.opensThisFrame;
            s.snapshot.sections.push_back(smp);
        }

        s.inFrame = false;
    }

    const Snapshot& GetSnapshot() {
        return S().snapshot;
    }

} // namespace HBE::Core::Profiler
```

---

## 2. Register `Profiler.cpp` in `HBE.Core.vcxproj`

Open `G:\Dev\HBE\HBE.Core\HBE.Core.vcxproj`.

Find the `<ItemGroup>` block containing `ClCompile` entries.
The first `<ClCompile>` line in that block is
`Include="include\HBE\ECS\Entity.h"` (this is intentionally
misfiled as a `ClCompile` on the current codebase — leave it
alone).

Right after the line:

```xml
    <ClCompile Include="src\Core\Log.cpp" />
```

and before the line:

```xml
    <ClCompile Include="src\Core\Time.cpp" />
```

insert:

```xml
    <ClCompile Include="src\Core\Profiler.cpp" />
```

So the resulting subsection reads:

```xml
    <ClCompile Include="src\Core\Log.cpp" />
    <ClCompile Include="src\Core\Profiler.cpp" />
    <ClCompile Include="src\Core\Time.cpp" />
```

---

## 3. Register `Profiler.cpp` in `HBE.Core.vcxproj.filters`

Open `G:\Dev\HBE\HBE.Core\HBE.Core.vcxproj.filters`.

Find the second `<ItemGroup>` block (the one with
`ClCompile` entries). Right after the line:

```xml
    <ClCompile Include="src\Core\Log.cpp" />
```

insert:

```xml
    <ClCompile Include="src\Core\Profiler.cpp" />
```

So the resulting order matches the vcxproj:

```xml
    <ClCompile Include="src\Core\Log.cpp" />
    <ClCompile Include="src\Core\Profiler.cpp" />
    <ClCompile Include="src\Core\Time.cpp" />
```

---

## 4. Sanity check

```powershell
Test-Path G:\Dev\HBE\HBE.Core\src\Core\Profiler.cpp
# -> True

Get-Content G:\Dev\HBE\HBE.Core\HBE.Core.vcxproj | Select-String "Profiler"
# -> TWO matches (Profiler.h and Profiler.cpp)

Get-Content G:\Dev\HBE\HBE.Core\HBE.Core.vcxproj.filters | Select-String "Profiler"
# -> TWO matches
```

Do not build yet. Even though the profiler now compiles on
its own, `Application.h/.cpp` haven't been wired to
`BeginFrame`/`EndFrame` — and nothing is calling
`HBE_PROFILE_SCOPE`, so the compile would succeed but no
timings would ever be recorded. Doc `03` handles that.

---

## 5. Design notes (read before doc 03)

**Why `bool seenThisFrame` on SectionStats?** A section's
"depth" is defined as the depth of its **first** open in the
current frame. If you re-open the same section later at a
different depth (rare — usually a bug), the display depth
stays stable across frames.

**Why "only touched" sections advance the rolling window?**
If a scene doesn't open `Combat` for 30 frames straight
(because no enemies are alive), writing zeros into the
`Combat` ring would drop its rolling avg to near-zero
falsely. The current logic keeps the last-known window
values until the section is opened again.

**Why is the section table fixed size?** Every game-side
`HBE_PROFILE_SCOPE("Name")` should use a string literal —
which has program-lifetime storage. Fresh strings only
appear when a game *adds* a new scope, and the section table
is bounded by `kMaxSections=64`. A game that needs more can
recompile with a larger constant; the API surface doesn't
change.

**Thread safety?** None. The current API is documented as
main-thread-only (Golden rule 5). If a future item adds a
job system, `State` needs to become thread-local (or wrapped
in a spinlock) — that's a follow-up work item, not this one.

Next: `03_application_integration.md`.
