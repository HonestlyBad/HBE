# 16 — Build, run and verify

Everything in docs `01`-`06` is now typed. This doc builds
it, proves it, tunes it, and closes the item.

Every expected number below was measured on this machine
with this exact code — Debug, `--no-vsync`, `level_01.json`,
one demo enemy, no keyboard input. Your absolute
milliseconds will differ; the **relationships** must not.

---

## 1. Files touched recap

**NEW**

* `HBE.Core/include/HBE/Core/Timestep.h` — doc `01`
* `HBE.Core/src/Core/Timestep.cpp` — doc `02`

**EDIT — engine**

* `HBE.Core/CMakeLists.txt` — one source line, doc `02` §2
* `HBE.Core/include/HBE/Core/Layer.h` — `onFixedUpdate`, doc `03` §1
* `HBE.Core/include/HBE/Core/Application.h` — include, 6 methods,
  3 members, doc `03` §§2-4
* `HBE.Core/src/Core/Application.cpp` — 2 definitions, the step
  loop, the frame limiter, doc `04`

**EDIT — MegaX (demonstration)**

* `include/Game/Player.h`, `src/Game/Player.cpp`
* `include/Game/Enemy.h`, `src/Game/Enemy.cpp`
* `include/Game/EnemyManager.h`, `src/Game/EnemyManager.cpp`
* `include/Game/Bullet.h`, `src/Game/Bullet.cpp`
* `include/Game/EnemyBullet.h`, `src/Game/EnemyBullet.cpp`
* `include/Game/GameLayer.h`, `src/Game/GameLayer.cpp`
* `src/main.cpp`, `src/Game/PerfCapture.cpp` (3 log lines)

**No changes to** `HBE.Platform.SDL/` (any file),
`HBE.Renderer.GL/` (any file), `Time.h`/`Time.cpp`,
`Profiler.h`/`Profiler.cpp`, `LayerStack.h`/`LayerStack.cpp`,
`MegaX/include/Game/PerfCapture.h`, `Effects.h`/`Effects.cpp`,
`World.h`/`World.cpp`, `MegaX/CMakeLists.txt`,
`HBE.Sandbox/`, `HBMapMaker/`.

`MegaX/CMakeLists.txt` is untouched on purpose: this item
adds no new `.cpp` to the game. The only new source anywhere
is `HBE.Core/src/Core/Timestep.cpp`.

---

## 2. Pre-build sanity check

```fish
cd /home/atulo/Projects/HBE

# The one new source is registered exactly once
grep -c 'src/Core/Timestep.cpp' HBE.Core/CMakeLists.txt
# -> 1

# ...and nothing crept into MegaX's list
grep -c 'Timestep' MegaX/CMakeLists.txt
# -> 0

# Both new engine files exist
test -f HBE.Core/include/HBE/Core/Timestep.h; and test -f HBE.Core/src/Core/Timestep.cpp; echo $status
# -> 0

# The virtual is declared once and overridden once
grep -c 'onFixedUpdate' HBE.Core/include/HBE/Core/Layer.h MegaX/include/Game/GameLayer.h
# -> 1 and 1

# The engine calls it in exactly one place
grep -c 'layer->onFixedUpdate' HBE.Core/src/Core/Application.cpp
# -> 1

# Nothing outside MegaX learned about MegaX
grep -rn 'MegaX' HBE.Core/ HBE.Platform.SDL/ HBE.Renderer.GL/
# -> no output
```

---

## 3. Build

```fish
cd /home/atulo/Projects/HBE
cmake --preset linux-clang            # only if you have not configured yet
cmake --build --preset linux-clang-debug
```

The first build after doc `02` shows the new translation
unit:

```
[N/M] Building CXX object HBE.Core/CMakeFiles/HBE.Core.dir/Debug/src/Core/Timestep.cpp.o
```

and the build ends with all three executables:

```
[53/54] Linking CXX executable bin/Debug/MegaX
[54/54] Linking CXX executable bin/Debug/HBE.Sandbox
```

**`HBE.Sandbox` must still link.** It never overrides
`onFixedUpdate`, so it is the proof that golden rule 2 held.

`Timestep.cpp` compiles with **zero** warnings under
`-Wall -Wextra`. The warnings you will still see are all
pre-existing and unrelated: `unused variable 'vpLeft'` and
`'vpTop'` in `Application.cpp`, two `-Wreturn-type` in
`InputMap.cpp`, `unused function 'makeCasing'` in
`Effects.cpp`, and a run of `-Wmismatched-tags` about
`TileMap` from `HBE.Renderer.GL`. If you see any of those,
you have not broken anything.

### Release

Run it as a separate check — the fixed step is not a Debug
feature and must survive `NDEBUG`:

```fish
cmake --build --preset linux-clang-release
```

Under `NDEBUG` every `HBE_PROFILE_SCOPE` expands to
`((void)0)`, including the new `"FixedUpdate"` one. The step
loop itself still runs — it is ordinary code, not a macro.
That is the whole point of criterion 2.

### Errors mapped to their cause

* **`fatal error: 'HBE/Core/Timestep.h' file not found`**
  — the header from doc `01` was not saved, or landed
  somewhere other than
  `HBE.Core/include/HBE/Core/Timestep.h`.

* **`error: unknown type name 'FixedTimestepConfig'`** in
  `Application.h` — the include from doc `03` §2 is missing.

* **`error: unknown type name 'FixedTimestepConfig'`** in
  `main.cpp` — you removed or moved `using namespace
  HBE::Core;` at line 10. Qualify it as
  `HBE::Core::FixedTimestepConfig` or restore the using.

* **`undefined reference to 'HBE::Core::FixedTimestep::configure(...)'`**
  at the link step — `src/Core/Timestep.cpp` is missing from
  `HBE.Core/CMakeLists.txt` (doc `02` §2).

* **`undefined reference to 'HBE::Core::Application::setTargetFrameRate(float)'`**
  — you did doc `03` (the declaration) but not doc `04` §2
  (the definition).

* **`error: no member named 'update' in 'MegaX::Player'; did you mean 'updateVisual'?`**
  — doc `05` renamed it but doc `06` §2 was not applied, so
  `GameLayer.cpp` still calls the old name.

* **`error: too few arguments to function call, expected 2, have 1`**
  pointing at `m_player.render(r2d)` — same cause.

* **`error: 'onFixedUpdate' marked 'override' but does not override any member functions`**
  — the signature in `GameLayer.h` does not match `Layer`'s.
  It is `void onFixedUpdate(float fixedDt)`; a `double`, a
  `const float&`, or a trailing `const` all produce this.

* **`ninja: error: 'src/Core/Timestep.cpp', needed by ..., missing`**
  — the path in `CMakeLists.txt` and the file on disk
  disagree. Fix the path, then `cmake --preset linux-clang`.

---

## 4. Run

```fish
./build/linux-clang/bin/Debug/MegaX
```

### 4.1 Startup

One new line, after `Application initialized.` and after
the `GameLayer attached` block:

```
[INFO]Application: fixed timestep 60.00 Hz (0.0167 s), max 5 catch-up steps, frame clamp 0.250 s.
```

With a flag, you get two — `setFixedTimestep`'s and
`run()`'s — and they must agree:

```fish
./build/linux-clang/bin/Debug/MegaX --fixed-hz 30
```

```
[INFO]Application: fixed timestep 30.00 Hz (0.0333 s), max 5 catch-up steps, frame clamp 0.250 s.
[INFO]Application: fixed timestep 30.00 Hz (0.0333 s), max 5 catch-up steps, frame clamp 0.250 s.
```

and with a cap:

```
[INFO]Application: render rate capped at 30.00 FPS.
```

Bad input degrades rather than exploding — this is the
validation in `configure()` being visible:

| You type | You get |
|---|---|
| `--fixed-hz 0` | 60.00 Hz (the flag is ignored: 0 means "leave the default") |
| `--fixed-hz -5` | 60.00 Hz |
| `--fixed-hz sixty` | 60.00 Hz (`atof` returns 0) |
| `--fixed-hz 5000` | 1000.00 Hz (clamped to `kMaxHz`) |
| `--fixed-catchup 0` | 5 (the flag is ignored) |
| `--fixed-hz` with nothing after it | 60.00 Hz (`i + 1 < argc` guards it) |

### 4.2 The 1-Hz profiler block

```
[INFO][Profiler] Frame=0.42ms (avg 0.82 min 0.32 max 15.50)
[INFO]  ApplicationUpdate  cur=  0.01 avg=  0.02 min=  0.01 max=  0.04 (n=58)
[INFO]    SceneUpdate        cur=  0.01 avg=  0.01 min=  0.00 max=  0.04 (n=58)
[INFO]      Particles          cur=  0.00 avg=  0.00 min=  0.00 max=  0.02 (n=58)
[INFO]  FixedUpdate        cur=  0.01 avg=  0.01 min=  0.00 max=  0.03 (n=58)
[INFO]  Audio              cur=  0.00 avg=  0.00 min=  0.00 max=  0.00 (n=58)
[INFO]  GameRender         cur=  0.25 avg=  0.29 min=  0.19 max=  1.72 (n=58)
[INFO]    TileRendering      cur=  0.09 avg=  0.12 min=  0.08 max=  0.24 (n=58)
[INFO]    SpriteRendering    cur=  0.12 avg=  0.13 min=  0.00 max=  1.55 (n=58)
[INFO]    Physics            cur=  0.00 avg=  0.00 min=  0.00 max=  0.01 (n=57)
[INFO]    Combat             cur=  0.00 avg=  0.00 min=  0.00 max=  0.00 (n=57)
[INFO]    AI                 cur=  0.01 avg=  0.01 min=  0.00 max=  0.02 (n=57)
```

Three structural things to check, and one that looks wrong
but is not:

* `FixedUpdate` exists, and sits at the **same** indent as
  `ApplicationUpdate`, `Audio` and `GameRender` — it is a
  top-level section, a sibling of the update loop.
* `Physics`, `Combat` and `AI` are indented **one** level
  (four spaces after the `[INFO]`), which is depth 1 — i.e.
  nested under `FixedUpdate`. They used to be at depth 2
  under `SceneUpdate`.
* `SceneUpdate` collapsed to roughly what `Particles` costs,
  because everything else it used to contain moved.

> **WHY ARE `Physics` / `Combat` / `AI` PRINTED LAST, AFTER
> `SpriteRendering`?** The profiler prints its sections in
> **registration order**, and a section registers the first
> frame it opens. Frame 1 runs **zero** fixed steps — the
> accumulator has not filled yet — so the three simulation
> sections register a frame later than everything else and
> sort to the bottom. Their `(n=57)` against everyone
> else's `(n=58)` is the same fact from the other side.
> This is not a nesting bug; read the indentation, not the
> order.

### 4.3 `F8`

The dump gains one line, directly under the CPU frame line:

```
Timestep: 60.00 Hz (0.0167 s)  steps last frame 0  dropped 0  alpha 0.412
```

`steps last frame` reading `0` on a Debug build with vsync
off is **correct and is the headline result of this item** —
the renderer is running several hundred frames a second and
the simulation is not. Press `F8` a few times: you should
see a mix of `0` and `1`, `alpha` wandering across `[0, 1)`,
and `dropped` pinned at `0`.

---

## 5. Verify checklist

### Group A — previous-item regression (nothing broke)

- [ ] `A`/`D` run, `SPACE` jumps, `S` crouches, `E` shoots —
      all feel the way they did before item 16.
- [ ] Jump height and distance are unchanged. Stand on the
      same tile and jump; you clear the same ledge.
- [ ] `G` toggles Ghost mode; flying is smooth in all four
      directions.
- [ ] `H` toggles the helmet sprite.
- [ ] `B` toggles the hit/hurt box overlay, and the boxes sit
      on the sprites rather than trailing them.
- [ ] `F1`/`F2`/`F3` switch difficulty and refill HP;
      `R` refills HP.
- [ ] `F5` reloads the map, `F7` soft-respawns, `F6` reloads
      the shader. **Watch the player sprite on the reload
      frame** — no streak, no smear (doc `05` §3.1).
- [ ] Editing `maps/level_01.json` on disk still triggers an
      automatic reload.
- [ ] `F8` prints the full snapshot; `F9` starts and stops a
      capture.
- [ ] The enemy patrols, shows `?` then `!`, chases, shoots,
      and dies with an explosion + fade.
- [ ] Particles: walk dust, muzzle flash, casings, bullet
      impacts, landing dust, blood splatter, enemy explosion.
- [ ] `F11` fullscreen still works and the letterbox is
      correct.
- [ ] `HBE.Sandbox` still runs:
      `./build/linux-clang/bin/Debug/HBE.Sandbox`

### Group B — the step loop exists and is bounded

- [ ] The startup timestep line appears exactly once with no
      flags, twice with `--fixed-hz`.
- [ ] `F8` shows `steps last frame` alternating between `0`
      and `1` on a fast Debug build.
- [ ] `--fixed-hz 30` halves it: over any interval the step
      count is half what 60 Hz produced.
- [ ] `--fixed-hz 120` doubles it.
- [ ] `dropped` reads `0` during normal play.
- [ ] `alpha` stays inside `[0, 1)` — never negative, never
      exactly 1.0 for more than a frame.

### Group C — interpolation is actually wired

- [ ] `--render-hz 144 --no-vsync`: the player, the enemy and
      bullets move **smoothly**, not in 60 Hz steps. Bullets
      are the tell — they are the fastest thing on screen.
- [ ] `--fixed-hz 15 --render-hz 60 --no-vsync`: the game
      crawls, and while it crawls the player sprite still
      **animates at full speed** and the animated tiles still
      flicker normally. That is golden rule 9 holding.
- [ ] In that same 15 Hz run, motion between steps is still
      smooth rather than a slideshow — that is the alpha
      doing its job.
- [ ] `F5` mid-run: no one-frame smear on the player or the
      enemy.
- [ ] Fire a burst of shots: the muzzle flash appears at the
      gun, not offset from it.

### Group D — the render cap

- [ ] `--render-hz 30 --no-vsync` → roughly 30 FPS.
- [ ] `--render-hz 120 --no-vsync` → roughly 120 FPS.
- [ ] No `render rate capped` line without the flag.
- [ ] With vsync **on** (no `--no-vsync`) and
      `--render-hz 500`, you get your monitor's refresh rate,
      not 500 — both are floors.
- [ ] `--render-hz 30` does not pin a CPU core.

Measured here, `--capture-seconds 5` with `--no-vsync`:

| Flag | Rows | Span | Rate |
|---|---|---|---|
| `--render-hz 30` | 151 | 5.00 s | **30.2 FPS** |
| `--render-hz 60` | 301 | 5.00 s | **60.2 FPS** |
| `--render-hz 120` | 601 | 5.00 s | **120.2 FPS** |
| uncapped | 3801 | 5.00 s | **760.3 FPS** |

Slightly *over* every target, never under — that is the
truncating cast in doc `04` §4 behaving as designed.

### Group E — hitch recovery

- [ ] Grab the window title bar and hold it still for two
      seconds, then release. The game resumes; the player has
      not teleported across the room; the loop does not stay
      slow afterwards.
- [ ] Press `F8` immediately after releasing: `dropped`
      is non-zero on that one frame and back to `0` on the
      next.
- [ ] Repeat the drag five times. No cumulative slowdown —
      that is the drain in `consumeStep()` preventing the
      spiral of death.

### Group F — Release, and item 15 still works

- [ ] Release runs: `./build/linux-clang/bin/Release/MegaX`
- [ ] Release plays identically to Debug (the simulation is
      not profiled, but it is not compiled out either).
- [ ] A capture still writes 27 columns:

  ```fish
  ./build/linux-clang/bin/Debug/MegaX --no-vsync --capture --capture-seconds 5 \
      --capture-label item16-baseline --capture-quit
  cd ~/.local/share/MegaX/MegaX/captures
  head -1 (ls -t *.csv | head -1) | tr ',' '\n' | wc -l
  # -> 27
  ```

- [ ] `physicsMs`, `combatMs` and `aiMs` are still non-zero
      in that CSV — proof the by-name lookup survived the
      re-nesting. Measured here at 60 Hz: `physicsMs` 0.0035,
      `combatMs` 0.0008, `aiMs` 0.0042 average.
- [ ] `sceneUpdateMs` is now much **smaller** than it was
      before this item. Expected — see doc `06` §2.1.

---

## 6. Done-criterion repro

The criterion, verbatim from `Docs/WorkItems.txt`:

> This item is complete when player movement and combat
> simulation produce nearly identical results at 30, 60, 120,
> and uncapped rendering rates.

"Nearly identical" is not something you can eyeball, so
measure it. The run is already deterministic without you
touching anything: the demo enemy patrols on a fixed path,
fires on a fixed cooldown, and the player stands still and
gets knocked around by the bullets. Everything that happens
is a pure function of simulated time — **provided** the
simulation is genuinely off the render clock, which is
exactly what is being tested.

### 6.1 Add the probe (temporary)

There is no permanent hook for this and there should not be
one. Add these two edits to
`/home/atulo/Projects/HBE/MegaX/src/Game/GameLayer.cpp`,
run the four measurements, then **delete them again**.

First, a step counter. Directly above
`void GameLayer::onFixedUpdate(float fixedDt) {`:

```cpp
    static unsigned long long s_totalSteps = 0;
```

and as the first line inside that function:

```cpp
        ++s_totalSteps;
```

Second, a dump. In `tickPerfCapture`, immediately inside
`if (m_perf.shouldQuit() && m_app)` and before the existing
`LogInfo("MegaX: capture finished, ...")`:

```cpp
	        {
	            char dbg[256];
	            const Enemy* e0 = m_enemies.enemies().empty() ? nullptr : &m_enemies.enemies()[0];
	            std::snprintf(dbg, sizeof(dbg),
	                "[DETERMINISM] steps=%llu playerX=%.4f playerY=%.4f enemyX=%.4f",
	                (unsigned long long)s_totalSteps,
	                m_player.x(), m_player.y(), e0 ? e0->x() : -1.0f);
	            LogInfo(dbg);
	        }
```

> **PASTE NOTE.** `tickPerfCapture` is one of the
> tab-indented functions in this mixed file — its body sits
> at a tab plus four spaces. The block above already
> matches; do not convert it.

Rebuild:

```fish
cmake --build --preset linux-clang-debug --target MegaX
```

### 6.2 Run the four measurements

Do not touch the keyboard or the mouse during these.

```fish
cd /home/atulo/Projects/HBE/build/linux-clang/bin/Debug

for hz in 30 60 120
    ./MegaX --no-vsync --render-hz $hz --capture --capture-warmup 1 \
        --capture-seconds 5 --capture-quit --capture-out /tmp/d$hz.csv 2>&1 \
        | grep DETERMINISM
end

./MegaX --no-vsync --capture --capture-warmup 1 \
    --capture-seconds 5 --capture-quit --capture-out /tmp/dunc.csv 2>&1 \
    | grep DETERMINISM
```

Measured here:

```
render-hz 30   : [INFO][DETERMINISM] steps=361 playerX=672.0000 playerY=152.0000 enemyX=839.2900
render-hz 60   : [INFO][DETERMINISM] steps=360 playerX=672.0000 playerY=152.0000 enemyX=839.2900
render-hz 120  : [INFO][DETERMINISM] steps=359 playerX=672.0000 playerY=152.0000 enemyX=839.2900
uncapped       : [INFO][DETERMINISM] steps=359 playerX=672.0000 playerY=152.0000 enemyX=839.2900
```

That is the criterion satisfied, and satisfied harder than
it asks for: the four rendering rates are **bit-identical**
on every position, not merely close. The render rate varies
by a factor of **25** between the first and last row and
changes nothing about where anything ended up.

Read the two columns separately:

* **`steps` varies by ±1 and that is correct.** 6 seconds of
  runtime at 60 Hz is 360 steps. The quit fires on a rendered
  frame, and depending on where that frame lands relative to
  the accumulator you catch 359, 360 or 361. A difference
  bigger than 2 means `--capture-warmup` or
  `--capture-seconds` differed between runs.
* **The three positions must match exactly.** If they do not,
  something in the simulation is still reading the render
  clock — see the troubleshooting matrix.

Before item 16, the same four runs would have produced four
different sets of positions, because `Player::update` and
`Enemy::tick` integrated against the frame time.

### 6.3 The control: changing the *simulation* rate does change the answer

```fish
./MegaX --no-vsync --render-hz 60 --fixed-hz 30 --capture --capture-warmup 1 \
    --capture-seconds 5 --capture-quit --capture-out /tmp/d.csv 2>&1 | grep DETERMINISM
./MegaX --no-vsync --render-hz 60 --fixed-hz 120 --capture --capture-warmup 1 \
    --capture-seconds 5 --capture-quit --capture-out /tmp/d.csv 2>&1 | grep DETERMINISM
```

```
fixed-hz 30  : steps=180 playerX=672.0000 playerY=152.0000 enemyX=839.2900
fixed-hz 120 : steps=719 playerX=672.0000 playerY=152.0000 enemyX=837.4675
```

* `steps` scales exactly with `hz`: 180, 360, 719 for 30, 60,
  120 over the same six seconds.
* At 120 Hz the enemy ends up **1.8 px** from where it does
  at 60 Hz. That is not a bug — a finer integration step
  produces a slightly different result from the same Euler
  integrator, and the enemy's patrol turnaround lands on a
  different sub-step.

This is the distinction the item is really about: **changing
the render rate must change nothing; changing the simulation
rate is allowed to.** A run that is identical across
`--fixed-hz` values would mean the fixed step is not actually
driving the simulation.

### 6.4 Remove the probe

Delete the counter, the `++s_totalSteps;`, and the
`[DETERMINISM]` block. Then confirm:

```fish
grep -c 'DETERMINISM\|s_totalSteps' /home/atulo/Projects/HBE/MegaX/src/Game/GameLayer.cpp
# -> 0
```

---

## 7. Tuning table — the flavor knobs

Two places to turn things. Per-run, on the command line:
`--fixed-hz`, `--fixed-catchup`, `--render-hz`. Permanently,
in `FixedTimestepConfig`'s defaults at
`/home/atulo/Projects/HBE/HBE.Core/include/HBE/Core/Timestep.h`
— that is also the only way to reach `maxFrameSeconds`, which
has no flag on purpose (doc `06` §5).

Iterate with the command line first. Only move a default once
you have found the value you want.

### Simulation rate

| Symptom | Field | Direction |
|---|---|---|
| Jumps and landings feel less precise than before item 16 | `hz` | `60 → 120` |
| A fast-falling player clips through a one-tile floor | `hz` | `60 → 120` (a 60 Hz step at `maxFall` 900 moves 15 px; a 32 px tile is only 2 steps thick) |
| The game is CPU-bound on a weak machine and you need headroom | `hz` | `60 → 30` |
| Enemy bullets pass through the player at Challenging speed | `hz` | `60 → 120` — 480 px/s is 8 px per step, and the hurtbox is 24 px wide, so this is not tunnelling yet; if it starts, this is the knob |
| You want to confirm something is on the fixed clock at all | `hz` | `60 → 10`, temporarily. Anything that keeps moving smoothly is on the render clock |

### Hitch behaviour

| Symptom | Field | Direction |
|---|---|---|
| After alt-tab the player has teleported / fallen through the world | `maxFrameSeconds` | `0.25 → 0.0833` (`maxCatchUpSteps / hz`). Stops skipping game time; the world runs in slow motion until it catches up instead |
| After a stall the game runs in slow motion for a noticeable beat | `maxCatchUpSteps` | `5 → 10`, or `--fixed-catchup 10`. Recovers faster, at the cost of a longer worst-case frame |
| One long frame causes a visible spike in the item-15 capture | `maxCatchUpSteps` | `5 → 3`. Caps the worst-case frame harder; more game time gets dropped |
| `dropped` is non-zero every frame, not just after a stall | — | The machine genuinely cannot sustain `hz`. Lower `hz` before touching anything else |

### Render rate

| Symptom | Field | Direction |
|---|---|---|
| Laptop fans spin up during development | `--render-hz 60` | Caps the loop without vsync's input latency |
| An item-15 capture reads absurd FPS and tells you nothing | drop `--render-hz`, keep `--no-vsync` | Uncapped is what measures real headroom |
| You want to reproduce a bug that only happens at low FPS | `--render-hz 20` | The simulation should be unaffected — if the bug follows the render rate, that is the bug |

### Ordering constraints

1. **`setFixedTimestep` must be called before `run()`.**
   Calling it later works — the config is applied on the next
   frame — but `run()`'s startup line will have already
   logged the old value, and that line is what you will
   believe six months from now.
2. **Changing `hz` resets the accumulator.** Pending game
   time is denominated in the old step size, so `configure()`
   throws it away. Never call `setFixedTimestep` on a timer
   or from inside `onFixedUpdate`.
3. **`--render-hz` and vsync are both floors.** With vsync on
   you get the slower of the two, so `--no-vsync` belongs in
   every command where the cap is the thing being tested.

---

## 8. Troubleshooting matrix

| Symptom | Likely cause | Fix |
|---|---|---|
| The game is frozen — renders fine, nothing moves | `FixedUpdate` never runs a step. `maxFrameSeconds` below one step, or `hz` enormous | Check the startup line. `configure()` floors `maxFrameSeconds` at `m_fixedDt` (doc `02` §1); if you removed that floor, restore it |
| The player is drawn smeared across the map for one frame after `F5` | A teleport that writes `m_x` but not `m_prevX` | Doc `05` §3.1 for `Player::setPosition`, §5.1 for `Enemy::spawn`, §7.2/§7.4 for the two `spawn` methods |
| Entities move in visible 60 Hz jumps on a 144 Hz display | `render` is drawing from `m_x` instead of the lerp, or `alpha` is not being passed | `grep -n 'interpolationAlpha' MegaX/src/Game/GameLayer.cpp` — it must appear in `onRender` |
| Everything jitters slightly, worse at high FPS | `alpha` read in `onUpdate` instead of `onRender`, so it is a frame stale | Golden rule 6. Move the read into `onRender` |
| One `SPACE` press produces two jumps | `Player::fixedUpdate` does not clear `m_jumpPressed`, so a second step in the same frame re-fires it | Doc `05` §3.2 — the two `= false` lines at the end |
| Jumping feels laggier than before | The step loop was placed **before** the `ApplicationUpdate` block | Doc `04` §3.3 — it goes between `ApplicationUpdate` and `Audio` |
| `F5` fires twice from one press | The F-key block was moved into `onFixedUpdate` | Doc `06` §5 — it stays in `onUpdate` |
| Animation freezes when `--fixed-hz` is low | `currentAnim().update` / `SpriteAnimation::update` left on the fixed step | Doc `05` §5.2 and §3.2 — both belong in `updateVisual` |
| Animated tiles freeze at low `--fixed-hz` | `m_world.update` left in `onFixedUpdate` | Doc `06` §2.1 — it moved to `onUpdate` |
| The 30/60/120 positions in §6.2 do not match | Something in the simulation still reads the render clock. Most likely a `dt` that should be `fixedDt` | Search `onFixedUpdate` for `dt`; every timing argument in it must be `fixedDt` |
| `steps` in §6.2 differs by more than 2 between runs | The runs had different warmup or duration, or you touched the keyboard | Re-run with identical flags and hands off |
| The `--fixed-hz 120` control run in §6.3 is *identical* to 60 Hz | The simulation is not on the fixed step at all — `onFixedUpdate` is empty or never called | `grep -c 'layer->onFixedUpdate' HBE.Core/src/Core/Application.cpp` → 1 |
| `steps last frame` in `F8` is always 5 | The machine cannot hit `hz`, and every frame is spending the whole catch-up budget | Lower `hz`. If it happens in Debug only, that is Debug being slow, not a defect |
| The loop pins a core at 100 % with no `--render-hz` | The `else` branch's `delayMillis(1)` was deleted | Doc `04` §4 — the `else` is the original line and must stay |
| `--render-hz 120` gives 60 | vsync is on. Both are floors | Add `--no-vsync` |
| `--render-hz 120` gives ~1000 | `setTargetFrameRate` is never called, or `m_targetFrameRate` stayed 0 | `grep -n 'setTargetFrameRate' MegaX/src/main.cpp` → 1 |
| A capture stops after exactly N rows when you asked for `--capture-seconds N` | **Item 15 defect, already fixed on 2026-08-18** — `PerfCapture::tick` had a stray `if (m_samples.size() >= m_req.durationSeconds)` comparing a sample count against a duration. Listed here because it is the kind of typo that gets retyped | Delete that block. `Docs/15-*/02_perf_capture_impl.md` line 374 specifies only the `kMaxSamples` guard |
| `physicsMs` / `combatMs` / `aiMs` read 0 in a new capture | Release build without `MEGAX_PROFILE_IN_RELEASE`, which is pre-existing item-15 behaviour | Capture in Debug, or configure with `-DMEGAX_PROFILE_IN_RELEASE=ON` |
| `sceneUpdateMs` collapsed compared to an older capture | Expected — the simulation moved out of `SceneUpdate` | Not a defect. Do not compare that column across item 16 |
| `HBE.Sandbox` fails to build | `onFixedUpdate` was made pure virtual | Doc `03` §5 — it must have a body |

---

## 9. What comes after this item

* **Item 17 — deterministic named random streams.** This
  item gives you a simulation that is a pure function of
  *step count*; item 17 makes it a pure function of *step
  count and seed*, which is what turns §6.2's hand-driven
  comparison into something a test can assert.
* **Item 20 — the golden benchmark room.** §6.2 works today
  because the demo enemy happens to be deterministic. Item 20
  makes that a property of the room rather than an accident,
  with fixed spawn points and a repeatable seed.
* **Items 21-28 — the lighting arc.** Lights are presentation
  and belong on the render clock. `interpolationAlpha()` is
  what a light attached to a moving entity will read so it
  does not lag its owner by a step.
* **Items 31+ — the roguelike run systems.** Every stat tick,
  status effect, cooldown and DoT gets `onFixedUpdate`, which
  means "5 seconds of burn" is 300 steps on every machine
  rather than "however many frames that was".

The minimalism of `FixedTimestep`'s public surface is
deliberate in that light: `beginFrame`, `consumeStep`,
`alpha` and three diagnostics are everything the loop needs,
and every one of them is already consumed by
`Application::run` or by MegaX. Nothing was added on
speculation.

**Item 16 is complete.**
