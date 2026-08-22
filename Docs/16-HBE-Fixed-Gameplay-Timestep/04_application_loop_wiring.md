# 16 — Wiring the loop (`HBE.Core/src/Core/Application.cpp`)

This doc edits one file:
`/home/atulo/Projects/HBE/HBE.Core/src/Core/Application.cpp`
(486 lines before you start). Four separate edits:

1. one include,
2. two new method definitions placed just before `run()`,
3. three insertions inside `run()`,
4. one replacement of the loop's trailing `delayMillis(1)`.

At the end of this doc the whole engine builds, MegaX builds,
and MegaX behaves **exactly** as it did before — because
`GameLayer` does not override `onFixedUpdate` yet. That is
the point of stopping here: it proves the engine change is
neutral before any game code moves.

> **LINE NUMBERS IN THIS DOC.** Every line number below
> refers to the file **as it is before you start this doc** —
> they are all measured against the pristine file, never
> against a partly-edited one. Once you apply an edit, the
> numbers for every later edit in the same file shift down by
> whatever you inserted. That is why each edit is *also*
> anchored by the exact text at the insertion point: when the
> number and the text disagree, **the text wins**. Search for
> the quoted line, do not scroll to the number.

> **PASTE NOTE — INDENTATION.** `Application.cpp` is
> tab-indented throughout, including the item-14 block at
> lines 390-402. The `run()` body sits at **two** tabs, the
> `while (m_running)` body at **three**, and a block inside
> that at **four**. Every code block below is written with
> hard tabs at the depth it belongs at.

---

## 1. Edit 1 — the include block

The top of the file currently reads (lines 1-13):

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

Insert right after `#include <cstdio>` (line 11) and before
the blank line that precedes
`#include "HBE/Renderer/GpuTimer.h"`:

```cpp
#include <cstdint>
```

Final block:

```cpp
#include <SDL3/SDL.h>
#include <SDL3/SDL_scancode.h>
#include <cstdio>
#include <cstdint>
```

`SDL.h` almost certainly drags `<cstdint>` in already, but
the frame limiter in §4 names `std::uint32_t` directly and a
translation unit should include what it uses.

`Timestep.h` needs no include here — `Application.h` pulls
it in (doc `03` §2).

---

## 2. Edit 2 — the two new method definitions

Find `void Application::syncWindowSizeAndViewport()`. It
ends at line 321 with a closing brace, and line 323 begins
`void Application::run() {`:

```cpp
		// Note: GLRenderer::resizeViewport early-outs if not initialized but at this point it is.
		m_gl.resizeViewport(m_winW, m_winH);
	}

	void Application::run() {
```

Insert the following between that closing brace (line 321)
and `void Application::run() {` (line 323), keeping one
blank line on each side:

```cpp
	void Application::setFixedTimestep(const FixedTimestepConfig& cfg) {
		m_timestep.configure(cfg);

		// Log what we RESOLVED to, not what was asked for. configure()
		// silently corrects out-of-range values (Timestep.cpp has no
		// logger by design), so this line is the only place a bad
		// --fixed-hz becomes visible.
		const FixedTimestepConfig& r = m_timestep.config();
		char buf[192];
		std::snprintf(buf, sizeof(buf),
			"Application: fixed timestep %.2f Hz (%.4f s), max %d catch-up steps, frame clamp %.3f s.",
			static_cast<double>(r.hz),
			static_cast<double>(m_timestep.fixedDeltaSeconds()),
			r.maxCatchUpSteps,
			static_cast<double>(r.maxFrameSeconds));
		LogInfo(buf);
	}

	void Application::setTargetFrameRate(float hz) {
		m_targetFrameRate = (hz > 0.0f) ? hz : 0.0f;

		// Re-base the schedule. Without this, changing the cap mid-run
		// leaves m_nextFrameTime pointing at a deadline computed from the
		// old period, and the loop either sleeps for a very long time or
		// free-runs for a burst of frames while it catches up.
		m_nextFrameTime = GetTimeSeconds();

		char buf[128];
		if (m_targetFrameRate > 0.0f) {
			std::snprintf(buf, sizeof(buf),
				"Application: render rate capped at %.2f FPS.",
				static_cast<double>(m_targetFrameRate));
		}
		else {
			std::snprintf(buf, sizeof(buf), "Application: render rate uncapped.");
		}
		LogInfo(buf);
	}
```

> **WHY `static_cast<double>` ON EVERY FLOAT?** `snprintf`
> is variadic, so `float` arguments are promoted to `double`
> automatically and the casts are technically redundant.
> They are there because `-Wdouble-promotion` is the kind of
> flag that gets turned on later, and because the explicit
> cast makes it obvious at a glance that `%.2f` is being fed
> the right width. The item-13/14 `snprintf` blocks in this
> same function already pass `double` members directly, so
> the file has no counter-example to contradict.

---

## 3. Edit 3 — inside `run()`

### 3.1 Initialise the schedule and log the timestep

The head of `run()` currently reads (lines 323-333):

```cpp
	void Application::run() {
		if (!m_initialized) {
			LogError("Application::run called before initialize()");
			return;
		}

		m_running = true;

		double prevTime = GetTimeSeconds();

		while (m_running) {
```

Replace lines 329-331 — that is, `m_running = true;` through
`double prevTime = GetTimeSeconds();` — with:

```cpp
		m_running = true;

		double prevTime = GetTimeSeconds();

		// --- item 16 ---
		// Start both clocks from the same instant so the first frame does
		// not inherit however long initialize() took.
		m_nextFrameTime = prevTime;
		m_timestep.reset();

		{
			// Log unconditionally, not just when setFixedTimestep() was
			// called: an application that never configures the timestep
			// still runs one, and "which rate am I simulating at" is the
			// first question every capture in item 15 raises.
			const FixedTimestepConfig& tsCfg = m_timestep.config();
			char tsBuf[192];
			std::snprintf(tsBuf, sizeof(tsBuf),
				"Application: fixed timestep %.2f Hz (%.4f s), max %d catch-up steps, frame clamp %.3f s.",
				static_cast<double>(tsCfg.hz),
				static_cast<double>(m_timestep.fixedDeltaSeconds()),
				tsCfg.maxCatchUpSteps,
				static_cast<double>(tsCfg.maxFrameSeconds));
			LogInfo(tsBuf);
		}
```

Final head of `run()`:

```cpp
	void Application::run() {
		if (!m_initialized) {
			LogError("Application::run called before initialize()");
			return;
		}

		m_running = true;

		double prevTime = GetTimeSeconds();

		// --- item 16 ---
		// Start both clocks from the same instant so the first frame does
		// not inherit however long initialize() took.
		m_nextFrameTime = prevTime;
		m_timestep.reset();

		{
			// Log unconditionally, not just when setFixedTimestep() was
			// called: an application that never configures the timestep
			// still runs one, and "which rate am I simulating at" is the
			// first question every capture in item 15 raises.
			const FixedTimestepConfig& tsCfg = m_timestep.config();
			char tsBuf[192];
			std::snprintf(tsBuf, sizeof(tsBuf),
				"Application: fixed timestep %.2f Hz (%.4f s), max %d catch-up steps, frame clamp %.3f s.",
				static_cast<double>(tsCfg.hz),
				static_cast<double>(m_timestep.fixedDeltaSeconds()),
				tsCfg.maxCatchUpSteps,
				static_cast<double>(tsCfg.maxFrameSeconds));
			LogInfo(tsBuf);
		}

		while (m_running) {
```

If an application called `setFixedTimestep` before `run()`
— MegaX does, when `--fixed-hz` is on the command line —
you now get two lines with the same shape. That is correct
and deliberate: the first proves the request was accepted,
the second proves nothing overwrote it.

### 3.2 Leave the `dt` block completely alone

The `dt` block currently reads (lines 353-358):

```cpp
			// dt
			double now = GetTimeSeconds();
			float dt = static_cast<float>(now - prevTime);
			if (dt < 0.0f) dt = 0.0f;
			if (dt > 0.25f) dt = 0.25f;
			prevTime = now;
```

**Do not change any of it.** In particular do not remove the
`0.25f` clamp on the grounds that `FixedTimestep` now does
its own clamping. They clamp different things for different
consumers: this one protects `onUpdate(dt)` and
`m_audio.update(dt)`, which still run on the render clock;
`maxFrameSeconds` protects the accumulator. Golden rule 2 —
a layer that ignores the fixed step must see the same `dt`
it always did.

### 3.3 The step loop

The update block and the audio block currently read:

```cpp
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
```

Insert the following between the closing brace of the
`ApplicationUpdate` block and the
`// keep audio spatialization ...` comment, with one blank
line on each side:

```cpp
			// --- item 16: fixed-rate simulation ---
			// This runs AFTER onUpdate on purpose. Input was polled at the
			// top of this frame; layers sample it and latch their intents
			// in onUpdate. Stepping first would feed every fixed step input
			// that is one whole frame stale -- guaranteed extra latency in
			// anything that needs a precise jump. Running second costs one
			// frame of staleness in the camera and the animation instead,
			// which is invisible behind a lerped camera.
			//
			// Zero steps on most frames is the normal case: at 300 FPS with
			// a 60 Hz step, four frames out of five only accumulate.
			{
				HBE_PROFILE_SCOPE("FixedUpdate");

				m_timestep.beginFrame(dt);
				const float fixedDt = m_timestep.fixedDeltaSeconds();

				while (m_timestep.consumeStep()) {
					for (std::size_t i = 0; i < m_layers.m_layers.size(); ++i) {
						auto& layer = m_layers.m_layers[i];
						if (layer) layer->onFixedUpdate(fixedDt);
					}
				}
			}
```

Final region:

```cpp
			// update
			{
				HBE_PROFILE_SCOPE("ApplicationUpdate");
				for (std::size_t i = 0; i < m_layers.m_layers.size(); ++i) {
					auto& layer = m_layers.m_layers[i];
					if (layer) layer->onUpdate(dt);
				}
			}

			// --- item 16: fixed-rate simulation ---
			// This runs AFTER onUpdate on purpose. Input was polled at the
			// top of this frame; layers sample it and latch their intents
			// in onUpdate. Stepping first would feed every fixed step input
			// that is one whole frame stale -- guaranteed extra latency in
			// anything that needs a precise jump. Running second costs one
			// frame of staleness in the camera and the animation instead,
			// which is invisible behind a lerped camera.
			//
			// Zero steps on most frames is the normal case: at 300 FPS with
			// a 60 Hz step, four frames out of five only accumulate.
			{
				HBE_PROFILE_SCOPE("FixedUpdate");

				m_timestep.beginFrame(dt);
				const float fixedDt = m_timestep.fixedDeltaSeconds();

				while (m_timestep.consumeStep()) {
					for (std::size_t i = 0; i < m_layers.m_layers.size(); ++i) {
						auto& layer = m_layers.m_layers[i];
						if (layer) layer->onFixedUpdate(fixedDt);
					}
				}
			}

			// keep audio spatialization / finished-track cleanup fresh
			{
				HBE_PROFILE_SCOPE("Audio");
				m_audio.update(dt);
			}
```

Three things about that block are load-bearing:

* **`fixedDt` is read once, outside the loop.** It cannot
  change during a frame — `configure()` is the only thing
  that changes it, and a layer calling `setFixedTimestep`
  from inside `onFixedUpdate` would otherwise change the
  step size *between* steps of the same frame.
* **The layer loop is re-walked on every step.** It is a
  handful of pointers and it means a layer pushed during a
  fixed step is picked up on the next one, matching how the
  `onUpdate` and `onRender` loops already behave.
* **`HBE_PROFILE_SCOPE("FixedUpdate")` wraps the whole
  block, not each step.** Item 13's profiler accumulates
  repeated opens of the same section within a frame
  (`SectionStats::currentMs += deltaMs`), so either spelling
  totals correctly — but wrapping once also captures the
  frames that ran **zero** steps, which is how you tell
  "the accumulator is filling" apart from "the section never
  opened".

---

## 4. Edit 4 — the frame limiter

The bottom of the loop currently reads (lines 479-482):

```cpp
#			endif

			m_platform.delayMillis(1);
		}

		LogInfo("Application exiting run loop.");
	}
```

Replace the single line `m_platform.delayMillis(1);` with:

```cpp
			// --- item 16: optional render-rate cap ---
			// Sleep-only, never a busy spin. A spin would hit the target
			// rate more precisely and would also burn a core and poison
			// every item-15 capture taken with a cap on. The millisecond or
			// two of residual jitter is exactly the thing the fixed
			// timestep exists to absorb, so precision here buys nothing.
			//
			// With vsync on, this and the swap interval are both floors:
			// the loop runs at whichever is slower.
			if (m_targetFrameRate > 0.0f) {
				const double period = 1.0 / static_cast<double>(m_targetFrameRate);
				m_nextFrameTime += period;

				const double nowLimit = GetTimeSeconds();
				if (m_nextFrameTime <= nowLimit) {
					// Already late. Re-base rather than clawing the time
					// back -- catching up would mean a burst of zero-delay
					// frames after every stall, which is the opposite of a
					// steady render rate.
					m_nextFrameTime = nowLimit;
				}
				else {
					const double slack = m_nextFrameTime - nowLimit;
					const std::uint32_t ms = static_cast<std::uint32_t>(slack * 1000.0);
					if (ms > 0) m_platform.delayMillis(ms);
				}
			}
			else {
				m_platform.delayMillis(1);
			}
```

Final region:

```cpp
#			endif

			// --- item 16: optional render-rate cap ---
			// Sleep-only, never a busy spin. A spin would hit the target
			// rate more precisely and would also burn a core and poison
			// every item-15 capture taken with a cap on. The millisecond or
			// two of residual jitter is exactly the thing the fixed
			// timestep exists to absorb, so precision here buys nothing.
			//
			// With vsync on, this and the swap interval are both floors:
			// the loop runs at whichever is slower.
			if (m_targetFrameRate > 0.0f) {
				const double period = 1.0 / static_cast<double>(m_targetFrameRate);
				m_nextFrameTime += period;

				const double nowLimit = GetTimeSeconds();
				if (m_nextFrameTime <= nowLimit) {
					// Already late. Re-base rather than clawing the time
					// back -- catching up would mean a burst of zero-delay
					// frames after every stall, which is the opposite of a
					// steady render rate.
					m_nextFrameTime = nowLimit;
				}
				else {
					const double slack = m_nextFrameTime - nowLimit;
					const std::uint32_t ms = static_cast<std::uint32_t>(slack * 1000.0);
					if (ms > 0) m_platform.delayMillis(ms);
				}
			}
			else {
				m_platform.delayMillis(1);
			}
		}

		LogInfo("Application exiting run loop.");
	}
```

The `else` branch is the original line, unchanged. With the
default `m_targetFrameRate = 0.0f`, this whole edit is one
predictable branch and behaviour is identical to item 15.

> **NOTE ON PRECISION.** `static_cast<std::uint32_t>` on
> `slack * 1000.0` truncates, so the loop always sleeps
> *less* than the remaining slack and never overshoots the
> deadline by a whole millisecond. At `--render-hz 120` the
> period is 8.33 ms and the sleep is 8 ms, so the measured
> rate lands slightly above 120 rather than below. That is
> the intended direction of the error.

---

## 5. What NOT to touch

* **Do not move `Profiler::BeginFrame` / `EndFrame`.** They
  bracket the rendered frame, not the simulation step. A
  `BeginFrame` per fixed step would reset the section table
  several times per rendered frame and destroy item 15's
  CSV.
* **Do not call `GpuTimer::NewFrame()` from the step loop.**
  Item 14's query ring is indexed by rendered frame; an
  extra advance per step would rotate the ring faster than
  results come back and every `gpuMs` would read `-1`.
* **Do not put the step loop after the render loop.** The
  render loop reads `interpolationAlpha()`, which is only
  latched once the step loop has finished. Stepping after
  rendering would draw with an alpha that is a full frame
  old.
* **Do not add a `HBE_PROFILE_SCOPE` inside the `while`.**
  One scope per step means up to `maxCatchUpSteps` opens per
  frame of a section that already totals correctly from the
  outside, and it hides the zero-step frames.
* **Do not `reset()` the timestep on a window resize or a
  focus change.** `handleSDLEvent` is tempting here. It is
  wrong: the frame that follows a resize is exactly the long
  frame `maxFrameSeconds` exists to clamp, and resetting
  would throw away legitimately pending game time on every
  window drag.
* **Do not touch `m_platform.delayMillis(1)` in the `else`
  branch.** That 1 ms yield is what keeps an uncapped,
  vsync-off MegaX from pinning a core at 100 %.

---

## 6. Sanity check before doc `05`

```fish
cd /home/atulo/Projects/HBE

grep -c 'onFixedUpdate' HBE.Core/src/Core/Application.cpp
# -> 1

grep -c 'm_timestep' HBE.Core/src/Core/Application.cpp
# -> 9

grep -c 'FixedUpdate' HBE.Core/src/Core/Application.cpp
# -> 2   (the profile scope name and the comment banner)

grep -c 'm_targetFrameRate' HBE.Core/src/Core/Application.cpp
# -> 5

# The step loop is between ApplicationUpdate and Audio:
grep -n 'ApplicationUpdate\|HBE_PROFILE_SCOPE("FixedUpdate")\|HBE_PROFILE_SCOPE("Audio")' \
    HBE.Core/src/Core/Application.cpp
# -> three lines, in that order, ascending
```

**Should the project compile now?** Yes, and this is the
checkpoint that matters most in the whole item:

```fish
cmake --build --preset linux-clang-debug
./build/linux-clang/bin/Debug/MegaX
```

Expect the new startup line, early, right after the window
opens:

```
[INFO]Application: fixed timestep 60.00 Hz (0.0167 s), max 5 catch-up steps, frame clamp 0.250 s.
```

and then a 1-Hz profiler block that now carries a
`FixedUpdate` row reading essentially zero:

```
[INFO][Profiler] Frame=1.84ms (avg 1.90 min 0.16 max 3.02)
[INFO]  ApplicationUpdate  cur=  0.21 avg=  0.19 ...
[INFO]    SceneUpdate      cur=  0.19 ...
[INFO]      Physics        cur=  0.01 ...
[INFO]  FixedUpdate        cur=  0.00 avg=  0.00 min=  0.00 max=  0.00 (n=120)
[INFO]  Audio              cur=  0.00 ...
```

`FixedUpdate` reads `0.00` because no layer overrides
`onFixedUpdate` yet, and `Physics` is still nested under
`SceneUpdate` because MegaX has not moved. **Play the game
for a minute.** Movement, jumping, shooting, the enemy, `F5`,
`F8` — all of it must feel and read exactly as it did before
you started this item. If anything changed, the engine edit
is wrong, and finding that out now is far cheaper than
finding it out after doc `06` has moved half of MegaX.

Also confirm the cap is inert by default: MegaX never calls
`setTargetFrameRate` until doc `06`, so there should be **no**
`render rate capped` line in the log.

Next: `05_megax_entities_fixed_update.md`.
