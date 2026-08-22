# 01 — Profiler API header (`HBE/Core/Profiler.h`)

This doc creates the **new** file
`G:\Dev\HBE\HBE.Core\include\HBE\Core\Profiler.h`. It has no
runtime dependencies — only `<cstdint>`, `<cstddef>`, and
`<vector>` from the STL. It is safe to include from
`Application.h` because it introduces no heavy templates.

---

## 1. Create the file

**Full path:** `G:\Dev\HBE\HBE.Core\include\HBE\Core\Profiler.h`

**Status:** file must not exist yet. If it does exist, stop
and check — someone else likely started this work item.

Paste the following content **verbatim** (no reformatting):

```cpp
#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

// -----------------------------------------------------------------------------
// Compile-time toggle.
//
// Default policy:
//   * Debug builds (_DEBUG defined by MSVC)  -> enabled
//   * Release builds (NDEBUG defined)        -> disabled
//
// A game can override this by defining HBE_PROFILE_ENABLED before including
// this header (e.g. via /D in a per-project setting), including forcing it on
// in a Release build for perf-capture purposes.
// -----------------------------------------------------------------------------
#ifndef HBE_PROFILE_ENABLED
    #if defined(_DEBUG) && !defined(NDEBUG)
        #define HBE_PROFILE_ENABLED 1
    #else
        #define HBE_PROFILE_ENABLED 0
    #endif
#endif

namespace HBE::Core::Profiler {

    // Rolling window width, in frames. ~2 seconds at 60 FPS.
    static constexpr std::size_t kRollingWindowFrames = 120;

    // Max unique named sections tracked at once. Bumps the fixed storage.
    static constexpr std::size_t kMaxSections = 64;

    // Max scope opens per frame across all sections. Guards against runaway
    // recursion in a single frame (a section can be opened multiple times
    // per frame — each open is one sample).
    static constexpr std::size_t kMaxScopesPerFrame = 512;

    // Public per-section timing values published every EndFrame().
    struct Sample {
        const char*   name         = nullptr;   // pointer identity — no strcmp
        int           depth        = 0;         // nesting depth when opened
        double        currentMs    = 0.0;       // sum of all opens this frame
        double        avgMs        = 0.0;       // rolling window
        double        minMs        = 0.0;       // rolling window
        double        maxMs        = 0.0;       // rolling window
        std::size_t   sampleCount  = 0;         // valid samples in window (<= kRollingWindowFrames)
        std::size_t   opensThisFrame = 0;       // how many times the scope was opened this frame
    };

    // Full frame snapshot. GetSnapshot() returns a reference to internal
    // storage — copy it if you need to keep it past the next EndFrame().
    struct Snapshot {
        double                frameMs      = 0.0;   // wall time between BeginFrame/EndFrame
        double                frameAvgMs   = 0.0;   // rolling avg of frameMs
        double                frameMinMs   = 0.0;
        double                frameMaxMs   = 0.0;
        std::size_t           frameSampleCount = 0;
        std::uint64_t         frameIndex   = 0;     // monotonically increasing
        std::vector<Sample>   sections;             // stable insertion order
    };

    // -------------------------------------------------------------------------
    // Lifecycle. Both are safe to call when disabled — they no-op cheaply.
    // -------------------------------------------------------------------------
    void BeginFrame();
    void EndFrame();

    // Runtime enable/disable. Independent of HBE_PROFILE_ENABLED — but when
    // HBE_PROFILE_ENABLED is 0, SetEnabled(true) still cannot re-enable
    // instrumentation because HBE_PROFILE_SCOPE has already expanded to
    // nothing at compile time. Use this to *disable* at runtime in a debug
    // build without a rebuild.
    void SetEnabled(bool on);
    bool IsEnabled();

    // Reset all sections, snapshot, and rolling rings. Useful after a scene
    // reload (F5 in MegaX) so old timings don't skew the rolling window.
    void Reset();

    // Read-only access to the last completed frame's snapshot. Do not cache
    // the returned reference past the next EndFrame() — internal storage is
    // reused.
    const Snapshot& GetSnapshot();

    // Nanosecond clock exposed for tests / direct use. Not required by the
    // macro path.
    std::uint64_t NowNs();

    // -------------------------------------------------------------------------
    // Internal scope open/close (used by the RAII wrapper below). Return an
    // opaque index that must be passed back to EndScope. Returns -1 when the
    // profiler is disabled or capacity is exceeded — EndScope tolerates -1.
    // -------------------------------------------------------------------------
    int  BeginScope(const char* name);
    void EndScope(int index);

    // -------------------------------------------------------------------------
    // RAII scope. Never allocates. Copyable/moveable operations deleted so
    // accidental copies can't produce a second close of the same open.
    // -------------------------------------------------------------------------
    class ScopeTimer {
    public:
        explicit ScopeTimer(const char* name) noexcept
            : m_index(BeginScope(name)) {}
        ~ScopeTimer() noexcept { EndScope(m_index); }

        ScopeTimer(const ScopeTimer&)            = delete;
        ScopeTimer& operator=(const ScopeTimer&) = delete;
        ScopeTimer(ScopeTimer&&)                 = delete;
        ScopeTimer& operator=(ScopeTimer&&)      = delete;

    private:
        int m_index;
    };

} // namespace HBE::Core::Profiler

// -----------------------------------------------------------------------------
// Public macros.
//
// HBE_PROFILE_SCOPE("Name")
//   RAII-scoped timer that starts at the point of declaration and closes at
//   the end of the enclosing scope.
//
// HBE_PROFILE_FRAME()
//   Convenience alias for BeginFrame() and EndFrame() when a game wants to
//   bracket its own loop. HBE::Core::Application already calls these — a
//   normal game does not need to.
// -----------------------------------------------------------------------------
#define HBE_PROFILE_CONCAT_INNER(a, b) a##b
#define HBE_PROFILE_CONCAT(a, b) HBE_PROFILE_CONCAT_INNER(a, b)

#if HBE_PROFILE_ENABLED
    #define HBE_PROFILE_SCOPE(NAME) \
        ::HBE::Core::Profiler::ScopeTimer HBE_PROFILE_CONCAT(_hbe_prof_, __LINE__)(NAME)

    #define HBE_PROFILE_BEGIN_FRAME() ::HBE::Core::Profiler::BeginFrame()
    #define HBE_PROFILE_END_FRAME()   ::HBE::Core::Profiler::EndFrame()
#else
    #define HBE_PROFILE_SCOPE(NAME)  ((void)0)
    #define HBE_PROFILE_BEGIN_FRAME() ((void)0)
    #define HBE_PROFILE_END_FRAME()   ((void)0)
#endif
```

> **PASTE NOTE.** In the `#if HBE_PROFILE_ENABLED` block, the
> two-line macro `HBE_PROFILE_SCOPE(NAME)` uses a real
> line-continuation backslash `\` at the **end** of the first
> line, with the second line starting at column 8. If your
> editor collapses the two lines into one, delete the `\` too
> — a `\` in the middle of a single line is a hard compile
> error (`character 'U+5c' is not permitted here`). Either
> form is legal:
>
> ```cpp
> // two-line form (preferred, matches file)
> #define HBE_PROFILE_SCOPE(NAME) \
>     ::HBE::Core::Profiler::ScopeTimer HBE_PROFILE_CONCAT(_hbe_prof_, __LINE__)(NAME)
>
> // one-line form (also fine, no backslash)
> #define HBE_PROFILE_SCOPE(NAME) ::HBE::Core::Profiler::ScopeTimer HBE_PROFILE_CONCAT(_hbe_prof_, __LINE__)(NAME)
> ```

Do **not** add any other includes to this header. Any
platform-specific timing lives in `Profiler.cpp` (doc `02`).

---

## 2. Register the file in the `HBE.Core.vcxproj`

Open `G:\Dev\HBE\HBE.Core\HBE.Core.vcxproj`.

Find the `<ItemGroup>` block that lists `ClInclude` entries.
On the current codebase this block begins on **line 13** (the
line reading `<ItemGroup>` right after the
`ProjectConfigurations` block) and the first entry inside it
is at **line 14** (`ClInclude Include="include\HBE\Core\AssetPaths.h"`).

Add a new `ClInclude` entry, **alphabetically ordered inside
the `HBE\Core\` block**, right after the line:

```xml
    <ClInclude Include="include\HBE\Core\Log.h" />
```

and before the line:

```xml
    <ClInclude Include="include\HBE\Core\Time.h" />
```

The new entry:

```xml
    <ClInclude Include="include\HBE\Core\Profiler.h" />
```

So the final subsection should read (in this exact order):

```xml
    <ClInclude Include="include\HBE\Core\Log.h" />
    <ClInclude Include="include\HBE\Core\Profiler.h" />
    <ClInclude Include="include\HBE\Core\Time.h" />
```

---

## 3. Register the file in `HBE.Core.vcxproj.filters`

Open `G:\Dev\HBE\HBE.Core\HBE.Core.vcxproj.filters`.

The first `<ItemGroup>` contains all `ClInclude` entries.
Right after the line:

```xml
    <ClInclude Include="include\HBE\Core\Log.h" />
```

(currently line 5 in the file), insert:

```xml
    <ClInclude Include="include\HBE\Core\Profiler.h" />
```

So the top of the file should now read:

```xml
<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup>
    <ClInclude Include="include\HBE\Core\Types.h" />
    <ClInclude Include="include\HBE\Core\Log.h" />
    <ClInclude Include="include\HBE\Core\Profiler.h" />
    <ClInclude Include="include\HBE\Core\Time.h" />
    ...
```

(The `Profiler.cpp` registration comes in doc `02`.)

---

## 4. Sanity check before moving on

From a PowerShell prompt:

```powershell
Test-Path G:\Dev\HBE\HBE.Core\include\HBE\Core\Profiler.h
# -> True

Get-Content G:\Dev\HBE\HBE.Core\HBE.Core.vcxproj | Select-String "Profiler.h"
# -> exactly ONE match, in an <ItemGroup> of ClInclude entries

Get-Content G:\Dev\HBE\HBE.Core\HBE.Core.vcxproj.filters | Select-String "Profiler.h"
# -> exactly ONE match
```

Do not attempt to build yet — `Profiler.h` references
`BeginScope / EndScope / BeginFrame / EndFrame / NowNs` which
are not implemented until doc `02`.

Next: `02_profiler_impl.md`.
