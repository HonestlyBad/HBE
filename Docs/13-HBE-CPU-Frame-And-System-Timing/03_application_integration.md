# 03 — Application integration

This doc wires the profiler into `HBE::Core::Application`:

1. Include `Profiler.h` in `Application.h`.
2. Bracket the main loop body with `BeginFrame` / `EndFrame`.
3. Wrap the layer `onUpdate` loop in a
   `HBE_PROFILE_SCOPE("ApplicationUpdate")`.
4. Wrap `m_audio.update(dt)` in a `HBE_PROFILE_SCOPE("Audio")`.
5. Add an **optional** 1-Hz `LogInfo` dump behind a
   `#define HBE_PROFILER_LOG_ONCE_PER_SECOND 1` toggle.

All edits are in **two** files:

* `G:\Dev\HBE\HBE.Core\include\HBE\Core\Application.h`
* `G:\Dev\HBE\HBE.Core\src\Core\Application.cpp`

No new files. No changes to `HBE.Platform.SDL`,
`HBE.Renderer.GL`, Sandbox, or MapMaker.

---

## 1. `Application.h` — add the include

Open `G:\Dev\HBE\HBE.Core\include\HBE\Core\Application.h`.

The include block at the top currently reads (lines 3-10):

```cpp
#include "HBE/Core/LayerStack.h"
#include "HBE/Core/AssetPaths.h"

#include "HBE/Platform/SDLPlatform.h"
#include "HBE/Platform/Audio.h"
#include "HBE/Renderer/GLRenderer.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/ResourceCache.h"
```

Insert a new line **right after** the
`#include "HBE/Core/LayerStack.h"` line (line 3) and
**before** `#include "HBE/Core/AssetPaths.h"` (line 4):

```cpp
#include "HBE/Core/Profiler.h"
```

Final include block should read:

```cpp
#include "HBE/Core/LayerStack.h"
#include "HBE/Core/Profiler.h"
#include "HBE/Core/AssetPaths.h"

#include "HBE/Platform/SDLPlatform.h"
#include "HBE/Platform/Audio.h"
#include "HBE/Renderer/GLRenderer.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/ResourceCache.h"
```

Nothing else in `Application.h` changes.

> **WHY HERE?** Every layer (including MegaX's `GameLayer`)
> already includes `Application.h`. Adding `Profiler.h` to
> the `Application.h` include chain means game code gets the
> `HBE_PROFILE_SCOPE` macro for free without hunting down a
> new include.

---

## 2. `Application.cpp` — main loop edits

Open `G:\Dev\HBE\HBE.Core\src\Core\Application.cpp`.

### 2.1  Add nothing new to the top of the file

`Profiler.h` is already reachable through `Application.h`.
Do **not** add `#include "HBE/Core/Profiler.h"` here.

### 2.2  Replace the body of `Application::run()`

The current `Application::run` body runs from **line 310** to
**line 369**. The relevant loop body is lines **320-366**.

We are going to change **four** locations inside this loop:

1. Add `HBE::Core::Profiler::BeginFrame();` at the top of the
   loop body.
2. Wrap the layer `onUpdate` for-loop in a
   `HBE_PROFILE_SCOPE("ApplicationUpdate")`.
3. Wrap `m_audio.update(dt)` in a
   `HBE_PROFILE_SCOPE("Audio")`.
4. Add `HBE::Core::Profiler::EndFrame();` right before
   `m_platform.delayMillis(1);` at the bottom of the loop.

**Do NOT** wrap the render for-loop in a
`HBE_PROFILE_SCOPE("Render")`. Render section scopes are the
game's responsibility (doc `04`) so an engine game that
doesn't use `GameLayer` still controls its own render
naming.

Below is the exact patched loop. Replace the loop body
(lines **320-366** inclusive) with the following:

```cpp
		while (m_running) {
			HBE::Core::Profiler::BeginFrame();

			HBE::Platform::Input::NewFrame();

			// mapping layer needs per-frame update too (edge detection for axis-threshold)
			HBE::Input::NewFrame();

			// Pump SDL events through platform
			bool quit = m_platform.pumpEvents([this](const SDL_Event& e) {
				this->handleSDLEvent(e);
				});

			if (quit) {
				m_running = false;
				HBE::Core::Profiler::EndFrame();
				break;
			}

			// dt
			double now = GetTimeSeconds();
			float dt = static_cast<float>(now - prevTime);
			if (dt < 0.0f) dt = 0.0f;
			if (dt > 0.25f) dt = 0.25f;
			prevTime = now;

			// update
			{
				HBE_PROFILE_SCOPE("ApplicationUpdate");
				for (std::size_t i = 0; i < m_layers.m_layers.size(); ++i) {
					auto& layer = m_layers.m_layers[i];
					if (layer) layer->onUpdate(dt);
				}
			}

			// keep audio spatialization / finished-track cleanup fresh
			{
				HBE_PROFILE_SCOPE("Audio");
				m_audio.update(dt);
			}

			// 1) Clear the whole window (black bars)
			m_gl.beginFrameFullWindow(m_winW, m_winH);

			// 2) Render scene only inside the letterboxed viewport
			m_gl.beginFrameInViewport(m_vpX, m_vpY, m_vpW, m_vpH);

			for (std::size_t i = 0; i < m_layers.m_layers.size(); ++i) {
				auto& layer = m_layers.m_layers[i];
				if (layer) layer->onRender();
			}

			m_gl.endFrame(m_platform);

			HBE::Core::Profiler::EndFrame();

#if defined(HBE_PROFILER_LOG_ONCE_PER_SECOND) && HBE_PROFILER_LOG_ONCE_PER_SECOND
			{
				static double s_lastLog = 0.0;
				const double nowS = GetTimeSeconds();
				if (nowS - s_lastLog >= 1.0) {
					s_lastLog = nowS;
					const auto& snap = HBE::Core::Profiler::GetSnapshot();
					char buf[256];
					std::snprintf(buf, sizeof(buf),
						"[Profiler] Frame=%.2fms (avg %.2f min %.2f max %.2f)",
						snap.frameMs, snap.frameAvgMs, snap.frameMinMs, snap.frameMaxMs);
					LogInfo(buf);
					for (const auto& s : snap.sections) {
						char line[256];
						const char* pad = "                    "; // 20 spaces
						int spaces = s.depth * 2;
						if (spaces > 20) spaces = 20;
						std::snprintf(line, sizeof(line),
							"  %.*s%-18s cur=%6.2f avg=%6.2f min=%6.2f max=%6.2f (n=%zu)",
							spaces, pad,
							s.name ? s.name : "?",
							s.currentMs, s.avgMs, s.minMs, s.maxMs,
							s.sampleCount);
						LogInfo(line);
					}
				}
			}
#endif

			m_platform.delayMillis(1);
		}
```

That's the entire `while` block. The lines outside the
`while` (the `if (!m_initialized)` guard, the `m_running = true;`,
`prevTime` initialization, and the final
`LogInfo("Application exiting run loop.");`) are unchanged.

### 2.3  Add the `#include <cstdio>` if not present

The optional log block above uses `std::snprintf`. If the
top of `Application.cpp` doesn't already include `<cstdio>`,
add it. On the current codebase (as of Item 12) the includes
are:

```cpp
#include "HBE/Core/Application.h"

#include "HBE/Core/Log.h"
#include "HBE/Core/Time.h"
#include "HBE/Core/Event.h"
#include "HBE/Platform/Input.h"
#include "HBE/Input/InputMap.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_scancode.h>
```

Add **right after** `#include <SDL3/SDL_scancode.h>` (line
10 in the current file):

```cpp
#include <cstdio>
```

Final include block:

```cpp
#include "HBE/Core/Application.h"

#include "HBE/Core/Log.h"
#include "HBE/Core/Time.h"
#include "HBE/Core/Event.h"
#include "HBE/Platform/Input.h"
#include "HBE/Input/InputMap.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_scancode.h>
#include <cstdio>
```

---

## 3. Turn on the 1-Hz log for verification

The 1-Hz `LogInfo` block is guarded by
`HBE_PROFILER_LOG_ONCE_PER_SECOND`, which defaults to
**undefined**. For Item 13 verification we want it on in the
Debug config **only**.

There are two ways to enable it. **Pick one.** Both are
supported.

### Option A (recommended) — Per-configuration preprocessor define

Open `G:\Dev\HBE\HBE.Core\HBE.Core.vcxproj` in a text editor
(or in Visual Studio: right-click **HBE.Core** ->
Properties -> C/C++ -> Preprocessor -> Preprocessor
Definitions, for the Debug|x64 configuration).

In the vcxproj XML, find the `<ItemDefinitionGroup>` block
that begins with:

```xml
  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
```

Inside its `<ClCompile>` element, find
`<PreprocessorDefinitions>` and prepend
`HBE_PROFILER_LOG_ONCE_PER_SECOND=1;` to the existing value.
Example before:

```xml
      <PreprocessorDefinitions>_DEBUG;%(PreprocessorDefinitions)</PreprocessorDefinitions>
```

After:

```xml
      <PreprocessorDefinitions>HBE_PROFILER_LOG_ONCE_PER_SECOND=1;_DEBUG;%(PreprocessorDefinitions)</PreprocessorDefinitions>
```

Do **not** add this define to the `Release|x64` group. The
Release build should stay silent.

### Option B — Local `#define` at the top of `Application.cpp`

Only if you're just eyeballing behavior locally and don't
want to touch the vcxproj:

At the top of `Application.cpp`, right below the include
block, add:

```cpp
#ifndef HBE_PROFILER_LOG_ONCE_PER_SECOND
    #ifdef _DEBUG
        #define HBE_PROFILER_LOG_ONCE_PER_SECOND 1
    #else
        #define HBE_PROFILER_LOG_ONCE_PER_SECOND 0
    #endif
#endif
```

This produces the same behavior as Option A (on in Debug,
off in Release) without touching any project files. If
you later want a Debug build **without** the console spam,
you'll need to comment this block out. Option A is cleaner.

---

## 4. What NOT to touch

* Do **not** add a scope inside `handleSDLEvent`. Input
  events are tiny and adding a scope per event floods the
  section table (the `pumpEvents` callback might fire 10-100
  times per frame).
* Do **not** wrap `m_gl.beginFrameFullWindow`,
  `beginFrameInViewport`, or `endFrame` yet. GPU work is
  Item 14 (`GpuFrame`), and CPU-side render orchestration
  time is what the MegaX game code (doc `04`) exposes as
  `TileRendering` and `SpriteRendering`.
* Do **not** call `Profiler::Reset()` from `Application`.
  The scene-reload path in `GameLayer` (Item 11) will call
  `Reset` if it wants to; base engine keeps the rolling
  window rolling across scene boundaries.
* Do **not** add `HBE_PROFILE_SCOPE` around
  `HBE::Platform::Input::NewFrame()` or
  `HBE::Input::NewFrame()`. Their combined cost is
  sub-microsecond and adding a scope there is pure noise.
* Do **not** thread anything through `WindowResizeEvent` or
  `KeyPressedEvent`. Item 13 is main-thread only.

---

## 5. Sanity check before doc 04

```powershell
Get-Content G:\Dev\HBE\HBE.Core\include\HBE\Core\Application.h | Select-String "Profiler"
# -> 1 match: #include "HBE/Core/Profiler.h"

Get-Content G:\Dev\HBE\HBE.Core\src\Core\Application.cpp | Select-String "Profiler|HBE_PROFILE_"
# -> 4+ matches: BeginFrame, EndFrame, 2 x HBE_PROFILE_SCOPE, optional LogInfo block
```

Try a compile of `HBE.Core` alone (right-click **HBE.Core**
in Solution Explorer -> **Build**). Expected:

* Debug|x64: builds. Zero warnings from `Profiler.h/.cpp`.
* Release|x64: builds. The macros expand to `((void)0)` so
  the two scope blocks become empty braces — cleanly
  optimized out.

If the compile fails with "identifier
`HBE_PROFILE_SCOPE` undeclared", you forgot to include
`Profiler.h` in `Application.h` (step 1).

If the compile fails with an unresolved external for
`HBE::Core::Profiler::BeginFrame`, you forgot to add
`Profiler.cpp` to the vcxproj (doc `02` §2).

Next: `04_megax_instrumentation.md` — MegaX verification pass
(only file touched outside HBE.Core).
