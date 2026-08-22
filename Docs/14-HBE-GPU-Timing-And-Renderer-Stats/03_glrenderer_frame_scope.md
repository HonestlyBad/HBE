# 03 — GLRenderer frame scope + Application wiring

Item 14's GPU timing hooks in at three call sites, all in
`HBE.Renderer.GL/`:

* `GLRenderer::initialize` — one call to
  `HBE::Renderer::GpuTimer::Initialize()`
* `GLRenderer::beginFrameInViewport` — open `GpuScene` scope
* `GLRenderer::endFrame` — close `GpuScene` before
  `PostProcessStack::present`, then wrap the present in a
  `GpuPostProcess` scope
* `~GLRenderer` (implicit — no dtor now but we'll add
  `Shutdown()` at process teardown via `Application`).

The `NewFrame()` call that advances the ring lives in
`Application::run` (§4 below) because it must happen once
per frame regardless of which renderer subsystem is active.

---

## 1. `GLRenderer.h` — no new members, one new include

Open `G:\Dev\HBE\HBE.Renderer.GL\include\HBE\Renderer\GLRenderer.h`.

**No** member changes. `GpuTimer` state is a `static State&`
inside `GpuTimer.cpp`, so nothing to add to the class.

---

## 2. `GLRenderer.cpp` — add includes + wire scopes

Open `G:\Dev\HBE\HBE.Renderer.GL\src\Renderer\GLRenderer.cpp`.

### 2.1  Add the include

The current include block (lines 1-16) reads:

```cpp
#include "HBE/Renderer/GLRenderer.h"
#include "HBE/Renderer/GLShader.h"
#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Transform2D.h"
#include "HBE/Renderer/Camera2D.h"
#include "HBE/Renderer/Mesh.h"
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/PostProcessStack.h"

#include "HBE/Core/Log.h"
#include "HBE/Core/Time.h"
#include "HBE/Platform/SDLPlatform.h"

#include <glad/glad.h>
#include <SDL3/SDL.h>
#include <cmath>
```

Add **right after** the `PostProcessStack.h` include:

```cpp
#include "HBE/Renderer/GpuTimer.h"
```

Final relevant portion:

```cpp
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/PostProcessStack.h"
#include "HBE/Renderer/GpuTimer.h"

#include "HBE/Core/Log.h"
```

### 2.2  Probe GPU timer support inside `initialize`

At the very end of `GLRenderer::initialize(...)` — right
before the current `m_initialized = true;` return path —
add one line. The current bottom of `initialize` (line ~80):

```cpp
        glDisable(GL_DEPTH_TEST);

        m_initialized = true;
        return true;
    }
```

Change to:

```cpp
        glDisable(GL_DEPTH_TEST);

        HBE::Renderer::GpuTimer::Initialize();

        m_initialized = true;
        return true;
    }
```

`Initialize()` is idempotent — safe to call more than once
even though it shouldn't happen.

### 2.3  Wrap the scene render in `GpuScene`

`beginFrameInViewport(...)` is called once per frame right
before the game layers submit their draws (see
`Application::run`). This is the natural top of the
"GpuScene" span.

Currently the tail of `beginFrameInViewport` (line ~262-278)
reads:

```cpp
    void GLRenderer::beginFrameInViewport(int vpX, int vpY, int vpW, int vpH) {
        if (!m_initialized) return;
        if (vpW <= 0 || vpH <= 0) return;

        m_vpX = vpX; m_vpY = vpY; m_vpW = vpW; m_vpH = vpH;

        if (m_postProcess && m_postProcess->isInitialized()) {
            m_postProcess->bindSceneFBO();
            glViewport(0, 0, m_postProcess->sceneFBO().width(), m_postProcess->sceneFBO().height());
            glClearColor(m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        else {
            glEnable(GL_SCISSOR_TEST);
            glViewport(vpX, vpY, vpW, vpH);
            glScissor(vpX, vpY, vpW, vpH);
            glClearColor(m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]);
            glClear(GL_COLOR_BUFFER_BIT);
            glDisable(GL_SCISSOR_TEST);
        }
    }
```

Add a static handle at file scope right above the function,
and open the scope on the last line of the function body:

```cpp
    // File-scope handle used across beginFrameInViewport / endFrame.
    static int s_gpuSceneHandle = -1;

    void GLRenderer::beginFrameInViewport(int vpX, int vpY, int vpW, int vpH) {
        if (!m_initialized) return;
        if (vpW <= 0 || vpH <= 0) return;

        m_vpX = vpX; m_vpY = vpY; m_vpW = vpW; m_vpH = vpH;

        if (m_postProcess && m_postProcess->isInitialized()) {
            m_postProcess->bindSceneFBO();
            glViewport(0, 0, m_postProcess->sceneFBO().width(), m_postProcess->sceneFBO().height());
            glClearColor(m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        else {
            glEnable(GL_SCISSOR_TEST);
            glViewport(vpX, vpY, vpW, vpH);
            glScissor(vpX, vpY, vpW, vpH);
            glClearColor(m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]);
            glClear(GL_COLOR_BUFFER_BIT);
            glDisable(GL_SCISSOR_TEST);
        }

        s_gpuSceneHandle = HBE::Renderer::GpuTimer::BeginScope("GpuScene");
    }
```

Do NOT use the `HBE_GPU_SCOPE` macro here — the scope spans
two function calls, so RAII inside one function won't work.
We open with `BeginScope` and close with `EndScope` at the
top of `endFrame`.

### 2.4  Close `GpuScene` and open/close `GpuPostProcess` in `endFrame`

Currently (line ~97-105):

```cpp
    void GLRenderer::endFrame(HBE::Platform::SDLPlatform& platform) {
        if (!m_initialized) return;

        if (m_postProcess && m_postProcess->isInitialized()) {
            m_postProcess->present(m_vpX, m_vpY, m_vpW, m_vpH);
        }

        platform.swapBuffers();
    }
```

Change to:

```cpp
    void GLRenderer::endFrame(HBE::Platform::SDLPlatform& platform) {
        if (!m_initialized) return;

        // Close the scene GPU scope opened in beginFrameInViewport.
        HBE::Renderer::GpuTimer::EndScope(s_gpuSceneHandle);
        s_gpuSceneHandle = -1;

        if (m_postProcess && m_postProcess->isInitialized()) {
            HBE_GPU_SCOPE("GpuPostProcess");
            m_postProcess->present(m_vpX, m_vpY, m_vpW, m_vpH);
        }

        platform.swapBuffers();
    }
```

Note that `HBE_GPU_SCOPE` is safe to use here because it
lives in a normal C++ scope — the `Scope` destructor fires
before we `swapBuffers`. The `if` block braces bound the
lifetime.

**Order guarantee:** `GpuScene` and `GpuPostProcess` never
overlap. They are **serial**, satisfying the "one
`GL_TIME_ELAPSED` at a time" GL rule (doc `01` §3).

### 2.5  Add a hook to call `GpuTimer::Shutdown()`

Currently `GLRenderer` has no explicit shutdown / dtor —
`~GLRenderer()` defaults to nothing. That's fine, but we
must free the query objects before the GL context tears
down. The safest hook is `Application::~Application()` after
`m_layers.clear()`.

**This edit is done in `Application.cpp` in §4.5 below —
not in `GLRenderer.cpp`.** No new dtor here.

---

## 3. Renderer2D — auto-add `resetFrameStats` on `beginFrame`? No.

Doc 02 §1.2 added `Renderer2D::resetFrameStats()`. We do NOT
call it from `Renderer2D::beginScene(...)` because a single
frame can `beginScene(...) / endScene(...)` multiple times
(one per RenderPass) — resetting mid-frame would corrupt
totals. Instead, `Application::run` calls it exactly once
per frame at the top (§4.1 below).

---

## 4. `Application.cpp` — six edits

Open `G:\Dev\HBE\HBE.Core\src\Core\Application.cpp`.

### 4.1  Add includes

The current include block already has `Profiler.h`
(transitively via `Application.h`) but does NOT yet include
`GpuTimer.h` or `Renderer2D.h`. Renderer2D IS visible via
`Application.h` already; we need `GpuTimer.h` explicitly.

Right after the existing `#include <cstdio>` line:

```cpp
#include <cstdio>
```

Add:

```cpp
#include "HBE/Renderer/GpuTimer.h"
```

Full include block will now be:

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

#include "HBE/Renderer/GpuTimer.h"
```

### 4.2  Call `GpuTimer::NewFrame()` at the top of the frame

Inside `Application::run`, find the block from Item 13:

```cpp
		while (m_running) {
			HBE::Core::Profiler::BeginFrame();

			HBE::Platform::Input::NewFrame();
```

Add **right after** `Profiler::BeginFrame();` — line where
you inserted it during Item 13:

```cpp
		while (m_running) {
			HBE::Core::Profiler::BeginFrame();
			HBE::Renderer::GpuTimer::NewFrame();

			HBE::Platform::Input::NewFrame();
```

### 4.3  Reset Renderer2D frame stats at the top of the frame

Find the block from Item 13 that begins with `// 1) Clear the whole window (black bars)`:

```cpp
			// 1) Clear the whole window (black bars)
			m_gl.beginFrameFullWindow(m_winW, m_winH);
```

Insert right BEFORE it:

```cpp
			// Item 14: zero per-frame renderer counters before any beginScene.
			m_renderer2D.resetFrameStats();
```

Final block:

```cpp
			// Item 14: zero per-frame renderer counters before any beginScene.
			m_renderer2D.resetFrameStats();

			// 1) Clear the whole window (black bars)
			m_gl.beginFrameFullWindow(m_winW, m_winH);
```

### 4.4  Publish renderer stats + GPU frame time before `EndFrame`

Below the render loop and above the `Profiler::EndFrame()`
you added in Item 13:

```cpp
			for (std::size_t i = 0; i < m_layers.m_layers.size(); ++i) {
				auto& layer = m_layers.m_layers[i];
				if (layer) layer->onRender();
			}

			m_gl.endFrame(m_platform);

			HBE::Core::Profiler::EndFrame();
```

Insert between `m_gl.endFrame(m_platform);` and
`HBE::Core::Profiler::EndFrame();`:

```cpp
			// Item 14: publish renderer stats to Profiler before the snapshot is built.
			{
				const auto rs = m_renderer2D.getStats();
				HBE::Core::Profiler::RendererStats out{};
				out.drawCalls          = rs.drawCalls;
				out.passes             = rs.passes;
				out.submittedQuads     = rs.submittedQuads;
				out.renderedQuads      = rs.renderedQuads;
				out.culledSprites      = rs.culledSprites;
				out.materialChanges    = rs.materialChanges;
				out.textureChanges     = rs.textureChanges;
				// visibleTileChunks / postProcessPasses / lights / liveParticles
				// are published by the game (or wired below if you own a fixed
				// scene). MegaX populates them in doc 05.
				HBE::Core::Profiler::PublishRendererStats(out);
			}
```

Note we do NOT touch `visibleTileChunks` / `postProcessPasses`
here — those are owned by the game (doc `05` §3) since not
every HBE game uses `TileMapRenderer` or a
`PostProcessStack`. Doc `05` shows the MegaX-side wiring.

`activeLights / shadowCastingLights / liveParticles` are
also game-side (Item 14 rules 9-10).

### 4.5  Call `GpuTimer::Shutdown()` in `~Application`

Currently the destructor (top of `Application.cpp`) reads:

```cpp
	Application::~Application() {
		m_layers.clear();
		// SDLPlatform destructur already calls shutdown()
	}
```

Change to:

```cpp
	Application::~Application() {
		m_layers.clear();
		HBE::Renderer::GpuTimer::Shutdown();
		// SDLPlatform destructur already calls shutdown()
	}
```

**Order matters.** `m_layers.clear()` runs first so any
in-flight GPU scopes owned by a layer are closed. Then
`GpuTimer::Shutdown()` frees the GL query IDs while the
context is still current (SDLPlatform's shutdown, called
last, is what tears it down).

### 4.6  Extend the 1-Hz log block

Find the 1-Hz `#if defined(HBE_PROFILER_LOG_ONCE_PER_SECOND)`
block from Item 13. After the loop that prints CPU sections
(the `for (const auto& s : snap.sections)`), add:

```cpp
					// GPU sections
					if (snap.gpu.supported) {
						char gpu[256];
						std::snprintf(gpu, sizeof(gpu),
							"[Profiler] GPU=%.2fms (avg %.2f min %.2f max %.2f)",
							snap.gpu.frameMs, snap.gpu.frameAvgMs,
							snap.gpu.frameMinMs, snap.gpu.frameMaxMs);
						LogInfo(gpu);
						for (const auto& g : snap.gpu.sections) {
							char line[256];
							const char* pad = "                    ";
							int spaces = g.depth * 2;
							if (spaces > 20) spaces = 20;
							std::snprintf(line, sizeof(line),
								"  %.*s%-18s cur=%6.2f avg=%6.2f min=%6.2f max=%6.2f (n=%zu)",
								spaces, pad,
								g.name ? g.name : "?",
								g.currentMs, g.avgMs, g.minMs, g.maxMs,
								g.sampleCount);
							LogInfo(line);
						}
					}
					else {
						LogInfo("[Profiler] GPU=unsupported (no ARB_timer_query)");
					}

					// Renderer stats
					const auto& r = snap.renderer;
					char rline1[256];
					std::snprintf(rline1, sizeof(rline1),
						"[Profiler] draws=%d passes=%d subQuads=%d rendQuads=%d culled=%d",
						r.drawCalls, r.passes, r.submittedQuads, r.renderedQuads, r.culledSprites);
					LogInfo(rline1);

					char rline2[256];
					std::snprintf(rline2, sizeof(rline2),
						"[Profiler] matChg=%d texChg=%d tileChunks=%d ppPasses=%d",
						r.materialChanges, r.textureChanges, r.visibleTileChunks, r.postProcessPasses);
					LogInfo(rline2);

					char rline3[256];
					std::snprintf(rline3, sizeof(rline3),
						"[Profiler] lights=%d shadowLights=%d liveParticles=%d",
						r.activeLights, r.shadowCastingLights, r.liveParticles);
					LogInfo(rline3);
```

This block must be pasted **inside** the existing
`if (nowS - s_lastLog >= 1.0) { ... }` guard from Item 13,
right after the CPU section loop closes. Don't put it
outside the guard — you'd log every frame.

---

## 5. What NOT to touch

* Do NOT open a GPU scope inside a layer's `onUpdate`. GPU
  scopes must live where a valid GL context is active AND
  no other `GL_TIME_ELAPSED` query is in flight. `onUpdate`
  is CPU-side.
* Do NOT open a GPU scope inside a `beginScene` /
  `endScene` block issued by a game. Renderer2D flushes at
  `endScene`, which is where the GPU work actually
  submits. The wrap in `GLRenderer` §2.3-2.4 covers the
  whole scene.
* Do NOT call `GpuTimer::NewFrame()` twice. Doing so drains
  the ring twice, missing ready slots.

---

## 6. Sanity check before doc 04

```powershell
Select-String -Path G:\Dev\HBE\HBE.Renderer.GL\src\Renderer\GLRenderer.cpp -Pattern "GpuTimer"
# -> expect 5 matches: 1 include + 1 Initialize + 1 static handle + 1 BeginScope + 1 EndScope + 1 HBE_GPU_SCOPE = 6 hits actually
# (adjust — Select-String counts each occurrence, close enough)

Select-String -Path G:\Dev\HBE\HBE.Core\src\Core\Application.cpp -Pattern "GpuTimer|resetFrameStats|PublishRendererStats"
# -> expect 5+ matches
```

Do not build yet — `Profiler::RendererStats`,
`PublishRendererStats`, `PublishGpuSection`, `SetGpuSupported`,
and `snap.gpu` / `snap.renderer` fields all don't exist yet.
Doc `04` adds them.

Next: `04_profiler_gpu_integration.md`.
