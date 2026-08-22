# 16 — The fixed timestep API (`HBE/Core/Timestep.h`)

This doc creates one new header and nothing else. No build
file changes, no existing file changes. At the end of it the
project still builds exactly as it did — the header is not
included from anywhere yet.

The header defines the accumulator that turns a stream of
irregular frame times into a stream of identical simulation
steps. It is deliberately the smallest thing that can do
that job, and it knows nothing about layers, applications,
rendering or logging.

---

## 1. What this header may and may not include

**May include:** nothing. Not `<cstdint>`, not `<cmath>`,
not `<algorithm>`. Everything it declares is `float`, `int`
and `bool`.

**May not include:** `HBE/Core/Log.h`, `<vector>`,
`<SDL3/SDL.h>`, or any other engine header.

> **WHY HERE?** `Timestep.h` gets included by
> `Application.h` in doc `03`, and `Application.h` is
> included by every layer of every game in the repo. Every
> transitive include it picks up is paid for by MegaX,
> Sandbox and MapMaker on every translation unit. `Profiler.h`
> follows the same discipline — it includes only `<cstdint>`,
> `<cstddef>` and `<vector>`, and only because its
> `Snapshot` genuinely needs a vector. This header needs
> nothing, so it includes nothing.

Validation that wants to *complain* about a bad config
therefore cannot log from here. It silently corrects
instead, and `Application::setFixedTimestep` (doc `04`) logs
the corrected result — that class already owns a logger.

---

## 2. Create the file

**Full path:** `/home/atulo/Projects/HBE/HBE.Core/include/HBE/Core/Timestep.h`

**Status:** file must not exist yet. Check first:

```fish
test -f /home/atulo/Projects/HBE/HBE.Core/include/HBE/Core/Timestep.h; echo $status
# -> 1   (does not exist — good)
```

If that prints `0`, stop and look at what is already there —
someone started this work item already.

Paste the following content **verbatim** (no reformatting):

```cpp
#pragma once

namespace HBE::Core {

	// Configuration for the fixed gameplay timestep. All three knobs are
	// validated in FixedTimestep::configure() -- pass whatever you like and
	// read config() back to see what you actually got.
	struct FixedTimestepConfig {
		// Simulation rate in steps per second. Clamped to [1, 1000].
		// This is the ONLY rate knob; there is no on/off switch, because a
		// disabled fixed step means onFixedUpdate() silently never fires,
		// which is indistinguishable from a frozen game. A layer that wants
		// variable-rate behaviour simply does not override onFixedUpdate().
		float hz = 60.0f;

		// The most fixed steps a single rendered frame may run. Clamped to
		// >= 1. Bounds worst-case frame cost after a stall: without it, one
		// long frame can queue hundreds of steps, which makes the next frame
		// longer still -- the "spiral of death".
		int maxCatchUpSteps = 5;

		// The longest single frame that may be fed into the accumulator.
		// Longer frames are truncated to this BEFORE being added, so a
		// two-second alt-tab stall contributes at most this much game time.
		// Matches the clamp Application::run has always applied to dt.
		//
		// The relationship between this and maxCatchUpSteps is the entire
		// hitch-recovery policy:
		//   maxFrameSeconds >  maxCatchUpSteps / hz  -> hitches SKIP game
		//                                               time (surplus is
		//                                               dropped and counted)
		//   maxFrameSeconds == maxCatchUpSteps / hz  -> hitches DILATE game
		//                                               time (nothing is ever
		//                                               dropped; slow motion
		//                                               until we catch up)
		// The default (0.25 vs 5/60 = 0.083) is "skip", because that is what
		// the variable-dt loop did before this item existed.
		float maxFrameSeconds = 0.25f;
	};

	// An accumulator that converts irregular frame times into identical
	// simulation steps. Not thread safe; owned and driven by Application.
	//
	// Usage, once per rendered frame:
	//
	//     ts.beginFrame(frameDeltaSeconds);
	//     const float h = ts.fixedDeltaSeconds();
	//     while (ts.consumeStep()) {
	//         simulate(h);
	//     }
	//     // ts.alpha() is now valid for this frame's rendering
	//
	// Note that the consumeStep() call which returns false is NOT a no-op:
	// it drains any surplus the step budget could not run and latches
	// alpha(). Calling beginFrame() without looping to a false consumeStep()
	// leaves alpha() stale.
	class FixedTimestep {
	public:
		// Validates cfg, recomputes fixedDeltaSeconds(), and resets the
		// accumulator. Safe to call at any time, including mid-run.
		void configure(const FixedTimestepConfig& cfg);

		// The validated config -- may differ from what was passed to
		// configure() if a value was out of range.
		const FixedTimestepConfig& config() const { return m_cfg; }

		// Seconds of game time one step represents: 1 / config().hz.
		// Constant between configure() calls. This is the value every
		// onFixedUpdate() receives, on every machine, every frame.
		float fixedDeltaSeconds() const { return m_fixedDt; }

		// How far the accumulator sits between the last completed step and
		// the next one, in [0, 1]. Multiply positions by it at render time:
		//     drawn = previous + (current - previous) * alpha
		// Latched by the consumeStep() that returns false, so it is valid
		// from the end of the step loop until the next beginFrame().
		float alpha() const { return m_alpha; }

		// Diagnostics for the frame that just ran its steps.
		int stepsLastFrame() const { return m_steps; }
		int droppedStepsLastFrame() const { return m_dropped; }
		float accumulatorSeconds() const { return m_accumulator; }

		// Clamp frameSeconds to config().maxFrameSeconds and add it to the
		// accumulator. Resets the per-frame step and drop counters.
		void beginFrame(float frameSeconds);

		// Loop condition. Returns true (and debits one step of accumulated
		// time) while a step is owed and the catch-up budget is unspent.
		// The call that returns false drains the unrunnable surplus and
		// latches alpha().
		bool consumeStep();

		// Zero the accumulator and the counters without re-validating the
		// config. Use after a load, a teleport, or anything else that makes
		// the pending game time meaningless.
		void reset();

	private:
		FixedTimestepConfig m_cfg{};

		float m_fixedDt = 1.0f / 60.0f;
		float m_accumulator = 0.0f;
		float m_alpha = 0.0f;

		int m_steps = 0;
		int m_dropped = 0;
	};
}
```

> **PASTE NOTE.** `HBE.Core` is tab-indented — `Application.h`,
> `Layer.h`, `Profiler.h` all use hard tabs, and so does this
> file. If your editor is set to spaces for `.h` files, the
> file will still compile, but it will not match its
> neighbours. The block above uses tabs.

---

## 3. Reading the API before you implement it

Three things about this surface are easy to get wrong in
doc `02`, so fix them in your head now.

### 3.1 `fixedDeltaSeconds()` is not `dt`

`dt` is how long the last frame took. `fixedDeltaSeconds()`
is a constant. On a machine running at 300 FPS with `hz =
60`, `dt` is about `0.0033` and `fixedDeltaSeconds()` is
exactly `0.016666...` on every single call. Most frames will
run **zero** steps. That is correct and expected — the
accumulator is just filling up.

### 3.2 `alpha()` is latched, not computed on read

It would be tempting to make `alpha()` compute
`m_accumulator / m_fixedDt` on the fly. Don't. The
accumulator is a live value that `beginFrame` mutates, so a
computed `alpha()` would silently change meaning depending
on where in the frame you asked. Latching it once, in the
`consumeStep()` that ends the loop, gives it a single
well-defined moment of validity.

### 3.3 `droppedStepsLastFrame()` reads 0 almost always

With the default config it is non-zero only on the frame
that recovers from a stall longer than about 83 ms. If you
see it non-zero every frame, the machine genuinely cannot
sustain `hz` and the tuning table in doc `07` is where you
go.

---

## 4. What NOT to touch

* **Do not add an `enabled` or `paused` flag.** Golden rule
  3. Pausing is a game concern — a paused `GameLayer` returns
  early from its own `onFixedUpdate`, which keeps the pause
  visible in game code where a reader will look for it.
* **Do not make `hz` a `double`.** Every other time value
  crossing the layer boundary in this engine is a `float`
  (`onUpdate(float dt)`, `Audio::update(float)`,
  `FileWatcher::poll(float)`). One `double` here would mean a
  narrowing conversion at the `onFixedUpdate` call site and
  nothing gained: at 60 Hz a `float` step is exact to about
  a nanosecond, and the value is never accumulated in
  `float` across frames — `m_accumulator` only ever holds
  less than one frame plus one step.
* **Do not add a `Application*` or any back-pointer.** This
  class is used by `Application` and tested standalone; a
  back-pointer would make it untestable and would create the
  include cycle `Application.h -> Timestep.h -> Application.h`.
* **Do not put the step loop in this header.** The loop
  needs the layer stack. It lives in `Application::run`
  (doc `04`).

---

## 5. Sanity check before doc `02`

```fish
test -f /home/atulo/Projects/HBE/HBE.Core/include/HBE/Core/Timestep.h; echo $status
# -> 0

grep -c '#include' /home/atulo/Projects/HBE/HBE.Core/include/HBE/Core/Timestep.h
# -> 0   (the header includes nothing — see §1)

grep -c 'pragma once' /home/atulo/Projects/HBE/HBE.Core/include/HBE/Core/Timestep.h
# -> 1

grep -n 'void configure\|bool consumeStep\|void beginFrame\|float alpha\|void reset' \
    /home/atulo/Projects/HBE/HBE.Core/include/HBE/Core/Timestep.h
# -> 5 lines
```

**Should the project compile now?** Yes, and nothing about
the build changes. No `.cpp` includes this header yet and no
`CMakeLists.txt` mentions it, so a build at this point is
identical to a build before you started:

```fish
cd /home/atulo/Projects/HBE
cmake --build --preset linux-clang-debug --target MegaX
```

If that fails, the failure is not from this doc.

Next: `02_timestep_impl.md`.
