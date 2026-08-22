# Build, run, verify — CachyOS

Everything here is verified against `CMakePresets.json` and
`CACHYOS_SETUP.md`. Use these exact commands in docs.

> `CACHYOS_SETUP.md` still says the clone lives at `~/Dev/HBE`. The real
> working copy is `/home/atulo/Projects/HBE`. Use the real path.

---

## Build

```fish
cd /home/atulo/Projects/HBE

cmake --preset linux-clang                        # configure, once per clone
cmake --build --preset linux-clang-debug          # everything, Debug
cmake --build --preset linux-clang-debug --target MegaX
cmake --build --preset linux-clang-release        # Release
```

Presets: `linux-clang` (default) and `linux-gcc` — swap the prefix in
every command to use gcc. `windows-msvc` exists but is inert on Linux.

Generator is **Ninja Multi-Config**, so configuration is chosen at
build time, not configure time. Binary dir: `build/<presetName>/`.

Clean rebuild:

```fish
rm -rf build/linux-clang
cmake --preset linux-clang
cmake --build --preset linux-clang-debug
```

### Two build trees exist

* `build/linux-clang/` — the preset tree. **Use this in docs.**
* `cmake-build-debug/` — CLion's own tree, single-config, output at
  `cmake-build-debug/bin/MegaX`. Mention it only when the user is
  running from inside CLion.

---

## Run

```fish
./build/linux-clang/bin/Debug/MegaX
./build/linux-clang/bin/Debug/HBE.Sandbox
./build/linux-clang/bin/Debug/HBMapMaker

./build/linux-clang/bin/Release/MegaX
```

Assets are copied next to the executable by `hbe_copy_runtime_deps` as
a post-build step, so the default working directory is correct. To run
against live assets without rebuilding:

```fish
set -x HBE_ASSET_ROOT /home/atulo/Projects/HBE/MegaX/assets
```

In CLion: **Edit Configurations → Environment variables**.

---

## MegaX developer hotkeys

Handled in `GameLayer::onUpdate`, `MegaX/src/Game/GameLayer.cpp`
around lines 128–171. Verification steps should drive these.

| Key | Effect |
|---|---|
| `F1` / `F2` / `F3` | difficulty → Casual / Difficult / Challenging (refills HP) |
| `F5` | full scene reload, including the map |
| `F6` | sprite shader hot reload |
| `F7` | soft respawn (reload without re-reading the map) |
| `F8` | one-shot profiler snapshot dump to the log |
| `R` | refill HP |
| `F11` | fullscreen toggle (engine-level) |
| (see `onUpdate`) | hit/hurt box overlay toggle |

Gameplay: `A`/`D` move, `SPACE` jump, plus shoot/crouch — confirm in
`Player.cpp` before writing them into a checklist.

---

## Profiler output

Debug builds print a 1-Hz block from `Application::run`, gated by
`HBE_PROFILER_LOG_ONCE_PER_SECOND` (defaults on when `NDEBUG` is
absent). Shape:

```
[INFO ] [Profiler] Frame=16.42ms (avg 16.51 min 15.98 max 18.20)
[INFO ]   ApplicationUpdate  cur=  8.31 avg=  8.42 ... (n=120)
[INFO ]     SceneUpdate      cur=  8.10 ...
[INFO ]       Physics        cur=  1.20 ...
[INFO ] [Profiler] GPU=2.10ms (avg 2.15 min 1.90 max 2.60)
[INFO ]   GpuFrame           cur=  2.10 ...
[INFO ] [Profiler] draws=42 passes=3 subQuads=980 rendQuads=910 culled=70
[INFO ] [Profiler] matChg=6 texChg=9 tileChunks=12 ppPasses=0
[INFO ] [Profiler] lights=0 shadowLights=0 liveParticles=134
```

On hardware without `ARB_timer_query` the GPU line reads
`[Profiler] GPU=unsupported (no ARB_timer_query)` — that is a pass, not
a failure.

Rolling window is 120 frames (`kRollingWindowFrames`), ~2s at 60 FPS,
so averages need ~2 seconds of uninterrupted runtime to be meaningful.
Alt-tab stalls poison `maxMs` until the ring refills — worth saying in
a troubleshooting matrix.

---

## Compiler diagnostics to expect

Builds use clang (or gcc) with `-Wall -Wextra -Wno-unused-parameter
-Wno-missing-field-initializers`, C++20. Write **clang** diagnostics in
troubleshooting sections, not MSVC codes:

| Situation | Diagnostic |
|---|---|
| header not found / not saved | ``fatal error: 'HBE/Core/Foo.h' file not found`` |
| `.cpp` missing from CMake | ``undefined reference to 'HBE::Core::Foo::Bar()'`` (link step) |
| missing include for a used symbol | ``error: no member named 'Bar' in namespace 'HBE::Core'`` |
| unused RAII var in a Release build | ``warning: unused variable '_hbe_prof_123' [-Wunused-variable]`` |
| CMake didn't re-configure | ``ninja: error: '<file>.cpp', needed by ..., missing`` |

After editing any `CMakeLists.txt`, the build re-runs CMake
automatically — but if the user hand-edits while a build is running,
`cmake --preset linux-clang` again is the fix.

---

## Verification patterns for docs

Registration landed exactly once:

```fish
grep -c "Profiler.cpp" /home/atulo/Projects/HBE/HBE.Core/CMakeLists.txt
# -> 1
```

A symbol is wired where expected:

```fish
grep -n "HBE_PROFILE_SCOPE" /home/atulo/Projects/HBE/MegaX/src/Game/GameLayer.cpp
# -> exactly 7 matches
```

A file exists:

```fish
test -f /home/atulo/Projects/HBE/HBE.Core/src/Core/Profiler.cpp; echo $status
# -> 0
```

Prefer `grep -c` with an exact expected count over "should appear" —
a count is checkable, a description is not.
