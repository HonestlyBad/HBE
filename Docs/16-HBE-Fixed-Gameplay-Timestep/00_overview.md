# 16 — [HBE] Add a Fixed Gameplay Timestep (overview)

Item 15 gave us a CSV that makes two builds comparable.
It also exposed the thing that makes those comparisons a
lie: **MegaX simulates on the render clock.** Every physics
integration, every AI tick, every bullet step uses the wall
time of the last rendered frame. Run the same room at 300
FPS in Debug and at 60 FPS with vsync and you get two
different games — different jump apexes, different bullet
travel per frame, different knockback distances. A capture
of one tells you nothing about the other.

Item 16 splits the two clocks. Gameplay advances in
**fixed-size steps** at a configurable rate (60 Hz by
default), driven by an accumulator that the render loop
feeds. Rendering keeps running as fast as it can and
smooths over the gap with an **interpolation alpha**. A
layer opts in by overriding one new virtual; a layer that
doesn't override it behaves exactly as it did yesterday.

Per the work-item tag `[HBE]`, the deliverable lives under
`/home/atulo/Projects/HBE/HBE.Core/`. `HBE.Platform.SDL/`
and `HBE.Renderer.GL/` are not touched. MegaX **is** edited,
in docs `05` and `06` — see the scope note at the end of §1
for exactly what kind of edit that is. `HBE.Sandbox/` and
`HBMapMaker/` are not touched; they keep compiling because
every engine addition is additive.

---

## 1. What Item 16 delivers

### HBE.Core — the accumulator itself

* A new header `HBE/Core/Timestep.h` and a matching
  `src/Core/Timestep.cpp`.
* `HBE::Core::FixedTimestepConfig` — the three knobs:
  * `float hz = 60.0f` — simulation rate. Clamped to
    `[1, 1000]` in `configure()`.
  * `int maxCatchUpSteps = 5` — the most fixed steps one
    rendered frame may run. Clamped to `>= 1`.
  * `float maxFrameSeconds = 0.25f` — the longest single
    frame that may be fed into the accumulator. Anything
    longer is truncated before it is added.
* `HBE::Core::FixedTimestep` — the accumulator. Its whole
  public surface:
  * `void configure(const FixedTimestepConfig&)` — validate,
    recompute `1/hz`, reset.
  * `const FixedTimestepConfig& config() const`
  * `float fixedDeltaSeconds() const` — the `h` every step
    is given. Never changes between `configure()` calls.
  * `void beginFrame(float frameSeconds)` — clamp and
    accumulate, once per rendered frame.
  * `bool consumeStep()` — the loop condition. Returns
    `true` and debits one step's worth of time while a step
    is owed and the budget is unspent; on the call that
    returns `false` it drains any unrunnable surplus and
    latches `alpha`.
  * `float alpha() const` — `accumulator / fixedDeltaSeconds`
    after the step loop, in `[0, 1]`. This is the number a
    renderer lerps with.
  * `int stepsLastFrame() const`, `int droppedStepsLastFrame()
    const`, `float accumulatorSeconds() const` — diagnostics.
  * `void reset()` — zero the accumulator without
    re-validating the config.

### HBE.Core — the layer lifecycle

* `HBE::Core::Layer` gains one virtual:

  ```cpp
  virtual void onFixedUpdate(float fixedDt) { (void)fixedDt; }
  ```

  It has a body, so **no existing layer changes and no
  existing layer recompiles differently.** `HBE.Sandbox`'s
  `GameLayer` does not override it and keeps its current
  behavior exactly.
* `onAttach` / `onUpdate` / `onRender` / `onDetach` /
  `onEvent` keep their signatures and their call order.

### HBE.Core — Application

* `Application` owns a `FixedTimestep m_timestep`.
* New public surface:
  * `void setFixedTimestep(const FixedTimestepConfig&)` —
    reconfigure at any time; logs the resolved values.
  * `const FixedTimestep& timestep() const`
  * `float fixedDeltaSeconds() const`
  * `float interpolationAlpha() const` — valid from the
    moment the step loop ends until the next frame's step
    loop, which means **it is valid for the whole of
    `onRender`** and stale during `onUpdate`.
  * `void setTargetFrameRate(float hz)` / `float
    targetFrameRate() const` — an optional render-rate cap.
    `0` (the default) means uncapped and preserves today's
    `delayMillis(1)` behavior byte for byte.
* `Application::run`'s loop gains one block between the
  existing `ApplicationUpdate` scope and the existing
  `Audio` scope:

  ```
  onUpdate(dt) for every layer          <- unchanged, still first
  FixedUpdate  { 0..N x onFixedUpdate(h) for every layer }   <- NEW
  Audio        { m_audio.update(dt) }   <- unchanged
  ```

  and the trailing `m_platform.delayMillis(1)` becomes the
  frame limiter.
* The new block is wrapped in `HBE_PROFILE_SCOPE("FixedUpdate")`,
  so item 13's profiler and item 15's CSV pick it up with no
  further work.

### MegaX — moving the simulation onto the fixed clock

* `GameLayer::onUpdate` keeps input sampling, the F-key
  block, animated tiles, particles, the camera, and the
  perf-capture tick. `GameLayer::onFixedUpdate` gets the
  `Physics`, `Combat` and `AI` blocks verbatim.
* `Player::update` splits into `Player::fixedUpdate(h)`
  (timers, ghost/play integration, latched-intent consume)
  and `Player::updateVisual(dt)` (tint, animation advance).
* `Enemy::tick` becomes `Enemy::fixedTick`, with its two
  `currentAnim().update(dt)` calls moved into a new
  `Enemy::updateVisual(dt)`.
* `Player`, `Enemy`, `BulletManager::Bullet` and
  `EnemyBulletManager::Bullet` each keep a previous-step
  position, and their `render` methods take the
  interpolation alpha and lerp with it.
* `main.cpp` learns `--fixed-hz`, `--fixed-catchup` and
  `--render-hz`, so the done criterion can be tested from
  the shell without a rebuild.

> **NOTE ON `[HBE]` SCOPE.** The engine changes above are
> the whole deliverable and they are **source compatible**:
> `onFixedUpdate` is a new virtual with a body, and every
> new `Application` method is additive. MegaX compiles
> untouched against the new engine, and if it did nothing
> else it would run exactly as it does today. So docs `05`
> and `06` are **not** "required — API changed".
>
> They are also not skippable. The item's done criterion is
> *"player movement and combat simulation produce nearly
> identical results at 30, 60, 120, and uncapped rendering
> rates"*, and nothing in `HBE.Core/` can satisfy that on
> its own — a game has to actually put its simulation on the
> fixed clock. Docs `05` and `06` are the **demonstration
> that closes the item**: optional to compile, mandatory to
> finish. They add no game logic to HBE and no MegaX header
> becomes visible to any `HBE.*` target.

---

## 2. Success criteria

Item 16 is complete when **all** of the following hold:

1. `cmake --build --preset linux-clang-debug` builds
   `HBE.Core`, `HBE.Sandbox` and `MegaX` with no new
   warnings.
2. `cmake --build --preset linux-clang-release` builds the
   same three targets. The fixed step is **not** a debug
   feature and is not compiled out by `NDEBUG`.
3. MegaX logs the resolved timestep once at startup:

   ```
   [INFO]Application: fixed timestep 60.00 Hz (0.0167 s), max 5 catch-up steps, frame clamp 0.250 s.
   ```

4. A `FixedUpdate` section appears in the profiler, as a
   sibling of `ApplicationUpdate`, and `Physics`, `Combat`
   and `AI` move from depth 2 under `SceneUpdate` to depth 1
   under it. In the 1-Hz log, read the **indentation**, not
   the order — the three simulation sections register a
   frame late (frame 1 runs zero steps) and therefore print
   last:

   ```
   [INFO]  ApplicationUpdate  cur=  0.01 avg=  0.02 ... (n=58)
   [INFO]    SceneUpdate        cur=  0.01 ...          (n=58)
   [INFO]      Particles          cur=  0.00 ...        (n=58)
   [INFO]  FixedUpdate        cur=  0.01 ...            (n=58)
   [INFO]  Audio              cur=  0.00 ...            (n=58)
   [INFO]  GameRender         cur=  0.25 ...            (n=58)
   [INFO]    TileRendering      cur=  0.09 ...          (n=58)
   [INFO]    SpriteRendering    cur=  0.12 ...          (n=58)
   [INFO]    Physics            cur=  0.00 ...          (n=57)
   [INFO]    Combat             cur=  0.00 ...          (n=57)
   [INFO]    AI                 cur=  0.01 ...          (n=57)
   ```

5. With vsync off and no render cap, `FixedUpdate` runs
   **0 steps on most frames** — the render loop is far
   faster than 60 Hz, so most frames only accumulate. Over
   six seconds of runtime the total step count is
   `6 x hz` ± 1, whether the renderer managed 30 FPS or 760.
6. **The done criterion.** Four hands-off runs of the demo
   room — `--render-hz 30`, `--render-hz 60`,
   `--render-hz 120`, and uncapped — end with the player and
   the enemy at the **same** world positions. Measured on
   this machine they agree to four decimal places across a
   25x spread in render rate. Doc `07` §6 gives the probe,
   the commands and the numbers.
7. `--fixed-hz 30` visibly halves the simulation rate
   (`stepsLastFrame` halves) while animation and particles
   keep running at the render rate. The game is still
   playable and the player still lands on the same tile.
8. A deliberate 2-second stall (drag the window title bar)
   does not teleport the player across the room and does
   not lock the loop up afterwards. `droppedStepsLastFrame()`
   is non-zero for exactly the frame that recovers.
9. Item 15's capture still works: `--capture --no-vsync`
   writes a 27-column CSV, and `physicsMs` / `combatMs` /
   `aiMs` are still populated.
10. Item 11's hot reload still works: `F5` reloads the map,
    `F7` soft-respawns, `F6` reloads the shader, and the
    player does not streak across the screen on the reload
    frame.

---

## 3. Golden rules (read before touching code)

1. **`onUpdate` runs before the fixed steps, not after.**
   This is the single most consequential ordering decision
   in the item, and it is deliberate. Input is polled at the
   top of the frame; layers sample it in `onUpdate` and
   latch intents. If the step loop ran first, every fixed
   step would be consuming input that was polled **one whole
   frame earlier** — a guaranteed extra frame of input
   latency in a precision platformer. Running `onUpdate`
   first costs one frame of staleness in the camera and the
   animation instead, which is invisible behind a lerped
   camera and a 14-fps sprite sheet. The trade is not close.

2. **A layer that ignores `onFixedUpdate` must be
   bit-identical to before.** That is why the virtual has a
   body, why `onUpdate` keeps its slot and its `dt`, and why
   the existing `dt > 0.25f` clamp stays exactly where it
   is. `HBE.Sandbox` is the test: it is never edited in this
   item and must still run.

3. **There is no `enabled` flag on the timestep, and that is
   on purpose.** An off switch would mean `onFixedUpdate`
   silently never fires, which turns "I disabled the fixed
   step" into "my game froze" with no diagnostic. A game
   that wants variable-rate behavior simply does not
   override `onFixedUpdate`. `hz` is the only rate knob.

4. **The accumulator clamps the frame, not the steps.**
   `beginFrame` truncates a long frame to `maxFrameSeconds`
   *before* adding it. `maxCatchUpSteps` then bounds how
   much of the accumulator one frame may spend. These two
   are independent on purpose, and the relationship between
   them is the whole hitch-recovery policy:
   * `maxFrameSeconds > maxCatchUpSteps / hz` (the default:
     0.25 vs 0.083) — a hitch **skips** game time. The
     surplus is dropped and counted in
     `droppedStepsLastFrame()`. The world jumps forward.
   * `maxFrameSeconds == maxCatchUpSteps / hz` — a hitch
     **dilates** game time. Nothing is ever dropped; the
     simulation just runs in slow motion until the render
     loop catches up.
   Both are defensible. The default is skip, because that is
   closest to what MegaX does today. Doc `07`'s tuning table
   says how to switch.

5. **Never let the accumulator grow without bound.** The
   call to `consumeStep()` that returns `false` drains
   everything past one step's worth. Without that drain, a
   machine that cannot hit `hz` accumulates debt forever,
   runs `maxCatchUpSteps` every frame, gets slower because
   of it, and accumulates faster — the spiral of death. The
   drain is three lines and it is the reason the loop is
   safe to ship.

6. **`alpha` is a render-time value.** It is latched when
   the step loop ends and it is stale during `onUpdate`.
   Read it in `onRender` via `Application::interpolationAlpha()`
   and nowhere else. A layer that lerps in `onUpdate` will
   look right at 60 Hz and wrong everywhere else, which is
   the worst possible failure mode because it passes the
   casual test.

7. **Interpolate positions, never simulate with alpha.**
   `alpha` may only feed presentation: the transform a
   renderer writes. It must never feed a collision query, a
   hit test, a timer, or an AI decision. If it does, the
   simulation is back on the render clock and the item has
   achieved nothing.

8. **Every teleport must set the previous position too.**
   `Player::setPosition`, `Enemy::spawn`, and both bullet
   `spawn` methods write `prev` and `current` to the same
   value. Miss one and the entity draws a smear from its old
   location to its new one for exactly one frame — which is
   what you will see on `F5` if you skip this.

9. **Animation and visual-only particles stay on the render
   clock.** The item says so explicitly. `Effects::update`,
   `World::update` (animated tiles), `SpriteAnimation::update`
   and every tint calculation stay in `onUpdate` with the
   real `dt`. At `--fixed-hz 15` the game should crawl and
   the torches should still flicker smoothly.

10. **The frame limiter sleeps, it does not spin.** A
    busy-wait would hit the target rate more precisely and
    would also burn a core and poison every item-15 capture
    taken with a cap on. Sleep-only leaves a millisecond or
    two of jitter in the render rate, and absorbing exactly
    that kind of jitter is what this entire item is for.

11. **`Timestep.h` stays dependency-free.** No `Log.h`, no
    `<vector>`, no SDL. It is pulled into `Application.h`,
    which every layer in every game already includes —
    the same discipline `Profiler.h` follows. Validation
    that wants to complain does it in `Application`, which
    already has a logger.

---

## 4. Files touched

| File | Item 15 | Item 16 |
|---|---|---|
| `HBE.Core/include/HBE/Core/Timestep.h` | — | **NEW** — `FixedTimestepConfig`, `FixedTimestep` |
| `HBE.Core/src/Core/Timestep.cpp` | — | **NEW** — validation, accumulate, step, drain, alpha |
| `HBE.Core/CMakeLists.txt` | source list | **EDIT** — add `src/Core/Timestep.cpp` |
| `HBE.Core/include/HBE/Core/Layer.h` | 5 virtuals | **EDIT** — add `onFixedUpdate(float)` |
| `HBE.Core/include/HBE/Core/Application.h` | window, layers, loop | **EDIT** — include `Timestep.h`; 6 new methods, 3 new members |
| `HBE.Core/src/Core/Application.cpp` | main loop | **EDIT** — `setFixedTimestep`, `setTargetFrameRate`, step loop in `run()`, frame limiter |
| `MegaX/include/Game/Player.h` | — | **EDIT** *(demonstration)* — split `update`, `render` takes alpha, prev position |
| `MegaX/src/Game/Player.cpp` | — | **EDIT** *(demonstration)* — `fixedUpdate` / `updateVisual` / lerped `render` |
| `MegaX/include/Game/Enemy.h` | — | **EDIT** *(demonstration)* — `tick`→`fixedTick`, `updateVisual`, prev position |
| `MegaX/src/Game/Enemy.cpp` | — | **EDIT** *(demonstration)* — same split |
| `MegaX/include/Game/EnemyManager.h` | — | **EDIT** *(demonstration)* — `update`→`fixedUpdate` + `updateVisual` |
| `MegaX/src/Game/EnemyManager.cpp` | — | **EDIT** *(demonstration)* — same split |
| `MegaX/include/Game/Bullet.h` | — | **EDIT** *(demonstration)* — `Bullet::px/py`, `render` takes alpha |
| `MegaX/src/Game/Bullet.cpp` | — | **EDIT** *(demonstration)* — record prev, lerp on draw |
| `MegaX/include/Game/EnemyBullet.h` | — | **EDIT** *(demonstration)* — same |
| `MegaX/src/Game/EnemyBullet.cpp` | — | **EDIT** *(demonstration)* — same |
| `MegaX/include/Game/GameLayer.h` | perf capture hook | **EDIT** *(demonstration)* — `onFixedUpdate` override |
| `MegaX/src/Game/GameLayer.cpp` | one `onUpdate` | **EDIT** *(demonstration)* — split into `onUpdate` + `onFixedUpdate`; `onRender` passes alpha |
| `MegaX/src/main.cpp` | capture args | **EDIT** *(demonstration)* — `--fixed-hz`, `--fixed-catchup`, `--render-hz` |
| `MegaX/src/Game/PerfCapture.cpp` | arg parsing + usage | **EDIT** *(demonstration)* — three lines in `PrintCaptureUsage()` |

**Not touched:** `HBE.Platform.SDL/` (any file),
`HBE.Renderer.GL/` (any file), `HBE.Core/include/HBE/Core/Time.h`,
`HBE.Core/src/Core/Time.cpp`, `HBE.Core/include/HBE/Core/Profiler.h`,
`HBE.Core/src/Core/Profiler.cpp`, `HBE.Core/src/Core/LayerStack.cpp`,
`MegaX/include/Game/PerfCapture.h`, `MegaX/include/Game/Effects.h`,
`MegaX/src/Game/Effects.cpp`, `MegaX/include/World/World.h`,
`MegaX/src/World/World.cpp`, `MegaX/CMakeLists.txt` (no new
`.cpp` on the game side), `HBE.Sandbox/`, `HBMapMaker/`.

---

## 5. Design shape

### The accumulator, in full

```cpp
namespace HBE::Core {

	struct FixedTimestepConfig {
		float hz              = 60.0f;   // simulation rate
		int   maxCatchUpSteps = 5;       // steps one frame may run
		float maxFrameSeconds = 0.25f;   // longest frame fed to the accumulator
	};

	class FixedTimestep {
	public:
		void configure(const FixedTimestepConfig& cfg);
		const FixedTimestepConfig& config() const { return m_cfg; }

		float fixedDeltaSeconds() const { return m_fixedDt; }
		float alpha() const { return m_alpha; }

		int   stepsLastFrame() const { return m_steps; }
		int   droppedStepsLastFrame() const { return m_dropped; }
		float accumulatorSeconds() const { return m_accumulator; }

		void beginFrame(float frameSeconds);
		bool consumeStep();
		void reset();

	private:
		FixedTimestepConfig m_cfg{};
		float m_fixedDt    = 1.0f / 60.0f;
		float m_accumulator = 0.0f;
		float m_alpha       = 0.0f;
		int   m_steps       = 0;
		int   m_dropped     = 0;
	};
}
```

### Why `consumeStep()` and not "give me a step count"

The obvious API is `int beginFrame(float dt)` returning how
many steps to run, and it is worse for one specific reason:
the caller then owns the accumulator arithmetic, and the
caller is `Application::run`, which already carries the
loop. Two places would have to agree on when to subtract
`h`, when to drain, and when to latch `alpha`.

`consumeStep()` keeps all of that inside the class and
reduces the call site to a `while`:

```cpp
m_timestep.beginFrame(dt);
const float h = m_timestep.fixedDeltaSeconds();
while (m_timestep.consumeStep()) {
    for (auto& layer : m_layers.m_layers)
        if (layer) layer->onFixedUpdate(h);
}
```

The subtlety worth knowing: the **falsy** call is not a
no-op. That is where the surplus drain and the `alpha`
latch happen. `beginFrame` without a terminating
`consumeStep()` leaves `alpha` at last frame's value.

### The loop, before and after

```
                    item 15                     item 16
  ---------------------------------------------------------------
  Profiler::BeginFrame        Profiler::BeginFrame
  GpuTimer::NewFrame          GpuTimer::NewFrame
  Input::NewFrame             Input::NewFrame
  pumpEvents                  pumpEvents
  dt = now - prev (clamp)     dt = now - prev (clamp)      unchanged
  [ApplicationUpdate]         [ApplicationUpdate]          unchanged
    layer->onUpdate(dt)         layer->onUpdate(dt)
                              [FixedUpdate]                NEW
                                beginFrame(dt)
                                while (consumeStep())
                                  layer->onFixedUpdate(h)
  [Audio]                     [Audio]                      unchanged
  resetFrameStats             resetFrameStats
  beginFrame*                 beginFrame*
    layer->onRender()           layer->onRender()          alpha valid here
  gl.endFrame                 gl.endFrame
  PublishRendererStats        PublishRendererStats
  Profiler::EndFrame          Profiler::EndFrame
  1-Hz log block              1-Hz log block
  delayMillis(1)              frame limiter or delayMillis(1)
```

### What each entity keeps

Interpolation needs exactly two numbers per entity and one
rule. The numbers are the position at the start of the
current fixed step and the position after it. The rule is
that every teleport writes both.

| Entity | Previous | Current | Written by |
|---|---|---|---|
| `Player` | `m_prevX`, `m_prevY` | `m_x`, `m_y` | `fixedUpdate` head, `setPosition` |
| `Enemy` | `m_prevX`, `m_prevY` | `m_x`, `m_y` | `fixedTick` head, `spawn` |
| `BulletManager::Bullet` | `px`, `py` | `x`, `y` | `update` per-bullet head, `spawn` |
| `EnemyBulletManager::Bullet` | `px`, `py` | `x`, `y` | `update` per-bullet head, `spawn` |

and every `render` becomes

```cpp
const float t = clamp01(alpha);
posX = prevX + (curX - prevX) * t;
posY = prevY + (curY - prevY) * t;
```

Nothing else in the entity changes. Velocity, collision,
hitboxes and AI all keep reading `m_x` / `m_y`, which is
the authoritative simulated position — the interpolated
value never leaves the transform.

### Where MegaX's work lands

| Was, in `onUpdate` | Goes to | Why |
|---|---|---|
| `tickPerfCapture` | `onUpdate` | measures rendered frames |
| `m_watcher.poll` | `onUpdate` | file I/O, wall-clock |
| input reads + `set*Input` | `onUpdate` | must see this frame's input |
| the F-key block | `onUpdate` | one press = one action |
| `m_world.update` | `onUpdate` | animated tiles are animation |
| `Physics` scope | `onFixedUpdate` | simulation |
| `Combat` scope | `onFixedUpdate` | simulation |
| landing-dust dispatch | `onFixedUpdate` | reads `landedThisFrame()` |
| `AI` scope | `onFixedUpdate` | simulation |
| `tickWalkDust` | `onUpdate` | cosmetic emitter cadence |
| camera follow + `setCamera` | `onUpdate` | presentation, already lerped |
| `Particles` scope | `onUpdate` | item forbids fixing particles |
| `PublishParticleStats` | `onUpdate` | per rendered frame |

---

## 6. Non-goals (deferred)

| Behavior | Deferred to |
|---|---|
| Deterministic RNG so two runs match exactly, not just closely | Item 17 — `RandomStream` |
| A repeatable input script / replay to drive the 30/60/120 comparison automatically | **Not scheduled.** Doc `07` §6 uses a hand-driven procedure |
| Rotation and scale interpolation | **Never needed so far** — no MegaX entity rotates during simulation except enemy bullets, whose angle comes from a constant velocity |
| Rolling back and re-simulating (netcode-style) | **Never** — single-player game, out of scope for the engine |
| Publishing `stepsLastFrame` as a CSV column | Item 15's format is frozen; adding a column is the four-step recipe in `Docs/15-*/05_capture_csv_format.md` §"Adding a column later" |
| Fixed-rate audio or a fixed-rate `Audio` update | **Never** — SDL_mixer runs on its own thread clock |
| A golden room that makes the 30/60/120 comparison meaningful at real density | Item 20 |
| Interpolating `Effects` particles | **Never** — they are explicitly render-clocked by this item's own rules |

---

## 7. Reading order

1. `01_timestep_api.md` — create `HBE/Core/Timestep.h`. Pure
   header, no build change yet.
2. `02_timestep_impl.md` — create `src/Core/Timestep.cpp`
   and register it in `HBE.Core/CMakeLists.txt`. `HBE.Core`
   compiles and links after this doc.
3. `03_layer_and_application_api.md` — `onFixedUpdate` on
   `Layer`, the new accessors and members on `Application`.
   Still builds and still runs unchanged: the two
   out-of-line methods are declared here and defined in
   doc `04`, and nothing calls them in between.
4. `04_application_loop_wiring.md` — `setFixedTimestep`,
   `setTargetFrameRate`, the step loop, the frame limiter.
   The whole engine builds and MegaX still runs, unchanged.
5. `05_megax_entities_fixed_update.md` — `Player`, `Enemy`,
   `EnemyManager`, both bullet managers. MegaX does **not**
   compile in the middle of this doc; doc `06` fixes the
   call sites.
6. `06_megax_gamelayer_and_main.md` — the `GameLayer` split,
   `onRender` alpha, the three new command-line flags.
   Everything compiles and runs again.
7. `07_build_run_and_verify.md` — build both configs, the
   verify checklist, the tuning table, the troubleshooting
   matrix, and the done-criterion procedure.

> Do them in that order. Each doc lists **exact** file
> paths, exact insertion points, and full copy-paste code
> blocks.

Next: `01_timestep_api.md`.
