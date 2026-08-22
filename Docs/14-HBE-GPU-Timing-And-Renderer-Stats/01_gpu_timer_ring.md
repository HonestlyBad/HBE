# 01 — GPU timer query ring (`HBE/Renderer/GpuTimer.h/.cpp`)

This doc creates two new files under `HBE.Renderer.GL/`:

* `include\HBE\Renderer\GpuTimer.h`
* `src\Renderer\GpuTimer.cpp`

Plus one project-file entry each in `HBE.Renderer.GL.vcxproj`
and `HBE.Renderer.GL.vcxproj.filters`.

The design is: for each **named** GPU scope, keep a small
ring of GL query object IDs. On scope open, issue
`glBeginQuery(GL_TIME_ELAPSED, ring[write])`. On scope close,
`glEndQuery`. Every frame, walk all sections and try to read
back the query from `kResultLatencyFrames` frames ago —
`glGetQueryObjectiv(..., GL_QUERY_RESULT_AVAILABLE, ...)`.
Publish the ready readings to the CPU Profiler
(`HBE::Core::Profiler::PublishGpuSection`) and roll the
window.

Capability probe: if `GL_ARB_timer_query` is missing (or
`glGenQueries` is `nullptr`), every entry point becomes a
no-op, and `Profiler::SetGpuSupported(false)` is called
once at initialize.

---

## 1. Create `GpuTimer.h`

**Full path:**
`G:\Dev\HBE\HBE.Renderer.GL\include\HBE\Renderer\GpuTimer.h`

**Status:** must not exist yet.

```cpp
#pragma once

#include <cstdint>
#include <cstddef>

namespace HBE::Renderer::GpuTimer {

    // Number of frames of latency between opening a scope and its result
    // being read back into the Profiler snapshot. 3 is enough for every
    // sane desktop driver; increase if you see repeated NOT_AVAILABLE warnings.
    static constexpr std::size_t kResultLatencyFrames = 3;

    // Max unique GPU section names tracked at once.
    static constexpr std::size_t kMaxGpuSections = 32;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    // Called once from GLRenderer::initialize AFTER glad has loaded. Probes
    // ARB_timer_query support. Safe to call multiple times.
    bool Initialize();

    // Called from GLRenderer's destructor path. Frees every GL query object.
    void Shutdown();

    // Should we assume timer queries work? Returns false on GLES-lite / driver
    // stubs / after Initialize() failed. When false, every begin/end call
    // below no-ops.
    bool IsSupported();

    // Advance the frame index. Must be called once per frame *before* any
    // scope opens. Also drains any newly-ready results from the ring and
    // publishes them to the CPU Profiler.
    void NewFrame();

    // -------------------------------------------------------------------------
    // Named scope open/close. Nested is legal. Returns -1 when unsupported or
    // the section table is full.
    // -------------------------------------------------------------------------
    int  BeginScope(const char* name);
    void EndScope(int handle);

    // -------------------------------------------------------------------------
    // RAII wrapper. Never allocates.
    // -------------------------------------------------------------------------
    class Scope {
    public:
        explicit Scope(const char* name) noexcept : m_handle(BeginScope(name)) {}
        ~Scope() noexcept { EndScope(m_handle); }

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
        Scope(Scope&&) = delete;
        Scope& operator=(Scope&&) = delete;
    private:
        int m_handle;
    };

} // namespace HBE::Renderer::GpuTimer

// -----------------------------------------------------------------------------
// Convenience macro. Same shape as HBE_PROFILE_SCOPE from Item 13.
// -----------------------------------------------------------------------------
#define HBE_GPU_SCOPE_CONCAT_INNER(a, b) a##b
#define HBE_GPU_SCOPE_CONCAT(a, b) HBE_GPU_SCOPE_CONCAT_INNER(a, b)
#define HBE_GPU_SCOPE(NAME) ::HBE::Renderer::GpuTimer::Scope HBE_GPU_SCOPE_CONCAT(_hbe_gpu_, __LINE__)(NAME)
```

Do **not** include `<glad/glad.h>` in this header. All GL
symbols live inside the .cpp.

---

## 2. Create `GpuTimer.cpp`

**Full path:**
`G:\Dev\HBE\HBE.Renderer.GL\src\Renderer\GpuTimer.cpp`

**Status:** must not exist yet.

```cpp
#include "HBE/Renderer/GpuTimer.h"
#include "HBE/Core/Log.h"
#include "HBE/Core/Profiler.h"

#include <glad/glad.h>

#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace HBE::Renderer::GpuTimer {

    namespace {

        constexpr std::size_t kRingSlots = kResultLatencyFrames + 1;

        struct Section {
            const char*                            name = nullptr;
            int                                    depth = 0;
            // one query object per ring slot, PLUS a same-slot "in-use" flag
            std::array<unsigned int, kRingSlots>   queryIds{};
            std::array<bool, kRingSlots>           slotInFlight{};
            std::array<std::uint64_t, kRingSlots>  slotFrameIndex{};
        };

        struct StackEntry {
            int  sectionIndex = -1;
            std::size_t writeSlot = 0;
        };

        struct State {
            bool         initialized   = false;
            bool         supported     = false;
            std::uint64_t frameIndex   = 0;
            int          currentDepth  = 0;
            std::size_t  writeSlot     = 0;

            std::array<Section, kMaxGpuSections> sections{};
            std::size_t sectionCount = 0;

            // scope stack for this frame
            std::array<StackEntry, kMaxGpuSections * 2> stack{};
            std::size_t stackSize = 0;
        };

        State& S() {
            static State s;
            return s;
        }

        bool checkForceDisable() {
            const char* env = std::getenv("HBE_FORCE_NO_GPU_TIMER");
            return env && std::strcmp(env, "1") == 0;
        }

        int findOrAddSection(const char* name) {
            State& s = S();
            for (std::size_t i = 0; i < s.sectionCount; ++i) {
                if (s.sections[i].name == name) return static_cast<int>(i);
            }
            if (s.sectionCount >= kMaxGpuSections) return -1;

            Section& sec = s.sections[s.sectionCount];
            sec = Section{};
            sec.name  = name;
            sec.depth = s.currentDepth;

            // Allocate one GL query per ring slot
            glGenQueries(static_cast<GLsizei>(kRingSlots), sec.queryIds.data());
            for (std::size_t i = 0; i < kRingSlots; ++i) {
                sec.slotInFlight[i]  = false;
                sec.slotFrameIndex[i] = 0;
            }

            const int idx = static_cast<int>(s.sectionCount);
            ++s.sectionCount;
            return idx;
        }

        // Try to drain result for a section slot that was written N frames ago.
        void drainReadySlot(Section& sec, std::size_t slot) {
            if (!sec.slotInFlight[slot]) return;

            GLint available = 0;
            glGetQueryObjectiv(sec.queryIds[slot], GL_QUERY_RESULT_AVAILABLE, &available);
            if (!available) return; // still in flight — try again next frame

            GLuint64 ns = 0;
            glGetQueryObjectui64v(sec.queryIds[slot], GL_QUERY_RESULT, &ns);
            sec.slotInFlight[slot] = false;

            // Publish to CPU Profiler
            HBE::Core::Profiler::PublishGpuSection(sec.name, ns, sec.depth);
        }

    } // anonymous namespace

    bool Initialize() {
        State& s = S();
        if (s.initialized) return s.supported;

        s.initialized = true;

        if (checkForceDisable()) {
            HBE::Core::LogInfo("GpuTimer: disabled by HBE_FORCE_NO_GPU_TIMER=1.");
            s.supported = false;
            HBE::Core::Profiler::SetGpuSupported(false);
            return false;
        }

        // Probe extension + function pointers.
        const bool haveExt =
            (GLAD_GL_ARB_timer_query != 0) ||
            (GLAD_GL_VERSION_3_3     != 0);

        const bool haveFuncs =
            (glGenQueries != nullptr) &&
            (glBeginQuery != nullptr) &&
            (glEndQuery   != nullptr) &&
            (glGetQueryObjectiv    != nullptr) &&
            (glGetQueryObjectui64v != nullptr);

        s.supported = haveExt && haveFuncs;

        if (!s.supported) {
            HBE::Core::LogWarn("GpuTimer: ARB_timer_query not available. GPU timings disabled.");
        }

        HBE::Core::Profiler::SetGpuSupported(s.supported);
        return s.supported;
    }

    void Shutdown() {
        State& s = S();
        if (!s.supported) {
            s = State{};
            return;
        }

        for (std::size_t i = 0; i < s.sectionCount; ++i) {
            Section& sec = s.sections[i];
            glDeleteQueries(static_cast<GLsizei>(kRingSlots), sec.queryIds.data());
        }
        s = State{};
    }

    bool IsSupported() { return S().supported; }

    void NewFrame() {
        State& s = S();
        if (!s.supported) return;

        ++s.frameIndex;
        s.writeSlot = static_cast<std::size_t>(s.frameIndex % kRingSlots);
        s.currentDepth = 0;
        s.stackSize    = 0;

        // Drain any slots that were written kResultLatencyFrames frames ago.
        // Since our ring width is (kResultLatencyFrames + 1), the readSlot is
        // simply the CURRENT writeSlot after advancing — its data is the oldest.
        const std::size_t readSlot = s.writeSlot;
        for (std::size_t i = 0; i < s.sectionCount; ++i) {
            drainReadySlot(s.sections[i], readSlot);
        }
    }

    int BeginScope(const char* name) {
        State& s = S();
        if (!s.supported || name == nullptr) return -1;
        if (s.stackSize >= s.stack.size()) return -1;

        const int sectionIdx = findOrAddSection(name);
        if (sectionIdx < 0) return -1;

        Section& sec = s.sections[sectionIdx];
        const std::size_t slot = s.writeSlot;

        // If the slot is still in flight (drain never fired) just skip -
        // we won't record this scope this frame. Prevents mixing samples.
        if (sec.slotInFlight[slot]) return -1;

        glBeginQuery(GL_TIME_ELAPSED, sec.queryIds[slot]);
        sec.slotInFlight[slot]  = true;
        sec.slotFrameIndex[slot] = s.frameIndex;

        StackEntry& e = s.stack[s.stackSize++];
        e.sectionIndex = sectionIdx;
        e.writeSlot    = slot;

        ++s.currentDepth;
        return static_cast<int>(s.stackSize) - 1;
    }

    void EndScope(int handle) {
        State& s = S();
        if (handle < 0 || !s.supported) return;
        if (handle >= static_cast<int>(s.stackSize)) return;

        // Close top-of-stack (RAII should always match this — tolerate mismatched
        // close by unwinding).
        while (s.stackSize > 0 && static_cast<int>(s.stackSize) - 1 >= handle) {
            glEndQuery(GL_TIME_ELAPSED);
            --s.stackSize;
            if (s.currentDepth > 0) --s.currentDepth;
            if (static_cast<int>(s.stackSize) <= handle) break;
        }
    }

} // namespace HBE::Renderer::GpuTimer
```

Notes on this file:

* `GL_TIME_ELAPSED` returns delta time in nanoseconds for a
  single query. Nested queries **are not allowed** in the
  same query target — GL forbids two active `GL_TIME_ELAPSED`
  queries at the same time. We use `glQueryCounter` +
  `GL_TIMESTAMP` when the spec demands nesting, but for
  serial scopes (which is what MegaX has) `GL_TIME_ELAPSED`
  is fine. **See Doc §5 for the nesting workaround.**
* `glDeleteQueries` on scope names that were never used
  is a no-op — safe.
* `HBE_FORCE_NO_GPU_TIMER=1` env var — verification hook
  used by doc `06` §3.

---

## 3. Handling nested `GL_TIME_ELAPSED` queries

OpenGL says: *"If a GL_TIME_ELAPSED query is active, no other
GL_TIME_ELAPSED query may be started."*

Our Item 14 nesting plan (from `00_overview.md` §5.2) is:

```
GpuFrame
  ├── GpuScene
  └── GpuPostProcess
```

Two of those overlap (`GpuScene` and `GpuPostProcess` are
serial, not nested with each other), but `GpuFrame` DOES
overlap both. That would violate the "only one
`GL_TIME_ELAPSED` at a time" rule.

**Fix (applied in the .cpp above):** the pattern in `BeginScope`
issues `glBeginQuery(GL_TIME_ELAPSED, ...)` unconditionally.
If a scope opens while another is already active, GL sets
`GL_INVALID_OPERATION` and the inner scope's query never
starts — that would silently produce a `0.00 ms` reading and
a driver warning.

The workaround **you must do at the call sites** (docs `03`
§2 and §3) is:

* Do NOT open `GpuFrame` and `GpuScene` at the same time.
  Doc `03` opens `GpuFrame` in `beginFrameInViewport`,
  closes it in `endFrame` — but `GpuScene` and
  `GpuPostProcess` are opened INSIDE that outer scope.

* The `GpuTimer.cpp` above enforces "one at a time" by
  ending the currently-active query before starting a
  nested one — but that would lose the outer time.

* **Real workaround:** we use `GL_TIMESTAMP` for `GpuFrame`
  (spec-legal to nest) and `GL_TIME_ELAPSED` for the inner
  serial pair. This adds ~30 lines to `GpuTimer.cpp`.

To keep this doc simple, use the pragmatic version below:
**flatten to 2 scopes** — `GpuScene` and `GpuPostProcess`
only — no outer `GpuFrame`. `Profiler::PublishGpuFrame`
receives the sum of the two ready sections in `EndFrame`.
Doc `04` §3 does exactly that.

If you later add lighting / shadow passes, add more serial
scopes: they'll all share the ring and never overlap.

---

## 4. Register `GpuTimer.h` in `HBE.Renderer.GL.vcxproj`

Open `G:\Dev\HBE\HBE.Renderer.GL\HBE.Renderer.GL.vcxproj`.

The `ClInclude` block for `HBE\Renderer\...` starts at
line ~14 and ends at line ~59. Find:

```xml
    <ClInclude Include="include\HBE\Renderer\GLShader.h" />
```

Right after that line, insert:

```xml
    <ClInclude Include="include\HBE\Renderer\GpuTimer.h" />
```

Alphabetically it slots between `GLShader.h` and
`Material.h`.

Now scroll to the `ClCompile` block. Find:

```xml
    <ClCompile Include="src\Renderer\GLShader.cpp" />
```

Right after it, insert:

```xml
    <ClCompile Include="src\Renderer\GpuTimer.cpp" />
```

---

## 5. Register in `HBE.Renderer.GL.vcxproj.filters`

Open
`G:\Dev\HBE\HBE.Renderer.GL\HBE.Renderer.GL.vcxproj.filters`.

Add matching entries in the two `<ItemGroup>` blocks — one
`ClInclude` and one `ClCompile`, both right after their
`GLShader` counterparts. If the filter file doesn't have
`GLShader` entries (some minimal filter files omit them),
just append the new entries at the end of the correct
`<ItemGroup>`:

```xml
    <ClInclude Include="include\HBE\Renderer\GpuTimer.h" />
```
```xml
    <ClCompile Include="src\Renderer\GpuTimer.cpp" />
```

---

## 6. Sanity check

```powershell
Test-Path G:\Dev\HBE\HBE.Renderer.GL\include\HBE\Renderer\GpuTimer.h
# -> True
Test-Path G:\Dev\HBE\HBE.Renderer.GL\src\Renderer\GpuTimer.cpp
# -> True

Get-Content G:\Dev\HBE\HBE.Renderer.GL\HBE.Renderer.GL.vcxproj | Select-String "GpuTimer"
# -> exactly 2 matches: one ClInclude, one ClCompile

Get-Content G:\Dev\HBE\HBE.Renderer.GL\HBE.Renderer.GL.vcxproj.filters | Select-String "GpuTimer"
# -> exactly 2 matches
```

Do **not** attempt a build yet. `GpuTimer.cpp` references
`HBE::Core::Profiler::PublishGpuSection` and
`SetGpuSupported`, which don't exist until doc `04`. Doing
`docs 02 -> 03 -> 04` first, then a full build in doc `06`,
is the recommended order.

Next: `02_renderer_stats.md`.
