# 16 — Splitting `GameLayer`, and the new flags (`GameLayer`, `main.cpp`)

This doc finishes MegaX. Four files:

* `MegaX/include/Game/GameLayer.h` — one declaration.
* `MegaX/src/Game/GameLayer.cpp` — `onUpdate` splits into
  `onUpdate` + `onFixedUpdate`; `onRender` reads the alpha.
* `MegaX/src/main.cpp` — `--fixed-hz`, `--fixed-catchup`,
  `--render-hz`.
* `MegaX/src/Game/PerfCapture.cpp` — three lines in the
  usage text, so `--help` still lists everything.

At the end of this doc everything compiles, everything runs,
and the item is implemented. Doc `07` verifies it.

---

## 1. `GameLayer.h`

Open `/home/atulo/Projects/HBE/MegaX/include/Game/GameLayer.h`.
**Tabs.**

Lines 21-23 currently read:

```cpp
		void onAttach(HBE::Core::Application& app) override;
		void onUpdate(float dt) override;
		void onRender() override;
```

Insert right after `void onUpdate(float dt) override;`
(line 22) and before `void onRender() override;` (line 23):

```cpp
		void onFixedUpdate(float fixedDt) override;
```

Final block:

```cpp
		void onAttach(HBE::Core::Application& app) override;
		void onUpdate(float dt) override;
		void onFixedUpdate(float fixedDt) override;
		void onRender() override;
```

No new members are needed. Every intent the fixed step
consumes is already latched inside `Player` (doc `05` §3.2),
which is why `GameLayer` needs no shadow copy of the input
state.

---

## 2. `GameLayer.cpp` — the split

Open `/home/atulo/Projects/HBE/MegaX/src/Game/GameLayer.cpp`.

> **LINE NUMBERS IN THIS DOC.** Every line number below
> refers to the file **as it is before you start this doc** —
> they are all measured against the pristine file, never
> against a partly-edited one. Once you apply an edit, the
> numbers for every later edit in the same file shift down by
> whatever you inserted. That is why each edit is *also*
> anchored by the exact text at the insertion point: when the
> number and the text disagree, **the text wins**. Search for
> the quoted line, do not scroll to the number.

> **PASTE NOTE — THIS IS A FULL REPLACEMENT, AND IT
> RE-INDENTS.** `GameLayer::onUpdate` is currently a mix:
> the input reads at lines 101-116 use **tabs**, the F-key
> block and the Physics/Combat/AI blocks use **8 spaces**,
> and line 97 is a tab followed by four spaces. Both forms
> render identically at tab width 4, which is why the file
> has drifted this way without anyone noticing.
>
> Because this doc **replaces** the function rather than
> inserting into it, the replacement standardises on
> 4-spaces-per-level, matching the dominant style of the
> region and matching `tickPerfCapture` at line 359 — the
> most recently added function in the file. This is a
> deliberate rewrite of lines you are retyping anyway, not a
> drive-by retab of lines you are only passing through.
> `onAttach`, `drawHud`, `reloadScene` and everything else
> in the file keep their current indentation untouched.

### 2.1 Replace `onUpdate`

**Replace lines 96-322 inclusive** — from
`void GameLayer::onUpdate(float dt) {` on line 96 through
its closing brace on line 322, stopping before the blank
line that precedes `void GameLayer::onRender() {` on line
324 — with the following **two** functions:

```cpp
    // ---------------------------------------------------------------------
    // Item 16: the render-clock half.
    //
    // Runs exactly once per rendered frame, before the fixed step. Owns:
    //   * anything that reads input (it must see THIS frame's input, and
    //     the setters below latch it where the fixed step will find it)
    //   * anything one-shot and user-driven (the F-key block)
    //   * animation, particles, and the camera -- all presentation
    //
    // Owns nothing that decides where an entity ends up. That is
    // onFixedUpdate's job, and the split is what makes the game behave the
    // same at 30, 60, 120 and uncapped rendering rates.
    // ---------------------------------------------------------------------
    void GameLayer::onUpdate(float dt) {
        tickPerfCapture(dt);

        HBE_PROFILE_SCOPE("SceneUpdate");
        m_watcher.poll(dt);

        // ---- input sampling ---------------------------------------------

        // horizontal run (both modes)
        const float ix = (Input::IsKeyDown(SDL_SCANCODE_D) ? 1.0f : 0.0f)
            - (Input::IsKeyDown(SDL_SCANCODE_A) ? 1.0f : 0.0f);

        // vertical fly intent -- only used by Ghost mode; Play mode ignores iy
        const float iy = (Input::IsKeyDown(SDL_SCANCODE_SPACE) ? 1.0f : 0.0f)
            - (Input::IsKeyDown(SDL_SCANCODE_S) ? 1.0f : 0.0f);

        // platformer intents (Play mode)
        const bool jumpPressed = Input::IsKeyPressed(SDL_SCANCODE_SPACE); // one-shot
        const bool jumpHeld    = Input::IsKeyDown(SDL_SCANCODE_SPACE);    // for variable height
        const bool crouchHeld  = Input::IsKeyDown(SDL_SCANCODE_S);

        // shooting (E) -- semi/auto-fire handled in Player at a cadence
        const bool firePressed = Input::IsKeyPressed(SDL_SCANCODE_E);
        const bool fireHeld    = Input::IsKeyDown(SDL_SCANCODE_E);

        // Hand the intents to Player NOW, on the render clock. Player latches
        // the one-shots (jump, fire) and clears them in its first fixed step,
        // so one press produces one jump even when two steps run this frame.
        m_player.setMoveInput(ix, iy);
        m_player.setJumpInput(jumpPressed, jumpHeld);
        m_player.setCrouchInput(crouchHeld);
        m_player.setFireInput(firePressed, fireHeld);

        if (Input::IsKeyPressed(SDL_SCANCODE_H)) {
            m_player.toggleHelmet();
        }

        // G toggles Play <-> Ghost (fly, no gravity/collision -- for map building)
        if (Input::IsKeyPressed(SDL_SCANCODE_G)) {
            m_player.toggleMode();
            LogInfo(m_player.mode() == Player::Mode::Ghost
                ? "MegaX: Ghost mode (fly, no collision)."
                : "MegaX: Play mode (gravity + collision).");
        }

        if (Input::IsKeyPressed(SDL_SCANCODE_B)) {
            m_showHitBoxes = !m_showHitBoxes;
            LogInfo(m_showHitBoxes
                ? "MegaX: hit/hurt box overlay ON."
                : "MegaX: hit/hurt box overlay OFF.");
        }

        if (Input::IsKeyPressed(SDL_SCANCODE_F1)) {
            m_difficulty = Difficulty::Casual;
            m_enemies.setDifficulty(m_difficulty);
            m_player.refillHp();
            LogInfo("Difficulty: Casual");
        }
        if (Input::IsKeyPressed(SDL_SCANCODE_F2)) {
            m_difficulty = Difficulty::Difficult;
            m_enemies.setDifficulty(m_difficulty);
            m_player.refillHp();
            LogInfo("Difficulty: Difficult");
        }
        if (Input::IsKeyPressed(SDL_SCANCODE_F3)) {
            m_difficulty = Difficulty::Challenging;
            m_enemies.setDifficulty(m_difficulty);
            m_player.refillHp();
            LogInfo("Difficulty: Challenging");
        }
        if (Input::IsKeyPressed(SDL_SCANCODE_R)) {
            m_player.refillHp();
            LogInfo("HP refilled");
        }
        if (Input::IsKeyPressed(SDL_SCANCODE_F5)) {
            LogInfo("MegaX: scene reload requested (F5).");
            reloadScene(true);
        }
        if (Input::IsKeyPressed(SDL_SCANCODE_F6)) {
            LogInfo("MegaX: sprite shader hot reload requested (F6).");
            hotReloadShader();
        }
        if (Input::IsKeyPressed(SDL_SCANCODE_F7)) {
            LogInfo("MegaX: soft respawn requested (F7).");
            reloadScene(false);
        }
        if (Input::IsKeyPressed(SDL_SCANCODE_F8)) {
            const auto& snap = HBE::Core::Profiler::GetSnapshot();
            char line[256];

            LogInfo("========== [Profiler F8 Snapshot] ==========");

            std::snprintf(line, sizeof(line),
                "CPU frame: %.2f ms (avg %.2f, min %.2f, max %.2f, samples %zu)",
                snap.frameMs, snap.frameAvgMs, snap.frameMinMs, snap.frameMaxMs,
                snap.frameSampleCount);
            LogInfo(line);

            // Item 16: how the simulation clock is actually behaving, right
            // next to the render clock it is decoupled from.
            std::snprintf(line, sizeof(line),
                "Timestep: %.2f Hz (%.4f s)  steps last frame %d  dropped %d  alpha %.3f",
                static_cast<double>(m_app->timestep().config().hz),
                static_cast<double>(m_app->fixedDeltaSeconds()),
                m_app->timestep().stepsLastFrame(),
                m_app->timestep().droppedStepsLastFrame(),
                static_cast<double>(m_app->interpolationAlpha()));
            LogInfo(line);

            if (snap.gpu.supported) {
                std::snprintf(line, sizeof(line),
                    "GPU frame: %.2f ms (avg %.2f, min %.2f, max %.2f, samples %zu)",
                    snap.gpu.frameMs, snap.gpu.frameAvgMs, snap.gpu.frameMinMs,
                    snap.gpu.frameMaxMs, snap.gpu.frameSampleCount);
                LogInfo(line);
            } else {
                LogInfo("GPU frame: <unsupported / disabled>");
            }

            const auto& r = snap.renderer;
            std::snprintf(line, sizeof(line),
                "Renderer: drawCalls=%d passes=%d submitQ=%d renderQ=%d culled=%d"
                " matChg=%d texChg=%d tileChunks=%d ppPasses=%d",
                r.drawCalls, r.passes, r.submittedQuads, r.renderedQuads, r.culledSprites,
                r.materialChanges, r.textureChanges, r.visibleTileChunks, r.postProcessPasses);
            LogInfo(line);

            std::snprintf(line, sizeof(line),
                "Scene: liveParticles=%d activeLights=%d shadowLights=%d",
                r.liveParticles, r.activeLights, r.shadowCastingLights);
            LogInfo(line);

            std::snprintf(line, sizeof(line), "CPU sections (%zu):", snap.sections.size());
            LogInfo(line);
            for (const auto& s : snap.sections) {
                std::snprintf(line, sizeof(line),
                    "  %-20s cur %7.3f ms  avg %7.3f  min %7.3f  max %7.3f",
                    s.name ? s.name : "?", s.currentMs, s.avgMs, s.minMs, s.maxMs);
                LogInfo(line);
            }

            std::snprintf(line, sizeof(line), "GPU sections (%zu):", snap.gpu.sections.size());
            LogInfo(line);
            for (const auto& g : snap.gpu.sections) {
                std::snprintf(line, sizeof(line),
                    "  %-20s cur %7.3f ms  avg %7.3f  min %7.3f  max %7.3f",
                    g.name ? g.name : "?", g.currentMs, g.avgMs, g.minMs, g.maxMs);
                LogInfo(line);
            }

            LogInfo("============================================");
        }
        if (Input::IsKeyPressed(SDL_SCANCODE_F9))
        {
            m_perf.toggle();
        }

        // ---- render-clock simulation ------------------------------------
        // Animated tiles are animation, so they advance on dt, not on the
        // fixed step. At --fixed-hz 15 the game crawls and the tiles keep
        // flickering at full speed -- that is the intended behaviour, and
        // the work item asks for it by name.
        m_world.update(dt);

        // Sprite frames and tints. Both read state the fixed step owns, so
        // they lag it by at most one frame; at 14 fps of sprite animation
        // that is not observable.
        m_player.updateVisual(dt);
        m_enemies.updateVisual(dt);

        {
            const bool moving = (ix != 0.0f);
            const bool grounded = (m_player.velY() == 0.0f) && (m_player.groundTileId() != 0);
            m_effects.tickWalkDust(dt, m_player.x(), m_player.feetY(), m_player.groundTileId(), moving, grounded);
        }

        {
            HBE_PROFILE_SCOPE("Particles");
            m_effects.update(dt);
        }

        // Camera follows the last completed fixed step's position. With
        // followResponse at 9.0 the camera's own smoothing has a time
        // constant around 110 ms, so a sub-step of lag is invisible.
        m_camera.setFollowTarget(m_player.x(), m_player.y());
        m_camera.setFollowVelocity(m_player.velX(), m_player.velY());
        m_camera.update(dt);
        m_app->gl().setCamera(m_camera.camera());

        // -------- Item 14: publish game-side stats to the profiler --------
        HBE::Core::Profiler::PublishParticleStats(m_effects.liveParticles());
        HBE::Core::Profiler::PublishLightStats(0, 0); // MegaX has no lighting yet.
    }

    // ---------------------------------------------------------------------
    // Item 16: the fixed-clock half.
    //
    // Runs 0..maxCatchUpSteps times per rendered frame, always with the same
    // fixedDt. Everything that decides where an entity ends up, who got hit,
    // and what the AI does lives here.
    //
    // Zero steps in a frame is normal and correct: at 300 FPS with a 60 Hz
    // step this body runs on one frame in five.
    // ---------------------------------------------------------------------
    void GameLayer::onFixedUpdate(float fixedDt) {
        {
            HBE_PROFILE_SCOPE("Physics");
            m_player.fixedUpdate(fixedDt);

            float bx, by; int bdir;
            if (m_player.consumeShot(bx, by, bdir)) {
                m_bullets.spawn(bx, by, bdir);

                m_enemies.notifyGunshot(bx, by);

                m_effects.spawnMuzzleFlash(bx, by, bdir);
                const float casingX = m_player.x() + static_cast<float>(bdir) * 5.0f;
                m_effects.spawnCasing(casingX, by, bdir);
            }
            m_bullets.update(fixedDt, &m_world.map(), m_ground, m_camera.camera());
        }

        {
            HBE_PROFILE_SCOPE("Combat");
            m_enemies.checkBulletHits(m_bullets, &m_effects, 1);

            {
                std::vector<BulletManager::Impact> impacts;
                if (m_bullets.consumeImpacts(impacts)) {
                    for (const auto& imp : impacts) {
                        m_effects.spawnBulletImpact(imp.x, imp.y, imp.tileId);
                    }
                }
            }
        }

        // landedThisFrame() is a one-shot flag the fixed step sets and
        // clears. Reading it from onUpdate would miss the landing whenever
        // two steps ran in one frame, and double-count it whenever none did.
        if (m_player.landedThisFrame()) {
            m_effects.spawnLandingDust(m_player.x(), m_player.feetY(), m_player.groundTileId());
        }

        {
            HBE_PROFILE_SCOPE("AI");
            m_enemies.fixedUpdate(fixedDt);

            auto& ebm = m_enemies.enemyBullets();
            ebm.update(fixedDt, &m_world.map(), m_ground, m_camera.camera());

            {
                std::vector<EnemyBulletManager::Impact> impacts;
                if (ebm.consumeImpacts(impacts)) {
                    for (const auto& imp : impacts) {
                        m_effects.spawnEnemyBulletImpact(imp.x, imp.y, imp.tileId);
                    }
                }
            }

            {
                const AABB pb = m_player.hurtbox();
                for (auto& b : ebm.bullets()) {
                    if (!b.alive) continue;
                    if (std::fabs(b.x - pb.cx) > pb.w * 0.5f) continue;
                    if (std::fabs(b.y - pb.cy) > pb.h * 0.5f) continue;

                    const int kbDir = (b.vx >= 0.0f) ? +1 : -1;
                    if (m_player.takeDamage(b.damage, kbDir)) {
                        m_effects.spawnBloodSplatter(b.x, b.y, kbDir);
                        b.alive = false;
                    }
                }
            }
        }
    }
```

What changed, so you can review the paste rather than trust
it:

| Was | Now |
|---|---|
| `m_player.set*Input(...)` inside the `Physics` scope | in `onUpdate`, right after the input reads |
| `m_world.update(dt)` first line of `Physics` | in `onUpdate`, on the render clock |
| `m_player.update(dt)` | `m_player.fixedUpdate(fixedDt)` in `onFixedUpdate` |
| `m_enemies.update(dt)` | `m_enemies.fixedUpdate(fixedDt)` in `onFixedUpdate` |
| landing dust, between `Combat` and the walk dust | in `onFixedUpdate`, same relative position |
| `tickWalkDust` | `onUpdate`, unchanged arguments |
| camera block, between `Combat` and `AI` | `onUpdate`, after the particles |
| — | `m_player.updateVisual(dt)` / `m_enemies.updateVisual(dt)`, new |
| — | a `Timestep:` line in the `F8` dump, new |
| everything else | byte for byte identical |

> **NOTE ON THE PROFILER TREE.** `Physics`, `Combat` and
> `AI` used to nest under `SceneUpdate`, which nests under
> `ApplicationUpdate`. They now nest under `FixedUpdate`,
> which is a sibling of `ApplicationUpdate`. Item 15's CSV
> looks sections up **by name**, so `physicsMs`, `combatMs`
> and `aiMs` keep working unchanged — only the indentation
> in the 1-Hz log and the `F8` dump moves. `sceneUpdateMs`
> gets much smaller, because most of what it used to
> contain now lives in `FixedUpdate`. Captures taken before
> and after this item are not comparable on those two
> columns; that is expected, and it is why every capture
> should carry a `--capture-label`.

### 2.2 Replace `onRender`

`onRender` currently occupies lines 324-357 of the pristine
file. **Replace the whole function** with:

```cpp
    void GameLayer::onRender() {
        HBE_PROFILE_SCOPE("GameRender");

        Renderer2D& r2d = m_app->renderer2D();

        // Item 16: how far this frame sits between the last completed fixed
        // step and the next one, in [0, 1]. onRender is the ONLY place this
        // is valid -- it is latched when the step loop ends and stale during
        // onUpdate. Presentation only; nothing below feeds it into a
        // collision query or a game decision.
        const float alpha = m_app->interpolationAlpha();

        r2d.beginScene(m_camera.camera(), RenderPass::World);

        {
            HBE_PROFILE_SCOPE("TileRendering");
            m_world.render(r2d);
        }

        {
            HBE_PROFILE_SCOPE("SpriteRendering");
            m_enemies.render(r2d, alpha);
            m_enemies.renderBubbles(m_debug, r2d);
            m_player.render(r2d, alpha);
            m_bullets.render(r2d, alpha);
            m_enemies.enemyBullets().render(r2d, alpha);
            m_effects.render(r2d);
        }

        if (m_showHitBoxes) {
            m_enemies.debugDrawBoxes(m_debug, r2d);
            m_enemies.debugDrawSenses(m_debug, r2d);
            const HBE::Renderer::AABB pb = m_player.hurtbox();
            m_debug.rect(r2d, pb.cx, pb.cy, pb.w, pb.h, 0.35f, 0.55f, 1.0f, 1.0f, false);
            m_enemies.debugDrawBoxes(m_debug, r2d);
        }

        drawHud(r2d);

        r2d.endScene();
    }
```

`m_effects.render(r2d)` keeps its one-argument form —
particles integrate on the render clock and have nothing to
interpolate between. `renderBubbles` and the `m_showHitBoxes`
overlay draw from simulated positions on purpose: the hitbox
overlay exists to show you what combat actually tested
against, and interpolating it would make it lie.

---

## 3. `main.cpp` — the three flags

Open `/home/atulo/Projects/HBE/MegaX/src/main.cpp`.
**Tabs.** 52 lines.

### 3.1 The include

Lines 7-8 currently read:

```cpp
#include <cstring>
#include <memory>
```

Insert between them:

```cpp
#include <cstdlib>
```

Final block:

```cpp
#include <cstring>
#include <cstdlib>
#include <memory>
```

`std::atof` and `std::atoi` come from `<cstdlib>`.

### 3.2 The parsing

The body currently reads (lines 13-33):

```cpp
int main(int argc, char** argv) {
	SetLogLevel(LogLevel::Info);

	for (int i = 1; i < argc; ++i)
	{
		if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0)
		{
			MegaX::PrintCaptureUsage();
			return 0;
		}
	}

	const MegaX::PerfCaptureRequest captureRequest = MegaX::ParseCaptureArgs(argc, argv);

	WindowConfig cfg;
```

Insert right after the closing brace of the `--help` loop
(line 23) and before
`const MegaX::PerfCaptureRequest captureRequest = ...`
(line 25):

```cpp

	// --- item 16: timestep and render-rate overrides ---
	// Parsed here rather than in ParseCaptureArgs because these are engine
	// knobs, not capture knobs -- they change how the game runs whether or
	// not a capture is armed. 0 means "leave the engine default alone".
	float fixedHz = 0.0f;
	int   fixedCatchUp = 0;
	float renderHz = 0.0f;

	for (int i = 1; i < argc; ++i)
	{
		if (std::strcmp(argv[i], "--fixed-hz") == 0 && i + 1 < argc)
		{
			fixedHz = static_cast<float>(std::atof(argv[++i]));
			continue;
		}
		if (std::strcmp(argv[i], "--fixed-catchup") == 0 && i + 1 < argc)
		{
			fixedCatchUp = std::atoi(argv[++i]);
			continue;
		}
		if (std::strcmp(argv[i], "--render-hz") == 0 && i + 1 < argc)
		{
			renderHz = static_cast<float>(std::atof(argv[++i]));
			continue;
		}
	}
```

`std::atof` returns `0.0` for anything unparseable, which
lands on "leave the default alone" — the same degrade-don't-
explode policy `FixedTimestep::configure` follows. A typo
like `--fixed-hz sixty` gives you 60 Hz and a startup log
line saying so, not a crash.

### 3.3 Applying it

Lines 40-49 currently read:

```cpp
	Application app;
	if (!app.initialize(cfg, assetCfg)) {
		return -1;
	}

	auto gameLayer = std::make_unique<MegaX::GameLayer>();
	gameLayer->setCaptureRequest(captureRequest);
	app.pushLayer(std::move(gameLayer));
	
	app.run();
```

Insert right after the closing brace of the `initialize`
check (line 43) and before
`auto gameLayer = std::make_unique<MegaX::GameLayer>();`
(line 45):

```cpp

	// --- item 16 ---
	if (fixedHz > 0.0f || fixedCatchUp > 0)
	{
		FixedTimestepConfig ts{};   // engine defaults: 60 Hz, 5 steps, 0.25 s
		if (fixedHz > 0.0f) ts.hz = fixedHz;
		if (fixedCatchUp > 0) ts.maxCatchUpSteps = fixedCatchUp;
		app.setFixedTimestep(ts);
	}

	if (renderHz > 0.0f)
	{
		app.setTargetFrameRate(renderHz);
	}
```

Final `main` body from `Application app;` onward:

```cpp
	Application app;
	if (!app.initialize(cfg, assetCfg)) {
		return -1;
	}

	// --- item 16 ---
	if (fixedHz > 0.0f || fixedCatchUp > 0)
	{
		FixedTimestepConfig ts{};   // engine defaults: 60 Hz, 5 steps, 0.25 s
		if (fixedHz > 0.0f) ts.hz = fixedHz;
		if (fixedCatchUp > 0) ts.maxCatchUpSteps = fixedCatchUp;
		app.setFixedTimestep(ts);
	}

	if (renderHz > 0.0f)
	{
		app.setTargetFrameRate(renderHz);
	}

	auto gameLayer = std::make_unique<MegaX::GameLayer>();
	gameLayer->setCaptureRequest(captureRequest);
	app.pushLayer(std::move(gameLayer));
	
	app.run();

	return 0;
}
```

`FixedTimestepConfig` needs no qualification: `main.cpp`
already has `using namespace HBE::Core;` at line 10, and
`Application.h` (line 1) pulls in `Timestep.h`.

> **WHY `--render-hz` AT ALL?** Because the item's done
> criterion names four rendering rates and there is no other
> way to produce two of them. Vsync gives you your monitor's
> refresh; `--no-vsync` gives you whatever the machine
> manages. 30 and 120 need an actual cap. It is eleven lines
> of `Application` and it turns criterion 6 from "trust me"
> into a shell command.

---

## 4. `PerfCapture.cpp` — the usage text

Open `/home/atulo/Projects/HBE/MegaX/src/Game/PerfCapture.cpp`.
**4 spaces.**

`PrintCaptureUsage()` currently reads (lines 175-188):

```cpp
    void PrintCaptureUsage()
    {
        LogInfo("MegaX performance capture options:");
        ...
        LogInfo("  --no-vsync               run uncapped so the capture measures real headroom");
        LogInfo("  --help                   print this list and exit");
        LogInfo("In-game, F9 starts/stops a capture with the same settings.");
    }
```

Insert three lines between the `--no-vsync` line (185) and
the `--help` line (186):

```cpp
        LogInfo("  --fixed-hz <N>           gameplay simulation rate (default 60)");
        LogInfo("  --fixed-catchup <N>      max simulation steps per rendered frame (default 5)");
        LogInfo("  --render-hz <N>          cap the render rate (default uncapped)");
```

Final function:

```cpp
    void PrintCaptureUsage()
    {
        LogInfo("MegaX performance capture options:");
        LogInfo("  --capture                start a capture automatically after the warmup");
        LogInfo("  --capture-seconds <N>    how long to record (default 10)");
        LogInfo("  --capture-warmup <N>     seconds to run before recording (default 3)");
        LogInfo("  --capture-label <text>   label folded into the output filename");
        LogInfo("  --capture-out <path>     explicit .csv path (default: user data captures/)");
        LogInfo("  --capture-120            score the run against the 120 FPS budget");
        LogInfo("  --capture-quit           quit once the capture is written.");
        LogInfo("  --no-vsync               run uncapped so the capture measures real headroom");
        LogInfo("  --fixed-hz <N>           gameplay simulation rate (default 60)");
        LogInfo("  --fixed-catchup <N>      max simulation steps per rendered frame (default 5)");
        LogInfo("  --render-hz <N>          cap the render rate (default uncapped)");
        LogInfo("  --help                   print this list and exit");
        LogInfo("In-game, F9 starts/stops a capture with the same settings.");
    }
```

This is the only edit to a file item 15 created, and it is
three log lines. `ParseCaptureArgs` below it is **not**
touched: the three new flags do not start with `--capture`,
so its unknown-option warning at line 246 never fires for
them.

---

## 5. What NOT to touch

* **Do not move `tickPerfCapture` into `onFixedUpdate`.**
  Item 15 records **one CSV row per rendered frame** and
  keys it on `Profiler::Snapshot::frameIndex`. Called from
  the fixed step it would record zero rows on most frames
  and several on others, and the duplicate-frame guard
  (`m_lastFrameIndex`) would silently drop all but the
  first.
* **Do not move the F-key block into `onFixedUpdate`.**
  `IsKeyPressed` is a per-frame edge flag cleared by
  `Platform::Input::NewFrame()`. Read from two fixed steps
  in the same frame, one `F5` press would trigger two scene
  reloads.
* **Do not call `m_app->interpolationAlpha()` from
  `onUpdate` or `onFixedUpdate`.** Golden rule 6. It
  compiles and returns a plausible number, which is exactly
  what makes it dangerous.
* **Do not put `m_camera.update` on the fixed step.** The
  camera is presentation and its `followResponse` lerp is
  already frame-rate independent. On the fixed step it would
  visibly step at `--fixed-hz 30`.
* **Do not add `--fixed-maxframe`.** `maxFrameSeconds` is
  the one knob whose two useful values (0.25 for skip,
  `catchup/hz` for dilate) are a design decision, not a
  per-run experiment. Doc `07`'s tuning table changes it in
  code where the choice is visible and commented.
* **Do not touch `ParseCaptureArgs`.** The capture request
  and the engine knobs are different concerns; merging them
  would put `--render-hz` into `PerfCaptureRequest`, which
  is serialised into every `.meta.txt`'s notion of what a
  capture is.

---

## 6. Sanity check before doc `07`

```fish
cd /home/atulo/Projects/HBE/MegaX

grep -c 'onFixedUpdate' include/Game/GameLayer.h src/Game/GameLayer.cpp
# -> 1 and 2   (declaration; definition + the comment banner)

# The simulation moved wholesale — all three scopes are in onFixedUpdate:
awk '/void GameLayer::onFixedUpdate/,/^    }$/' src/Game/GameLayer.cpp | grep -c 'HBE_PROFILE_SCOPE'
# -> 3   (Physics, Combat, AI)

# ...and none of them stayed behind in onUpdate:
awk '/void GameLayer::onUpdate/,/void GameLayer::onFixedUpdate/' src/Game/GameLayer.cpp \
    | grep -c 'HBE_PROFILE_SCOPE'
# -> 2   (SceneUpdate, Particles)

# alpha is read exactly once, in onRender:
grep -n 'interpolationAlpha' src/Game/GameLayer.cpp
# -> 2 lines, both inside onRender (one is the F8 dump line — see below)

grep -c 'fixed-hz\|fixed-catchup\|render-hz' src/main.cpp
# -> 3   (grep -c counts lines, one per strcmp)

grep -c 'fixed-hz\|fixed-catchup\|render-hz' src/Game/PerfCapture.cpp
# -> 3
```

> **ON THAT `interpolationAlpha` COUNT.** It is 2, not 1 —
> the `F8` dump in `onUpdate` also prints it. That one is a
> *diagnostic read of a stale value*, which is fine and is
> the only sanctioned exception: `F8` is telling you where
> the accumulator sat at the end of the previous frame, and
> that is genuinely useful when a capture looks wrong.

**Should MegaX compile now?** Yes. Build everything:

```fish
cd /home/atulo/Projects/HBE
cmake --build --preset linux-clang-debug
```

Then run it and confirm the game still plays:

```fish
./build/linux-clang/bin/Debug/MegaX
```

Move, jump, shoot the enemy, take a hit, press `F5`. It
should feel the same as it did before this item — that is
the whole point. Doc `07` proves it properly.

Next: `07_build_run_and_verify.md`.
