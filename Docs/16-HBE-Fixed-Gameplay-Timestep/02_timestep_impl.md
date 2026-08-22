# 16 — The fixed timestep implementation (`src/Core/Timestep.cpp`)

This doc creates `HBE.Core/src/Core/Timestep.cpp` and adds
one line to `HBE.Core/CMakeLists.txt`. It depends on doc
`01`'s header existing. Nothing else in the engine changes.

At the end of this doc `HBE.Core` compiles and links with
the new translation unit in it, and the whole project still
builds. The class is not called from anywhere yet — that is
doc `04`.

---

## 1. Create the file

**Full path:** `/home/atulo/Projects/HBE/HBE.Core/src/Core/Timestep.cpp`

**Status:** file must not exist yet.

```fish
test -f /home/atulo/Projects/HBE/HBE.Core/src/Core/Timestep.cpp; echo $status
# -> 1   (does not exist — good)
```

Paste the following content **verbatim**:

```cpp
#include "HBE/Core/Timestep.h"

namespace HBE::Core {

	// Below 1 Hz the accumulator holds more than a second of pending game
	// time and every step integrates a second of gravity in one go, which
	// tunnels through any tilemap. Above 1000 Hz a float step loses enough
	// precision that the catch-up budget stops being meaningful. Neither
	// bound is a real use case; they exist so a typo in a command-line
	// argument degrades instead of exploding.
	static constexpr float kMinHz = 1.0f;
	static constexpr float kMaxHz = 1000.0f;

	void FixedTimestep::configure(const FixedTimestepConfig& cfg) {
		m_cfg = cfg;

		// Written as "not greater than" rather than "less than or equal" so
		// a NaN -- which compares false against everything -- lands here
		// instead of sailing through into a division.
		if (!(m_cfg.hz > 0.0f)) m_cfg.hz = 60.0f;
		if (m_cfg.hz < kMinHz)  m_cfg.hz = kMinHz;
		if (m_cfg.hz > kMaxHz)  m_cfg.hz = kMaxHz;

		if (m_cfg.maxCatchUpSteps < 1) m_cfg.maxCatchUpSteps = 1;

		m_fixedDt = 1.0f / m_cfg.hz;

		// A frame clamp below one step would mean the accumulator can never
		// fill and no step would ever run: a frozen game with a running
		// renderer. Floor it at one step's worth.
		if (!(m_cfg.maxFrameSeconds > 0.0f)) m_cfg.maxFrameSeconds = 0.25f;
		if (m_cfg.maxFrameSeconds < m_fixedDt) m_cfg.maxFrameSeconds = m_fixedDt;

		reset();
	}

	void FixedTimestep::reset() {
		m_accumulator = 0.0f;
		m_alpha = 0.0f;
		m_steps = 0;
		m_dropped = 0;
	}

	void FixedTimestep::beginFrame(float frameSeconds) {
		m_steps = 0;
		m_dropped = 0;

		// NaN-safe, negative-safe: a monotonic clock should never hand us
		// either, but Application::run computes dt as a subtraction of two
		// doubles narrowed to float, and a machine coming back from suspend
		// has produced negative deltas before.
		if (!(frameSeconds > 0.0f)) frameSeconds = 0.0f;

		if (frameSeconds > m_cfg.maxFrameSeconds) {
			frameSeconds = m_cfg.maxFrameSeconds;
		}

		m_accumulator += frameSeconds;
	}

	bool FixedTimestep::consumeStep() {
		if (m_accumulator >= m_fixedDt && m_steps < m_cfg.maxCatchUpSteps) {
			m_accumulator -= m_fixedDt;
			++m_steps;
			return true;
		}

		// The loop is over for this frame. Whatever is still owed beyond a
		// single step is time this machine is never going to simulate --
		// drop it and count it, rather than carrying the debt into the next
		// frame where it makes the same frame longer and the debt bigger.
		// This drain is what keeps the loop out of the spiral of death.
		while (m_accumulator >= m_fixedDt) {
			m_accumulator -= m_fixedDt;
			++m_dropped;
		}

		// Latch the interpolation factor for this frame's rendering. After
		// the drain above, m_accumulator is strictly less than m_fixedDt,
		// so this is already in [0, 1); the clamps are belt and braces
		// against a denormal m_fixedDt.
		m_alpha = m_accumulator / m_fixedDt;
		if (!(m_alpha > 0.0f)) m_alpha = 0.0f;
		if (m_alpha > 1.0f) m_alpha = 1.0f;

		return false;
	}
}
```

> **PASTE NOTE.** Tabs, matching every other file in
> `HBE.Core/src/Core/`. The file includes exactly one header
> and no standard library header — `!(x > 0.0f)` does the
> NaN work that `std::isnan` would otherwise pull `<cmath>`
> in for.

---

## 2. Register the source in `HBE.Core/CMakeLists.txt`

Open `/home/atulo/Projects/HBE/HBE.Core/CMakeLists.txt`.

The `add_library` block currently reads (lines 2-15):

```cmake
add_library(HBE.Core STATIC
    src/Core/Application.cpp
    src/Core/AssetPaths.cpp
    src/Core/EventBus.cpp
    src/Core/FileWatcher.cpp
    src/Core/LayerStack.cpp
    src/Core/Log.cpp
    src/Core/Profiler.cpp
    src/Core/Time.cpp
    src/Core/UUID.cpp
    src/ECS/CombatEvents.cpp
    src/ECS/CombatSystem.cpp
    src/Input/InputMap.cpp
)
```

The list is alphabetical within each subdirectory group, so
`Timestep.cpp` goes between `src/Core/Time.cpp` (line 10)
and `src/Core/UUID.cpp` (line 11). Insert right after
`src/Core/Time.cpp`:

```cmake
    src/Core/Timestep.cpp
```

> **NOTE ON ORDER.** `Time.cpp` sorts before `Timestep.cpp`
> because `Time.` < `Times` — the `.` (0x2E) is below `s`
> (0x73). It looks wrong at a glance and it is right.

The final block should read:

```cmake
add_library(HBE.Core STATIC
    src/Core/Application.cpp
    src/Core/AssetPaths.cpp
    src/Core/EventBus.cpp
    src/Core/FileWatcher.cpp
    src/Core/LayerStack.cpp
    src/Core/Log.cpp
    src/Core/Profiler.cpp
    src/Core/Time.cpp
    src/Core/Timestep.cpp
    src/Core/UUID.cpp
    src/ECS/CombatEvents.cpp
    src/ECS/CombatSystem.cpp
    src/Input/InputMap.cpp
)
```

4-space indentation, one file per line, no globbing —
`CMakeLists.txt` files in this repo are 4-space regardless
of the source style of the target they build.

Do **not** add `include/HBE/Core/Timestep.h` to this list.
Headers are never listed; they resolve through the existing
`target_include_directories(HBE.Core PUBLIC .../include)`.

---

## 3. Walking the algorithm once

Worth doing on paper before you trust it. Take the defaults
— `hz = 60` so `h = 0.016667`, `maxCatchUpSteps = 5`,
`maxFrameSeconds = 0.25` — and a machine rendering at 300
FPS, so `dt = 0.00333` every frame.

| Frame | acc before | after `beginFrame` | steps | acc after | alpha |
|---|---|---|---|---|---|
| 1 | 0.00000 | 0.00333 | 0 | 0.00333 | 0.20 |
| 2 | 0.00333 | 0.00667 | 0 | 0.00667 | 0.40 |
| 3 | 0.00667 | 0.01000 | 0 | 0.01000 | 0.60 |
| 4 | 0.01000 | 0.01333 | 0 | 0.01333 | 0.80 |
| 5 | 0.01333 | 0.01667 | **1** | 0.00000 | 0.00 |
| 6 | 0.00000 | 0.00333 | 0 | 0.00333 | 0.20 |

One step every five frames — 60 simulation steps per second
against 300 rendered frames — and `alpha` sweeping 0 → 0.8
between them. That sweep is what makes an entity drawn at
`prev + (cur - prev) * alpha` appear to move smoothly at 300
FPS while only actually moving 60 times a second.

Now the stall case. Same config, a frame that took 2.0
seconds:

| Step | Value |
|---|---|
| `frameSeconds` in | `2.000000` |
| clamped to `maxFrameSeconds` | `0.250000` |
| accumulator after `beginFrame` | `0.250000` |
| steps run (budget is 5) | `5` → consumes `0.083333` |
| accumulator at the drain | `0.166667` |
| dropped in the drain | `10` |
| accumulator after the drain | `0.000000` |
| `alpha` | `0.000000` |

The frame simulated 83 ms of game time, threw away 167 ms,
and left the accumulator empty — so the *next* frame starts
clean instead of inheriting a debt. `stepsLastFrame() == 5`
and `droppedStepsLastFrame() == 10` on exactly that one
frame, and both are back to normal on the next.

If you would rather that stall play out in slow motion with
nothing skipped, set `maxFrameSeconds` to
`maxCatchUpSteps / hz` — `5 / 60 = 0.0833`. Then the clamp
and the budget agree, the drain never fires, and
`droppedStepsLastFrame()` is 0 forever. Doc `07`'s tuning
table has the exact knob.

---

## 4. What NOT to touch

* **Do not "fix" the drain into a carry-over.** Replacing
  the `while` with `m_accumulator = m_fixedDt` (or leaving
  the surplus alone) reintroduces the spiral of death. The
  test is a machine that cannot hit `hz`: with the drain it
  runs slow and steady; without it, the frame time climbs
  until the process is unresponsive.
* **Do not move the `alpha` latch into `beginFrame`.**
  `alpha` describes the state *after* the steps ran. Latched
  at the top of the frame it would describe the previous
  frame, and every interpolated entity would be exactly one
  simulation step behind.
* **Do not add logging here.** Doc `01` §1. `configure()`
  corrects silently; `Application::setFixedTimestep` logs
  the result.
* **Do not include `<algorithm>` for `std::clamp`.** Two
  `if`s do the job, and the `!(x > 0.0f)` spelling this file
  relies on for NaN safety is not what `std::clamp` does.
* **Do not make `configure()` preserve the accumulator.** It
  calls `reset()` on purpose: after an `hz` change the
  pending time is denominated in the old step size and
  carrying it forward runs a partial step at the wrong rate.

---

## 5. Sanity check before doc `03`

```fish
test -f /home/atulo/Projects/HBE/HBE.Core/src/Core/Timestep.cpp; echo $status
# -> 0

grep -c 'src/Core/Timestep.cpp' /home/atulo/Projects/HBE/HBE.Core/CMakeLists.txt
# -> 1

# The drain and the latch both live in consumeStep, not beginFrame:
grep -n 'm_dropped\|m_alpha =' /home/atulo/Projects/HBE/HBE.Core/src/Core/Timestep.cpp
# -> all inside consumeStep() except the two resets in reset()/beginFrame()
```

**Should the project compile now?** Yes. This is a good
place to stop and prove it:

```fish
cd /home/atulo/Projects/HBE
cmake --build --preset linux-clang-debug --target HBE.Core
```

Expect a single new compile line and nothing else:

```
[1/2] Building CXX object HBE.Core/CMakeFiles/HBE.Core.dir/src/Core/Timestep.cpp.o
[2/2] Linking CXX static library ...
```

If ninja says `'src/Core/Timestep.cpp', needed by ..., missing`
the path in `CMakeLists.txt` and the path on disk disagree —
re-check both, then `cmake --preset linux-clang` to
reconfigure.

Nothing calls this class yet, so MegaX's behaviour is
unchanged. Running it now is optional.

Next: `03_layer_and_application_api.md`.
