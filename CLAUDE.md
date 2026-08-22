# HBE — project context

**Repo root:** `/home/atulo/Projects/HBE` · **Platform:** CachyOS (Arch) ·
**IDE:** CLion · **Build:** CMake + Ninja Multi-Config · **Shell:** fish

HBE ("Honestly Bad Engine") is a **C++20 2D game library**, not a
full engine with an editor UI. The goal is a powerful, lightweight
library that gives a C++ game maker everything they need. It is
developed **in tandem with MegaX**, the game that consumes it.

## The projects

| Project | What it is | Touch it when |
|---|---|---|
| `HBE.Core` | Foundations: `Application`, layers, events, ECS, input map, log, time, profiler | `[HBE]` items |
| `HBE.Platform.SDL` | SDL3 window/GL context, low-level input, audio, graphics settings | `[HBE]` items |
| `HBE.Renderer.GL` | 2D GL renderer: cameras, sprites, tilemaps, particles, text, post-process, UI, GPU timer | `[HBE]` items |
| `MegaX` | **The game.** Roguelike 2D platformer, Mega Man X feel. Primary focus. | `[MEGAX]` items |
| `HBE.Sandbox` | Old engine scratchpad. Superseded by MegaX. | Only if named explicitly |
| `HBMapMaker` | ImGui tilemap editor. Follows engine map format changes. | Only if named explicitly |

Dependency order: `HBE.Core` ← `HBE.Platform.SDL` ← `HBE.Renderer.GL` ←
games. Engine code must never gain a MegaX-specific dependency.

## How work gets done here

**The user implements every code change by hand.** I write numbered
step-by-step markdown docs; the user types the code. I do not edit
`.cpp`/`.h`/`CMakeLists.txt` for a work item unless explicitly asked to.

* Work item backlog: `Docs/WorkItems.txt` (items 0–63, tagged `[HBE]` /
  `[MEGAX]`).
* One directory per item: `Docs/NN-Kebab-Case-Title/` holding
  `00_overview.md`, then implementation docs, then a final
  `NN_build_run_and_verify.md`.
* **Items 0–14 are complete.** Item 15 is next.
* Reference examples of the required doc quality:
  `Docs/13-HBE-CPU-Frame-And-System-Timing/` and
  `Docs/14-HBE-GPU-Timing-And-Renderer-Stats/`.

**`/workitem <N>`** authors a work item's doc set end to end. It also
accepts a free description for work that isn't in `WorkItems.txt`
(`/workitem add an F9 free-camera toggle`), numbering it as the next
unused `Docs/NN-` prefix. The command drives the **`hbe-workitem`
skill**, which carries the doc format, project map, build commands,
conventions, and templates — invoke the skill directly if you get
there without the command.

## Build & run (canonical)

```fish
cmake --preset linux-clang                      # configure (once)
cmake --build --preset linux-clang-debug        # all targets
cmake --build --preset linux-clang-debug --target MegaX
./build/linux-clang/bin/Debug/MegaX
```

`linux-clang-release` for Release. `build/linux-clang/` is the preset
tree; `cmake-build-debug/` is CLion's own tree — both exist, prefer the
preset tree in docs.

## Hard rules

1. **CMake is the build system.** A new `.cpp` must be added to its
   target's source list in that project's `CMakeLists.txt`. Headers need
   no registration. The leftover `MegaX.vcxproj` / `HBMapMaker.vcxproj` /
   `HonestlyBadEngine.slnx` are stale — never reference them.
2. **Respect the item tag — it is not symmetric.**
   `[MEGAX]` = the game only. **Never** touch an `HBE.*` project. If the
   item can't be done without an engine change, stop and say so; that's
   a separate `[HBE]` item and my call to schedule.
   `[HBE]` = the engine, **and keep MegaX working**. Update MegaX in the
   same item when the engine change would otherwise break it (mandatory),
   and consume the new feature from game code to prove it works
   (expected). MegaX edits go in their own doc, labelled in the
   files-touched table, and never push game logic into HBE.
3. **Docs use Linux absolute paths** (`/home/atulo/Projects/HBE/...`) and
   **fish** for shell snippets.
4. **Match the surrounding file's style.** `HBE.Core` and older files are
   tab-indented; newer files (`GpuTimer.h`, `GLRenderer.cpp`, `GameLayer.cpp`)
   use 4 spaces. Never reformat a file you're only inserting into.
5. Git: branch `HB/NN-Short-Name` for engine items, `MX/NN-Short-Name` for
   game items. Commit only when asked.

## API reference

`Docs/GameEngine/ENGINE_API_REFERENCE.md` documents the public engine
surface. Treat it as authoritative, but **verify against the real header**
before writing code into a doc — headers are the source of truth.
