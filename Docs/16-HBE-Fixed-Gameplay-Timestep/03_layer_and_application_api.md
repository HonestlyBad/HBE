# 16 — The lifecycle and application API (`Layer.h`, `Application.h`)

This doc edits two headers:

* `/home/atulo/Projects/HBE/HBE.Core/include/HBE/Core/Layer.h`
  — one new virtual.
* `/home/atulo/Projects/HBE/HBE.Core/include/HBE/Core/Application.h`
  — one include, six methods, three members.

> **LINE NUMBERS IN THIS DOC.** Every line number below
> refers to the file **as it is before you start this doc** —
> they are all measured against the pristine file, never
> against a partly-edited one. Once you apply an edit, the
> numbers for every later edit in the same file shift down by
> whatever you inserted. That is why each edit is *also*
> anchored by the exact text at the insertion point: when the
> number and the text disagree, **the text wins**. Search for
> the quoted line, do not scroll to the number.

Two of the six new methods are **declared here and defined
in doc `04`**. That is fine: nothing calls them until doc
`04` does, so the project still builds and MegaX still runs
unchanged at the end of this doc. §6 confirms it.

---

## 1. `Layer.h` — the opt-in virtual

Open `/home/atulo/Projects/HBE/HBE.Core/include/HBE/Core/Layer.h`.

The whole file is 20 lines. The class body currently reads
(lines 8-19):

```cpp
	class Layer {
	public:
		virtual ~Layer() = default;

		virtual void onAttach(Application& app) {}
		virtual void onDetach() {}

		virtual void onUpdate(float dt) {}
		virtual void onRender() {}

		virtual bool onEvent(Event& e) { (void)e; return false; }
	};
```

Insert right after `virtual void onUpdate(float dt) {}`
(line 15) and before `virtual void onRender() {}` (line 16):

```cpp

		// Fixed-rate simulation step (item 16). Runs zero or more times per
		// rendered frame, always with the same fixedDt, always AFTER this
		// frame's onUpdate and before onRender. Put physics, combat and AI
		// here so they behave identically at 30, 60, 120 and uncapped
		// rendering rates.
		//
		// Opt-in: this has a body, so a layer that does not override it is
		// completely unaffected by the fixed timestep and keeps whatever
		// behaviour it had before item 16.
		//
		// Read Application::interpolationAlpha() in onRender -- NOT here --
		// to smooth the visual gap between steps.
		virtual void onFixedUpdate(float fixedDt) { (void)fixedDt; }
```

The final class body should read:

```cpp
	class Layer {
	public:
		virtual ~Layer() = default;

		virtual void onAttach(Application& app) {}
		virtual void onDetach() {}

		virtual void onUpdate(float dt) {}

		// Fixed-rate simulation step (item 16). Runs zero or more times per
		// rendered frame, always with the same fixedDt, always AFTER this
		// frame's onUpdate and before onRender. Put physics, combat and AI
		// here so they behave identically at 30, 60, 120 and uncapped
		// rendering rates.
		//
		// Opt-in: this has a body, so a layer that does not override it is
		// completely unaffected by the fixed timestep and keeps whatever
		// behaviour it had before item 16.
		//
		// Read Application::interpolationAlpha() in onRender -- NOT here --
		// to smooth the visual gap between steps.
		virtual void onFixedUpdate(float fixedDt) { (void)fixedDt; }

		virtual void onRender() {}

		virtual bool onEvent(Event& e) { (void)e; return false; }
	};
```

> **WHY `(void)fixedDt`?** `onUpdate(float dt) {}` gets away
> with an unnamed-but-named parameter because the build
> passes `-Wno-unused-parameter`. `onEvent` still writes
> `(void)e;`. Follow `onEvent` — the cast documents that the
> default really is "do nothing", and it survives someone
> tightening the warning flags later.

That is the entire `Layer.h` change. `LayerStack` needs no
edit: `Application::run` iterates `m_layers.m_layers`
directly, which is a public member.

---

## 2. `Application.h` — the include

Open `/home/atulo/Projects/HBE/HBE.Core/include/HBE/Core/Application.h`.

The include block at the top currently reads (lines 1-11):

```cpp
#pragma once

#include "HBE/Core/LayerStack.h"
#include "HBE/Core/Profiler.h"
#include "HBE/Core/AssetPaths.h"

#include "HBE/Platform/SDLPlatform.h"
#include "HBE/Platform/Audio.h"
#include "HBE/Renderer/GLRenderer.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/ResourceCache.h"
```

Insert right after `#include "HBE/Core/AssetPaths.h"`
(line 5) and before the blank line that precedes
`#include "HBE/Platform/SDLPlatform.h"`:

```cpp
#include "HBE/Core/Timestep.h"
```

Final block:

```cpp
#pragma once

#include "HBE/Core/LayerStack.h"
#include "HBE/Core/Profiler.h"
#include "HBE/Core/AssetPaths.h"
#include "HBE/Core/Timestep.h"

#include "HBE/Platform/SDLPlatform.h"
#include "HBE/Platform/Audio.h"
#include "HBE/Renderer/GLRenderer.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/ResourceCache.h"
```

The Core group is not alphabetical today and this doc does
not make it so — reordering the group would be a reformat of
lines you are only inserting into.

---

## 3. `Application.h` — the public methods

Still in the same file. The public section currently reads
(lines 37-44):

```cpp
		void run();
		void requestQuit() { m_running = false; }

		void pushLayer(std::unique_ptr<Layer> layer);
		void pushOverlay(std::unique_ptr<Layer> overlay);

		// Logical render size for letterboxing
		void setLogicalSize(int w, int h) { m_logicalW = w; m_logicalH = h; recalcViewportAndNotify(); }
```

Insert right after `void requestQuit() { m_running = false; }`
(line 38) and before the blank line that precedes
`void pushLayer(...)` (line 40):

```cpp

		// --- item 16: fixed gameplay timestep -------------------------
		// Reconfigure the simulation rate. Safe at any time, including from
		// inside a layer callback: the change takes effect on the next
		// frame, never mid-step-loop. Logs the resolved (validated) values.
		void setFixedTimestep(const FixedTimestepConfig& cfg);

		const FixedTimestep& timestep() const { return m_timestep; }

		// Seconds of game time one onFixedUpdate() represents. Constant.
		float fixedDeltaSeconds() const { return m_timestep.fixedDeltaSeconds(); }

		// How far between the last completed fixed step and the next one
		// this frame is being drawn, in [0, 1]. VALID DURING onRender().
		// Stale during onUpdate() and meaningless inside onFixedUpdate().
		// Use it for presentation only -- never feed it into simulation.
		float interpolationAlpha() const { return m_timestep.alpha(); }

		// Optional render-rate cap, in frames per second. 0 (the default)
		// means uncapped, which reproduces the pre-item-16 behaviour
		// exactly. With vsync on, this can only make the loop slower than
		// the display, never faster.
		void setTargetFrameRate(float hz);
		float targetFrameRate() const { return m_targetFrameRate; }
```

The final region should read:

```cpp
		void run();
		void requestQuit() { m_running = false; }

		// --- item 16: fixed gameplay timestep -------------------------
		// Reconfigure the simulation rate. Safe at any time, including from
		// inside a layer callback: the change takes effect on the next
		// frame, never mid-step-loop. Logs the resolved (validated) values.
		void setFixedTimestep(const FixedTimestepConfig& cfg);

		const FixedTimestep& timestep() const { return m_timestep; }

		// Seconds of game time one onFixedUpdate() represents. Constant.
		float fixedDeltaSeconds() const { return m_timestep.fixedDeltaSeconds(); }

		// How far between the last completed fixed step and the next one
		// this frame is being drawn, in [0, 1]. VALID DURING onRender().
		// Stale during onUpdate() and meaningless inside onFixedUpdate().
		// Use it for presentation only -- never feed it into simulation.
		float interpolationAlpha() const { return m_timestep.alpha(); }

		// Optional render-rate cap, in frames per second. 0 (the default)
		// means uncapped, which reproduces the pre-item-16 behaviour
		// exactly. With vsync on, this can only make the loop slower than
		// the display, never faster.
		void setTargetFrameRate(float hz);
		float targetFrameRate() const { return m_targetFrameRate; }

		void pushLayer(std::unique_ptr<Layer> layer);
		void pushOverlay(std::unique_ptr<Layer> overlay);

		// Logical render size for letterboxing
		void setLogicalSize(int w, int h) { m_logicalW = w; m_logicalH = h; recalcViewportAndNotify(); }
```

> **WHY `const FixedTimestep&` AND THREE FORWARDING
> GETTERS?** `timestep()` exposes the diagnostics
> (`stepsLastFrame`, `droppedStepsLastFrame`,
> `accumulatorSeconds`, `config`) without `Application`
> growing a wrapper for each. But `fixedDeltaSeconds()` and
> `interpolationAlpha()` are read from game code every
> single frame, and `app.interpolationAlpha()` reads a great
> deal better at a draw call than
> `app.timestep().alpha()`. The two spellings are the same
> value; the short one is the one to use.

---

## 4. `Application.h` — the private members

Still in the same file. The private section currently reads
(lines 64-81):

```cpp
	private:
		// store config so app can toggle mode
		HBE::Platform::WindowConfig m_windowCfg{};

		bool m_initialized = false;
		bool m_running = false;

		HBE::Platform::SDLPlatform m_platform;
		HBE::Platform::Audio m_audio;

		HBE::Renderer::GLRenderer m_gl;
		HBE::Renderer::Renderer2D m_renderer2D{ m_gl };
		HBE::Renderer::ResourceCache m_resources;

		LayerStack m_layers;

		int m_winW = 0;
		int m_winH = 0;
```

Insert right after `LayerStack m_layers;` (line 78) and
before the blank line that precedes `int m_winW = 0;`
(line 80):

```cpp

		// --- item 16 ---
		FixedTimestep m_timestep{};

		// 0 = uncapped. Wall-clock time the next frame should not start
		// before; advanced by exactly one period per frame so the cap
		// tracks a rate rather than adding a fixed delay to each frame.
		float m_targetFrameRate = 0.0f;
		double m_nextFrameTime = 0.0;
```

The final region should read:

```cpp
		LayerStack m_layers;

		// --- item 16 ---
		FixedTimestep m_timestep{};

		// 0 = uncapped. Wall-clock time the next frame should not start
		// before; advanced by exactly one period per frame so the cap
		// tracks a rate rather than adding a fixed delay to each frame.
		float m_targetFrameRate = 0.0f;
		double m_nextFrameTime = 0.0;

		int m_winW = 0;
		int m_winH = 0;
```

`m_timestep` is default constructed, which means
`FixedTimestep`'s in-class initialisers apply:
`m_fixedDt = 1.0f / 60.0f` and a zeroed accumulator. Note
that this is **not** the same as having called
`configure()` — `m_cfg` holds its own defaults (60 Hz, 5
steps, 0.25 s) and they happen to agree with `m_fixedDt`, so
an application that never calls `setFixedTimestep` gets a
correct 60 Hz step. Doc `04` does not call `configure()` at
startup for exactly that reason.

`m_nextFrameTime` is `double` to match `GetTimeSeconds()`,
which returns `double`. A `float` accumulating wall-clock
seconds loses millisecond resolution after about four hours
of runtime.

---

## 5. What NOT to touch

* **Do not give `onFixedUpdate` a `= 0`.** A pure virtual
  would break `HBE.Sandbox`, `HBMapMaker` and every future
  layer that does not want a fixed step, and it would
  violate golden rule 2.
* **Do not add `onFixedUpdate` to `LayerStack`.**
  `LayerStack::dispatchEvent` exists because event
  dispatch has ordering and consumption semantics. Update
  and render do not — `Application::run` walks
  `m_layers.m_layers` inline for both, and the fixed step
  follows suit.
* **Do not remove `m_running`, `prevTime`, or the existing
  `dt` clamp** while you are in `Application`. Golden rule 2:
  a layer that ignores the fixed step must be unaffected,
  and `onUpdate` still receives the same clamped `dt` it
  always did.
* **Do not make `interpolationAlpha()` non-const or have it
  recompute.** It forwards to a latched value; see doc `01`
  §3.2.
* **Do not put `setFixedTimestep`'s body in the header.** It
  logs, which would drag `Log.h` into `Application.h` and
  therefore into every game translation unit.

---

## 6. Sanity check before doc `04`

```fish
grep -c 'onFixedUpdate' /home/atulo/Projects/HBE/HBE.Core/include/HBE/Core/Layer.h
# -> 1

grep -c 'Timestep.h' /home/atulo/Projects/HBE/HBE.Core/include/HBE/Core/Application.h
# -> 1

grep -n 'setFixedTimestep\|interpolationAlpha\|setTargetFrameRate\|fixedDeltaSeconds\|m_timestep\|m_targetFrameRate\|m_nextFrameTime' \
    /home/atulo/Projects/HBE/HBE.Core/include/HBE/Core/Application.h
# -> 9 lines: 5 method declarations, 1 forwarding body each for
#    fixedDeltaSeconds/interpolationAlpha/targetFrameRate, 3 members
```

**Should the project compile now?** Yes — build it and make
sure:

```fish
cd /home/atulo/Projects/HBE
cmake --build --preset linux-clang-debug --target MegaX
```

`setFixedTimestep` and `setTargetFrameRate` are **declared
but not defined** at this point, and that is fine: nothing
calls them yet, so the linker never looks for them. They get
their bodies in doc `04` §1, which is also the first doc
that calls them.

MegaX's behaviour is still unchanged — `onFixedUpdate` is
declared but nothing invokes it. Any error you do see is a
real mistake:

* `error: unknown type name 'FixedTimestep'` — the include
  in §2 is missing or misspelled.
* `error: no member named 'alpha' in 'HBE::Core::FixedTimestep'`
  — doc `01`'s header did not get saved.

Next: `04_application_loop_wiring.md`.
