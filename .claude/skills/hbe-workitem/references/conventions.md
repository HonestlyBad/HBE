# HBE code & project conventions

Code you put in a doc gets typed verbatim by the user. It must look
like it was already there.

---

## Indentation — mixed, deliberately unresolved

There is no repo-wide setting. **Match the file you are editing.**

| Style | Files |
|---|---|
| **Tabs** | `HBE.Core/**` (`Application.h/.cpp`, `Profiler.h`, `Layer.h`, `Log.h`, ...), `HBE.Renderer.GL` headers like `Renderer2D.h`, `GameLayer.h` |
| **4 spaces** | Newer / CLion-authored files: `GpuTimer.h/.cpp`, `GLRenderer.cpp`, `MegaX/src/Game/*.cpp`, all `CMakeLists.txt` |

`Application.cpp` is tab-indented, but the item-14 GPU block inside it
was added with spaces. When inserting there, match the immediately
surrounding lines, not the file average.

Never reformat a region you are only inserting into. A doc that
silently retabs 40 lines produces a diff the user can't review.

---

## C++

* **C++20**, `CMAKE_CXX_EXTENSIONS OFF`. `std::filesystem`,
  designated initializers, and `<concepts>` are available.
* `#pragma once` in every header. `GpuTimer.h` additionally carries an
  include guard — that's an outlier, don't propagate it.
* Nested namespace syntax: `namespace HBE::Core::Profiler { }`,
  closed with `} // namespace HBE::Core::Profiler` or a bare `}`
  matching the file.
* Members prefixed `m_`, file-statics `s_`, constants `kCamelCase`
  (`kRollingWindowFrames`, `kMaxSections`).
* Free functions in engine namespaces are `PascalCase`
  (`BeginFrame`, `LogInfo`, `GetTimeSeconds`, `NowNs`); class methods
  are `camelCase` (`beginScene`, `getStats`, `onUpdate`).
* RAII over begin/end pairs for anything a game calls — an early
  return must not leak. Both `Profiler::ScopeTimer` and
  `GpuTimer::Scope` delete copy *and* move.
* Hot-path types take `const char*` (pointer identity, no `strcmp`,
  no allocation) and use fixed-capacity storage, not growing vectors.
* Compile-time feature gates are macros defaulting off in Release,
  and must expand to `((void)0)` when off:

  ```cpp
  #ifndef HBE_FEATURE_ENABLED
      #if defined(NDEBUG)
          #define HBE_FEATURE_ENABLED 0
      #else
          #define HBE_FEATURE_ENABLED 1
      #endif
  #endif
  ```

  Gate on **`NDEBUG`, never `_DEBUG`** — `_DEBUG` is MSVC-only and
  silently compiled the profiler out of Linux Debug builds. That bug
  is why this rule exists.
* Logging: `LogInfo`/`LogWarn`/`LogError` take `std::string_view`.
  For formatted output the codebase uses a `char buf[256]` +
  `std::snprintf` + `LogInfo(buf)` pattern; follow it rather than
  introducing `std::format`.

---

## CMake

Source lists are **explicit** — no globbing. A new `.cpp` must be added
to its target:

| Target | File | Block |
|---|---|---|
| `HBE.Core` | `HBE.Core/CMakeLists.txt` | `add_library(HBE.Core STATIC ...)` |
| `HBE.Platform.SDL` | `HBE.Platform.SDL/CMakeLists.txt` | `add_library(...)` |
| `HBE.Renderer.GL` | `HBE.Renderer.GL/CMakeLists.txt` | `add_library(...)` |
| `MegaX` | `MegaX/CMakeLists.txt` | `add_executable(MegaX ...)` |

Rules:

* Paths in the list are relative to that `CMakeLists.txt`
  (`src/Core/Profiler.cpp`), 4-space indented, one per line.
* Ordered alphabetically **within each subdirectory group**
  (`src/Core/*` then `src/ECS/*` then `src/Input/*`). `MegaX` keeps
  `src/main.cpp` first, then alphabetical.
* **Headers are never listed** — they resolve through
  `target_include_directories`.
* Third-party comes from `cmake/HBEDependencies.cmake` as
  `hbe::glad`, `hbe::glm`, `hbe::nlohmann_json`, `hbe::stb`,
  `hbe::imgui`, `hbe::sdl3`, `hbe::sdl3_mixer`, `hbe::sdl3_ttf`,
  `hbe::opengl`. Link the alias, never a raw path.
* Options at root: `HBE_BUILD_SANDBOX`, `HBE_BUILD_MEGAX`,
  `HBE_BUILD_MAPMAKER`, `HBE_USE_SYSTEM_SDL3`.

**`.vcxproj` / `.filters` / `.slnx` are dead.** `MegaX.vcxproj`,
`HBMapMaker.vcxproj`, and `HonestlyBadEngine.slnx` still sit in the
tree but no longer drive any build. Never instruct an edit to them.

---

## Include order

As seen in `Application.cpp`:

1. the matching header for this `.cpp`
2. other HBE headers, roughly Core → Platform → Input → Renderer
3. third-party (`<SDL3/SDL.h>`, `<glad/glad.h>`)
4. standard library (`<cstdio>`, `<vector>`)

Blank line between groups. New engine headers should keep their
include list minimal — `Profiler.h` deliberately includes only
`<cstdint>`, `<cstddef>`, `<vector>` so it is safe to pull into
`Application.h`, which every layer already sees.

---

## Scope discipline by tag

The tag is not symmetric. `[MEGAX]` is a hard wall; `[HBE]` is a wall
with an expected door.

### `[MEGAX]` — the game only

Edit `MegaX/` and nothing else. **Never touch `HBE.Core/`,
`HBE.Platform.SDL/`, or `HBE.Renderer.GL/`** in a `[MEGAX]` item, not
even a one-line convenience accessor.

If the item genuinely cannot be built without an engine change, stop
and say so. That is a new `[HBE]` item, and it is the user's call
whether to insert it first — not a quiet scope expansion. Say which
engine change is needed and why the game side can't cover it.

### `[HBE]` — the engine, and keep MegaX working

The deliverable is in `HBE.Core/`, `HBE.Platform.SDL/`,
`HBE.Renderer.GL/`. But **you are expected to check MegaX and bring it
up to date**, for two distinct reasons — decide which applies, because
they carry different obligations:

1. **Keeping MegaX working — mandatory.** If the engine change renames,
   re-signatures, deprecates, or changes the behavior of anything MegaX
   uses, MegaX must be updated in the same item or the build breaks.
   This is part of the deliverable, not an optional extra. Grep MegaX
   for every symbol you touched before deciding there's nothing to do.

2. **Verification / demonstration — expected.** Consume the new engine
   feature from real game code to prove it works end to end. This is
   what items 13 and 14 did with the profiler scopes.

Either way the MegaX edits live in **their own doc**, are labelled in
the files-touched table (`verification only`, or `required — API
changed`), and add **no game-specific logic to HBE**. The test for
category 2 is item 13's: strip the MegaX doc out and HBE still builds
and still ships the feature.

### `[HBE/MEGAX]`

Both, as the item text describes. Item 62 is the only one currently
tagged this way.

### Always off-limits

`HBE.Sandbox/` and `HBMapMaker/` — unless the user names them.

The engine must never contain game-specific logic. Item 18 exists
because `InputMap` hardcoded a game's action enum; item 19 exists
because `SceneSerializer` couldn't serialize a game's own components.
Don't reintroduce that shape.

---

## Git

* Branches: `HB/NN-Short-Name` for engine items, `MX/NN-Short-Name`
  for game items — e.g. `HB/24-Packaging-Build-Cleanup`,
  `MX/03-Animated-Tiles`.
* Commits: short imperative summary, no body — "Add frame profiler
  with per-scope timing", "Add enemy shooting, difficulty, and hot
  reload".
* Merged to `main` via PR.
* Never commit or push unless asked.
* Not in git: `build/`, `cmake-build-debug/`, the vendored
  `HBE.Renderer.GL/external/glad/`, `external/nlohmann/json.hpp`,
  `external/stb/*` — see `CACHYOS_SETUP.md` §1.4 for regenerating them.
