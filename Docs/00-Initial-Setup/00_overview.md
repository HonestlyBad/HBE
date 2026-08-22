# Item 00 — Initial Setup (MegaX)

**Goal:** stand up a brand-new game project, **MegaX**, inside the existing
`HonestlyBadEngine.slnx` solution, wired to the HBE engine *as if the engine
were a downloaded package*. When you're done, pressing **F5** opens a blank
(cleared) window titled **MegaX**, and **F11** toggles borderless fullscreen.

> **Done when:** a blank window opens and `F11` fullscreen toggle works.
> (WorkItems.txt, item 0.)

---

## The golden rules for this project

1. **Same solution, new project.** MegaX is a new `.vcxproj` added to
   `G:\Dev\HBE\HonestlyBadEngine.slnx`, living in `G:\Dev\HBE\MegaX\`.
2. **Engine = package.** MegaX consumes the engine only through its **public
   headers** (`HBE.Core\include`, `HBE.Platform.SDL\include`,
   `HBE.Renderer.GL\include`) and its **built static libs**
   (`HBE.Core.lib`, `HBE.Platform.SDL.lib`, `HBE.Renderer.GL.lib`). Treat it
   like a third-party SDK you downloaded.
3. **NEVER reference the Sandbox.** MegaX must not `#include` any
   `HBE.Sandbox` header, link its code, or add a project reference to it. The
   Sandbox is a *sibling consumer* of the engine, not part of it.
4. **All new work goes in MegaX.** From here on, unless a task explicitly says
   otherwise, you only add/modify files under `G:\Dev\HBE\MegaX\`. The engine
   (`HBE.Core`, `HBE.Platform.SDL`, `HBE.Renderer.GL`) stays untouched so it
   can keep evolving independently. If you *ever* find you must change the
   engine, stop and call it out explicitly — it's a separate decision.

---

## What you get "for free" from the engine

You do **not** need to write any of this yourself for item 00 — it lives in
the engine and works the moment you link it:

| Capability | Where it lives |
|---|---|
| Window + OpenGL context + SDL event pump | `HBE::Core::Application` + `HBE::Platform::SDLPlatform` |
| Per-frame clear of the whole window (black) | `Application::run()` → `GLRenderer::beginFrameFullWindow` |
| Letterboxed logical viewport + resize handling | `Application::recalcViewport()` |
| **F11 fullscreen toggle (global)** | `Application::handleSDLEvent()` → `SDL_SCANCODE_F11` → `toggleFullscreen()` |
| Window-close / quit handling | `Application::run()` event loop |
| Asset root resolution | `HBE::Core::AssetPaths` |
| Audio init (SDL3_mixer) | `HBE::Platform::Audio` |

Because the engine clears the window every frame and handles F11 itself, a
game **Layer** that does *nothing* already satisfies item 00's success
criteria. Everything else (player, tiles, etc.) comes in later items.

---

## The 5 steps (read in order)

1. **`01_create_project_and_configs.md`** — create the MegaX project in the
   solution and set its compiler/linker configuration (platforms, toolset,
   C++20, isolated output folder).
2. **`02_wire_engine_references.md`** — point MegaX at the engine: project
   references, include dirs, library dirs, dependencies, the runtime
   DLL/asset copy step, and the debugger working directory. Includes a
   complete reference `MegaX.vcxproj` to diff against.
3. **`03_folder_structure_and_assets.md`** — create the game's `include/`,
   `src/`, and `assets/` tree and seed the engine's stock shaders so the
   asset system initializes.
4. **`04_minimal_game_code.md`** — the tiny `main.cpp` + `GameLayer` that open
   the blank window.
5. **`05_build_run_and_verify.md`** — build, set MegaX as the startup project,
   run, and walk the verification checklist (+ troubleshooting).

---

## Target layout (what you'll have created)

```
G:\Dev\HBE\
├── HonestlyBadEngine.slnx            (MegaX added here)
├── HBE.Core\            }
├── HBE.Platform.SDL\    }  the engine — DO NOT MODIFY
├── HBE.Renderer.GL\     }
├── HBE.Sandbox\            (ignore it; never reference it)
└── MegaX\                  <-- NEW: the game
    ├── MegaX.vcxproj
    ├── include\
    │   └── Game\
    │       └── GameLayer.h
    ├── src\
    │   ├── main.cpp
    │   └── Game\
    │       └── GameLayer.cpp
    ├── assets\
    │   ├── shaders\        (seeded from the engine's stock shaders)
    │   ├── sprites\
    │   ├── tilesets\
    │   ├── maps\
    │   ├── scenes\
    │   ├── prefabs\
    │   ├── audio\
    │   ├── fonts\
    │   └── ui\
    ├── bin\   (build output — git-ignored)
    └── obj\   (intermediate — git-ignored)
```

Now open **`01_create_project_and_configs.md`**.
