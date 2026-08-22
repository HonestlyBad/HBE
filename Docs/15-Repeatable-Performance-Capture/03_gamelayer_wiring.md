# 03 — Wiring the capture into the game (`Game/GameLayer.h/.cpp`)

This doc edits two existing files: `GameLayer.h` (four small
insertions) and `GameLayer.cpp` (five insertions). It
depends on docs `01` and `02`. Nothing is deleted and no
existing line is modified — every edit is an insertion.

After this doc, `F9` works and the capture records real
data. The command-line switches still do nothing until doc
`04` wires `main.cpp`.

**Both files are tab-indented in their existing regions and
4-space-indented in the newer blocks.** `GameLayer.h` is
tab-indented throughout — use tabs there. `GameLayer.cpp` is
mixed: the `onAttach`/`onUpdate` signatures and the older
body lines use tabs, while everything added in items 12–14
uses 4 spaces. Each code block below already carries the
right whitespace for where it goes; paste it as given and do
not re-indent the surrounding lines.

---

## Key engine APIs used here

- `HBE::Platform::Input::IsKeyPressed(SDL_Scancode)` — one-shot
  edge detect, already aliased as `Input::` at the top of
  `GameLayer.cpp` (line 22). `SDL_SCANCODE_F9` is unused by
  MegaX and by `Application::handleSDLEvent`, which only
  claims `SDL_SCANCODE_F11`.
- `HBE_PROFILE_SCOPE(NAME)` — RAII CPU timer from
  `HBE/Core/Profiler.h`, already included by `GameLayer.cpp`
  (line 7). Expands to `((void)0)` under `NDEBUG`.
- `HBE::Core::Application::requestQuit()` — sets
  `m_running = false`, so the loop exits cleanly after the
  current frame (`Application.h:38`).
- Already-existing MegaX accessors the capture reads:
  `EnemyManager::aliveCount()`, `BulletManager::count()`,
  `EnemyBulletManager::count()`, `Effects::liveParticles()`,
  `EnemyManager::profile()` → `DifficultyProfile` (its
  `label` is a string literal).

---

## 1. `GameLayer.h` — four insertions

Open `/home/atulo/Projects/HBE/MegaX/include/Game/GameLayer.h`.

### 1.1 The include

The game-side include block currently reads (lines 8-12):

```cpp
#include "Game/Player.h"
#include "Game/Bullet.h"
#include "Game/Effects.h"
#include "Game/EnemyManager.h"
#include "World/World.h"
```

Insert **right after** `#include "Game/EnemyManager.h"`
(line 11) and **before** `#include "World/World.h"`
(line 12):

```cpp
#include "Game/PerfCapture.h"
```

Final block should read:

```cpp
#include "Game/Player.h"
#include "Game/Bullet.h"
#include "Game/Effects.h"
#include "Game/EnemyManager.h"
#include "Game/PerfCapture.h"
#include "World/World.h"
```

### 1.2 The `setCaptureRequest` accessor

The public section currently ends (lines 20-26):

```cpp
		void onAttach(HBE::Core::Application& app) override;
		void onUpdate(float dt) override;
		void onRender() override;

		Difficulty m_difficulty = Difficulty::Difficult;

	private:
```

Insert **right after** `Difficulty m_difficulty = Difficulty::Difficult;`
(line 24, which is now line 25 after edit 1.1) and **before**
the blank line preceding `private:`:

```cpp

		// Item 15: applied by main.cpp before the layer is pushed.
		void setCaptureRequest(const PerfCaptureRequest& req) { m_perf.configure(req); }
```

Final block should read:

```cpp
		void onAttach(HBE::Core::Application& app) override;
		void onUpdate(float dt) override;
		void onRender() override;

		Difficulty m_difficulty = Difficulty::Difficult;

		// Item 15: applied by main.cpp before the layer is pushed.
		void setCaptureRequest(const PerfCaptureRequest& req) { m_perf.configure(req); }

	private:
```

> **WHY PUBLIC AND INLINE?** `main.cpp` needs to reach it,
> and it is a one-line forward to `m_perf.configure`. Making
> it a real function in the `.cpp` would add a translation
> unit dependency for nothing. `m_difficulty` is already a
> public member on this class, so a public setter is in
> keeping with the file.

### 1.3 The private method declaration

The private method block currently reads (lines 27-34, now
31-38):

```cpp
	private:
		void buildSpritePipeline();
		void drawHud(HBE::Renderer::Renderer2D& r2d);
		void spawnDemoEnemies();
		bool reloadScene(bool alsoReloadMap);
		void clearTransientEntites();
		void hotReloadShader();
		void setupHotReloadWatches();
```

Insert **right after** `void setupHotReloadWatches();`
(originally line 33) and **before** the whitespace-only line
that precedes `HBE::Core::Application* m_app = nullptr;`:

```cpp
		void tickPerfCapture(float dt);
```

Final block should read:

```cpp
	private:
		void buildSpritePipeline();
		void drawHud(HBE::Renderer::Renderer2D& r2d);
		void spawnDemoEnemies();
		bool reloadScene(bool alsoReloadMap);
		void clearTransientEntites();
		void hotReloadShader();
		void setupHotReloadWatches();
		void tickPerfCapture(float dt);
```

### 1.4 The member

The member block currently reads (lines 48-52, now 53-57):

```cpp
		HBE::Core::FileWatcher m_watcher{};
		HBE::Renderer::DebugDraw2D m_debug{};
		bool m_showHitBoxes = false;

		std::string m_tileMapPath = "maps/level_01.json";
```

Insert **right after** `bool m_showHitBoxes = false;`
(originally line 50) and **before** the blank line preceding
`std::string m_tileMapPath`:

```cpp

		PerfCapture m_perf{};
```

Final block should read:

```cpp
		HBE::Core::FileWatcher m_watcher{};
		HBE::Renderer::DebugDraw2D m_debug{};
		bool m_showHitBoxes = false;

		PerfCapture m_perf{};

		std::string m_tileMapPath = "maps/level_01.json";
```

Nothing else in `GameLayer.h` changes. The file grows from
58 to 65 lines.

---

## 2. `GameLayer.cpp` — five insertions

Open `/home/atulo/Projects/HBE/MegaX/src/Game/GameLayer.cpp`.

No new `#include` is needed: `GameLayer.h` now pulls in
`PerfCapture.h`, and `HBE/Core/Profiler.h` is already
included at line 7. **Do not** add a duplicate include.

The five edits are given in top-to-bottom order. Each one
lists the line number **in the original file** and, in
parentheses, the line number **after the previous edits in
this doc** have been applied — so you can work straight down
the file without recounting.

| # | Original line | After previous edits | What |
|---|---|---|---|
| 2.1 | 91 | 91 | `setSceneLabel` in `onAttach` |
| 2.2 | 94 | 96 | `tickPerfCapture(dt)` at the top of `onUpdate` |
| 2.3 | 222 | 229 | the `F9` toggle |
| 2.4 | 317 | 328 | `HBE_PROFILE_SCOPE("GameRender")` |
| 2.5 | 350 | 363 | the `tickPerfCapture` definition |

### 2.1 Tell the capture which scene it is recording

The end of `onAttach` currently reads (lines 89-92):

```cpp
		LogInfo("MegaX GameLayer attached (Play mode; press G for Ghost).");

        setupHotReloadWatches();
	}
```

Insert **right after** `setupHotReloadWatches();` (line 91)
and **before** the closing `	}` (line 92):

```cpp

        m_perf.setSceneLabel(m_tileMapPath);
```

Final block should read:

```cpp
		LogInfo("MegaX GameLayer attached (Play mode; press G for Ghost).");

        setupHotReloadWatches();

        m_perf.setSceneLabel(m_tileMapPath);
	}
```

This is the only thing the capture needs from attach time.
It is a `std::string` copy of `"maps/level_01.json"`, done
once, and it is what ends up on the `scene :` line of the
`.meta.txt` so two captures of two different rooms are
never confused.

### 2.2 Sample the previous frame

`onUpdate` currently begins (lines 94-96):

```cpp
	void GameLayer::onUpdate(float dt) {
        HBE_PROFILE_SCOPE("SceneUpdate");
        m_watcher.poll(dt);
```

Insert **right after** `void GameLayer::onUpdate(float dt) {`
(line 94; line 96 after edit 2.1) and **before**
`HBE_PROFILE_SCOPE("SceneUpdate");`:

```cpp
        // Item 15: sample the frame that just finished, before any of this
        // frame's work runs and outside every profile scope, so the capture
        // never shows up in its own numbers.
        tickPerfCapture(dt);

```

Final block should read:

```cpp
	void GameLayer::onUpdate(float dt) {
        // Item 15: sample the frame that just finished, before any of this
        // frame's work runs and outside every profile scope, so the capture
        // never shows up in its own numbers.
        tickPerfCapture(dt);

        HBE_PROFILE_SCOPE("SceneUpdate");
        m_watcher.poll(dt);
```

> **WHY HERE, AND NOWHERE ELSE?** Three reasons, all
> load-bearing. First, `Profiler::EndFrame()` builds the
> snapshot at the end of the frame, so this is the earliest
> point at which a *complete and self-consistent* snapshot
> exists — CPU sections, GPU timings and renderer stats all
> from the same frame. Second, being above
> `HBE_PROFILE_SCOPE("SceneUpdate")` keeps the capture's own
> cost out of the `SceneUpdate` column. Third, the game
> state has not changed yet this frame, so the enemy /
> bullet / particle counts gathered here match the frame
> the snapshot describes.
>
> **Do not** move this below the `Input::IsKeyPressed`
> block, and **do not** put it in `onRender` — by then the
> counts describe a different frame than the timings do.

### 2.3 The `F9` toggle

The `F8` block ends (lines 219-225):

```cpp
            }

            LogInfo("============================================");
        }

        {
            HBE_PROFILE_SCOPE("Physics");
```

Insert **right after** the `        }` that closes the `F8`
block (line 222; line 229 after edits 2.1-2.2) and **before**
the blank line preceding the `Physics` block:

```cpp

        if (Input::IsKeyPressed(SDL_SCANCODE_F9)) {
            m_perf.toggle();
        }
```

Final block should read:

```cpp
            }

            LogInfo("============================================");
        }

        if (Input::IsKeyPressed(SDL_SCANCODE_F9)) {
            m_perf.toggle();
        }

        {
            HBE_PROFILE_SCOPE("Physics");
```

`F9` sits with the other F-keys, after `F8` and before the
`Physics` scope, so it is outside every profile scope — the
same reasoning as the `F1`-`F8` handlers above it. It
changes no gameplay state at all.

### 2.4 The `"GameRender"` scope

`onRender` currently begins (lines 317-320):

```cpp
	void GameLayer::onRender() {
		Renderer2D& r2d = m_app->renderer2D();

		r2d.beginScene(m_camera.camera(), RenderPass::World);
```

Insert **right after** `void GameLayer::onRender() {`
(line 317; line 328 after edits 2.1-2.3) and **before**
`Renderer2D& r2d = m_app->renderer2D();`:

```cpp
        HBE_PROFILE_SCOPE("GameRender");

```

Final block should read:

```cpp
	void GameLayer::onRender() {
        HBE_PROFILE_SCOPE("GameRender");

		Renderer2D& r2d = m_app->renderer2D();

		r2d.beginScene(m_camera.camera(), RenderPass::World);
```

> **WHY A WHOLE-FUNCTION SCOPE?** The existing
> `TileRendering` and `SpriteRendering` scopes only cover
> submission. The batch is not flushed until
> `r2d.endScene()` at the bottom of the function, and
> `drawHud` draws between them — so together those two
> scopes account for well under half of the real CPU render
> cost. `GameRender` wraps `beginScene` through `endScene`
> and is what the CSV's `renderMs` column reports.
>
> Nesting is fine and intended: the profiler tracks depth,
> so `GameRender` shows at depth 0 with `TileRendering` and
> `SpriteRendering` as its children in the 1-Hz log block,
> exactly like `SceneUpdate` and its children.

### 2.5 The `tickPerfCapture` definition

`buildSpritePipeline` currently begins (lines 348-351):

```cpp
	}

    void GameLayer::buildSpritePipeline() {
        auto& resources = m_app->resources();
```

Insert **right before** `void GameLayer::buildSpritePipeline() {`
(line 350; line 363 after edits 2.1-2.4), immediately after
the blank line that follows the closing brace of
`onRender`:

```cpp
    void GameLayer::tickPerfCapture(float dt) {
        PerfCapture::FrameCounts counts{};
        counts.enemies      = m_enemies.aliveCount();
        counts.bullets      = m_bullets.count();
        counts.enemyBullets = m_enemies.enemyBullets().count();
        counts.particles    = m_effects.liveParticles();
        counts.difficulty   = m_enemies.profile().label;

        m_perf.tick(dt, counts);

        if (m_perf.shouldQuit() && m_app) {
            LogInfo("MegaX: capture finished, --capture-quit requested. Exiting.");
            m_app->requestQuit();
        }
    }

```

Final region should read:

```cpp
		r2d.endScene();
	}

    void GameLayer::tickPerfCapture(float dt) {
        PerfCapture::FrameCounts counts{};
        counts.enemies      = m_enemies.aliveCount();
        counts.bullets      = m_bullets.count();
        counts.enemyBullets = m_enemies.enemyBullets().count();
        counts.particles    = m_effects.liveParticles();
        counts.difficulty   = m_enemies.profile().label;

        m_perf.tick(dt, counts);

        if (m_perf.shouldQuit() && m_app) {
            LogInfo("MegaX: capture finished, --capture-quit requested. Exiting.");
            m_app->requestQuit();
        }
    }

    void GameLayer::buildSpritePipeline() {
        auto& resources = m_app->resources();
```

Note what each count is, because the CSV depends on it:

| Field | Source | Counts |
|---|---|---|
| `enemies` | `EnemyManager::aliveCount()` | enemies whose `isAlive()` is true — dying/finished ones excluded |
| `bullets` | `BulletManager::count()` | the whole player-bullet vector, including entries marked dead this frame but not yet culled |
| `enemyBullets` | `EnemyBulletManager::count()` | same, for enemy bullets |
| `particles` | `Effects::liveParticles()` | live particle-system particles **plus** live shell casings |
| `difficulty` | `EnemyManager::profile().label` | `"Casual"` / `"Difficult"` / `"Challenging"` string literal |

`entities` is derived inside `PerfCapture::record()` as
`1 + enemies + bullets + enemyBullets` — the `1` is the
player. MegaX does not register these in the ECS, so there
is no registry count to read; this is the honest definition
and doc `05` §1 states it in the column table.

`GameLayer.cpp` grows from 579 to 608 lines — 2 + 5 + 4 + 2
+ 16 inserted lines, in the order of the table above.

---

## 3. What NOT to touch

* Do **not** add a `PerfCapture` call inside `reloadScene`,
  `clearTransientEntites`, or `spawnDemoEnemies`. `F5`
  during a capture is a legitimate measurement (it shows
  the reload spike in the data), and stopping or resetting
  the capture on reload would silently discard it.
* Do **not** call `m_perf.tick()` from `onRender` as well.
  Two calls per frame would be harmless only because
  `record()` de-dupes on `frameIndex`, and relying on that
  is how the second call quietly becomes load-bearing.
* Do **not** change what `F8` does. It stays the one-shot
  profiler dump from item 14; `00_overview.md` §7 explains
  why the two are kept separate.
* Do **not** move `drawHud(r2d)` or reorder anything inside
  `onRender` to try to make `drawCalls` non-zero. That is
  golden rule 7 in the overview — the fix belongs in the
  engine, in a later `[HBE]` item.
* Do **not** add scopes around the individual counter reads
  in `tickPerfCapture`. `aliveCount()` walks at most a
  handful of enemies; a profiler scope costs more than the
  loop.

---

## 4. Sanity check before doc 04

```fish
grep -c "m_perf" /home/atulo/Projects/HBE/MegaX/include/Game/GameLayer.h
# -> 2  (setCaptureRequest's body, and the member declaration)

grep -c "m_perf" /home/atulo/Projects/HBE/MegaX/src/Game/GameLayer.cpp
# -> 4  (setSceneLabel, toggle, tick, shouldQuit)

grep -n "HBE_PROFILE_SCOPE" /home/atulo/Projects/HBE/MegaX/src/Game/GameLayer.cpp
# -> 8 matches: SceneUpdate, Physics, Combat, AI, Particles,
#    GameRender, TileRendering, SpriteRendering
#    (item 14 had 7 — GameRender is the new one)

grep -c "SDL_SCANCODE_F9" /home/atulo/Projects/HBE/MegaX/src/Game/GameLayer.cpp
# -> 1

grep -c "tickPerfCapture" /home/atulo/Projects/HBE/MegaX/src/Game/GameLayer.cpp
# -> 2  (the call and the definition)
```

The project **should** build and run at this point:

```fish
cd /home/atulo/Projects/HBE
cmake --build --preset linux-clang-debug --target MegaX
./build/linux-clang/bin/Debug/MegaX
```

Press `F9`, wait ten seconds, and you should get the
`[PerfCapture]` lines and a summary block. The command-line
switches do nothing yet — that is doc `04`.

If it fails with
``error: no member named 'tickPerfCapture' in 'MegaX::GameLayer'``,
edit 1.3 was skipped.

If it fails with
``error: unknown type name 'PerfCapture'`` in `GameLayer.h`,
edit 1.1 was skipped or the include landed after
`World/World.h` in a way that broke the order.

If it fails with
``error: use of undeclared identifier 'SDL_SCANCODE_F9'``,
something removed the `HBE/Platform/Input.h` include at
line 13 — restore it; the F-key block above your edit needs
it too.

Next: `04_main_command_line.md`.
