# HBE project map

Repo root: `/home/atulo/Projects/HBE`

Verified against the tree at item 14 (commit `e805921`). If a file you
need isn't here, the tree has moved on — list it and update this file.

---

## Layering

```
HBE.Core            foundations, no engine deps
   ^
HBE.Platform.SDL    SDL3 window / input / audio
   ^
HBE.Renderer.GL     2D GL renderer
   ^
MegaX  HBE.Sandbox  HBMapMaker
```

`HBE.Core` links `hbe::glm` + `hbe::nlohmann_json` publicly and
`hbe::sdl3` privately, and privately includes the sibling engine
headers (`Application.h` pulls in `SDLPlatform.h`, `GLRenderer.h`,
`Renderer2D.h`, `ResourceCache.h`). So the "Core has no deps" rule is
about *concepts*, not includes — Core already sees Platform and
Renderer headers.

**The rule that matters:** nothing in an `HBE.*` project may include
or know about `MegaX/`.

---

## HBE.Core

`include/HBE/` — public headers · `src/` — implementation

| Header | Owns |
|---|---|
| `Core/Application.h` | `Application` — window, layers, main loop, letterbox viewport, `platform()/audio()/gl()/renderer2D()/resources()` accessors |
| `Core/AssetPaths.h` | asset root resolution, `HBE_ASSET_ROOT` override, user-data root |
| `Core/Event.h` | `Event` base + window/key/mouse event types |
| `Core/EventBus.h` | typed pub/sub bus |
| `Core/FileWatcher.h` | mtime polling for hot reload |
| `Core/Layer.h` | `Layer` — `onAttach(Application&)`, `onUpdate(float)`, `onRender()`, `onEvent(Event&)` |
| `Core/LayerStack.h` | `LayerStack`, public member `m_layers` |
| `Core/Log.h` | `LogTrace/LogInfo/LogWarn/LogError/LogFatal(std::string_view)`, `SetLogLevel` |
| `Core/Profiler.h` | **item 13/14** — CPU scopes, GPU timings, renderer stats, `Snapshot` |
| `Core/Time.h` | `GetTimeSeconds()` |
| `Core/Types.h`, `Core/UUID.h` | scalar aliases, UUID |
| `ECS/Registry.h`, `ECS/Entity.h` | ECS |
| `ECS/Components.h`, `ECS/RuntimeComponents.h`, `ECS/ESCSComponents2D.h` | components (note the typo in `ESCSComponents2D.h` — it is real, don't "fix" it in a path) |
| `ECS/CombatComponents.h`, `CombatEvents.h`, `CombatSystem.h` | hitbox/hurtbox combat |
| `Events/GameplayEvents.h` | gameplay event payloads |
| `Input/InputMap.h` | high-level action/axis mapping, `HBE::Input::NewFrame()` |

Sources: `src/Core/{Application,AssetPaths,EventBus,FileWatcher,LayerStack,Log,Profiler,Time,UUID}.cpp`,
`src/ECS/{CombatEvents,CombatSystem}.cpp`, `src/Input/InputMap.cpp`.

### The main loop — `src/Core/Application.cpp`, `Application::run()` at line 323

Order inside `while (m_running)`:

1. `Profiler::BeginFrame()` · `GpuTimer::NewFrame()`
2. `Platform::Input::NewFrame()` · `HBE::Input::NewFrame()`
3. `m_platform.pumpEvents(...)` → `handleSDLEvent`; on quit, `EndFrame()` then `break`
4. `dt` from `GetTimeSeconds()`, clamped to `[0, 0.25]`
5. `HBE_PROFILE_SCOPE("ApplicationUpdate")` around the layer `onUpdate` loop
6. `HBE_PROFILE_SCOPE("Audio")` around `m_audio.update(dt)`
7. `m_renderer2D.resetFrameStats()`
8. `m_gl.beginFrameFullWindow(...)` · `m_gl.beginFrameInViewport(...)`
9. layer `onRender()` loop (**not** profiled at engine level — games name their own render scopes)
10. `m_gl.endFrame(m_platform)`
11. copy `Renderer2D::getStats()` → `Profiler::PublishRendererStats`
12. `Profiler::EndFrame()`
13. 1-Hz `LogInfo` dump behind `HBE_PROFILER_LOG_ONCE_PER_SECOND`
14. `m_platform.delayMillis(1)`

This loop is the insertion point for items 16 (fixed timestep) and
anything frame-scoped. Cite line numbers from a fresh read — this
function has changed in three consecutive items.

---

## HBE.Platform.SDL

| Header | Owns |
|---|---|
| `Platform/SDLPlatform.h` | `WindowConfig`, window + GL context, `pumpEvents`, `swapBuffers`, `delayMillis`, fullscreen |
| `Platform/Input.h` | `HBE::Platform::Input` — `IsKeyPressed(SDL_SCANCODE_*)`, `IsKeyDown`, mouse, `NewFrame()` |
| `Platform/Audio.h` | SDL3_mixer wrapper, `update(dt)`, positional audio |
| `Platform/GraphicsSettings.h` | vsync / resolution settings |

---

## HBE.Renderer.GL

Vendored `glad` lives at `HBE.Renderer.GL/external/glad/` (generated,
not in git — see `CACHYOS_SETUP.md` §1.4).

| Header | Owns |
|---|---|
| `Renderer/GLRenderer.h` | backend: context init, viewport/letterbox, `beginFrameFullWindow`, `beginFrameInViewport`, `endFrame`, post-process hookup |
| `Renderer/Renderer2D.h` | batching facade. `Renderer2DStats` (drawCalls, quads, stateChanges, passes, submittedQuads, renderedQuads, culledSprites, materialChanges, textureChanges), `getStats`, `resetFrameStats`, `addCulledSprites`, `beginScene(cam[, pass])`, `endScene`, `draw`, `drawDirect` |
| `Renderer/GpuTimer.h` | **item 14** — `Initialize/Shutdown/IsSupported/NewFrame`, `BeginScope/EndScope`, RAII `Scope`, `HBE_GPU_SCOPE(NAME)`, 3-frame result latency |
| `Renderer/Camera2D.h`, `CameraController.h` | camera + follow/lerp |
| `Renderer/Sprite2D.h`, `SpriteRenderer2D.h`, `SpriteBatch2D.h` | sprites + batching |
| `Renderer/SpriteAnimationStateMachine.h`, `AnimationPresetRegistry.h` | animation |
| `Renderer/TileMap.h`, `TileMapLoader.h`, `TileMapRenderer.h`, `TileCollision.h` | tilemaps; `TileMapRenderer::visibleTileChunks()`; `TileCollision::moveAndCollideEx` |
| `Renderer/ParticleSystem.h`, `ParticleEmitter.h`, `EmitterConfig.h`, `ParticleTypes.h` | particles |
| `Renderer/Scene2D.h`, `SceneSerializer.h`, `Prefab.h`, `PrefabLibrary.h`, `ScriptRegistry.h` | scene + serialization |
| `Renderer/PostProcessStack.h`, `Framebuffer.h` | post-process |
| `Renderer/TextRenderer2D.h`, `Font.h`, `DebugFont8x8.h` | text |
| `Renderer/DebugDraw2D.h` | debug shapes |
| `Renderer/Texture2D.h`, `GLShader.h`, `Mesh.h`, `Material.h`, `Color.h`, `Transform2D.h`, `RenderItem.h`, `RenderPass.h`, `ResourceCache.h` | primitives + asset cache |
| `Renderer/UI/UIContext.h`, `UIRect.h`, `UIStyle.h`, `UIThemeLoader.h` | immediate-mode UI |

GPU scopes currently opened by `GLRenderer`: `"GpuFrame"` and
`"GpuScene"`, begun in `GLRenderer.cpp:270-271`, ended in
`endFrame()` at lines 106 and 113.

---

## MegaX — the game

```
MegaX/
  src/main.cpp                 Application setup, pushes GameLayer
  src/Game/GameLayer.cpp       578 lines — the hub
  src/Game/Player.cpp          422
  src/Game/Enemy.cpp           582
  src/Game/EnemyManager.cpp    322
  src/Game/Bullet.cpp          91
  src/Game/EnemyBullet.cpp     111
  src/Game/Effects.cpp         839 — named particle effect registry
  src/World/World.cpp          tilemap + animated tiles wrapper
  include/Game/*.h  include/World/*.h
  assets/{maps,shaders,sprites,tilesets}
```

`GameLayer` owns: `CameraController m_camera`, `Player m_player`,
`World m_world`, `BulletManager m_bullets`, `Effects m_effects`,
`EnemyManager m_enemies`, `FileWatcher m_watcher`, `DebugDraw2D m_debug`,
`Difficulty m_difficulty`.

Key `GameLayer.cpp` landmarks (re-verify line numbers before citing):

| Line | What |
|---|---|
| 30 | `onAttach` |
| 94 | `onUpdate` — opens `HBE_PROFILE_SCOPE("SceneUpdate")` at 95 |
| 134–171 | the F-key block |
| 225 / 247 / 276 / 308 | `Physics` / `Combat` / `AI` / `Particles` scopes |
| 313–314 | `PublishParticleStats` / `PublishLightStats(0,0)` |
| 317 | `onRender` — `TileRendering` (323), `SpriteRendering` (328) |
| 350 | `buildSpritePipeline` |
| 429 | `drawHud` |
| 528–566 | `clearTransientEntites` (sic), `hotReloadShader`, `setupHotReloadWatches`, `spawnDemoEnemies` |

MegaX has **no lighting** yet — items 23–27 add it engine-side first.

---

## Other consumers

* `HBE.Sandbox` — `src/{main,GameLayer,DevConsole,ParticleEffects}.cpp`.
  Legacy scratchpad; superseded by MegaX. Do not touch unless named.
* `HBMapMaker` — `src/{main,Editor/Editor,IO/SceneIO,Platform/FileDialog}.cpp`,
  needs `hbe::imgui` (skipped by CMake if `external/imgui` is absent).
  Only touch when map format / rendering changes require it, and only
  when the user says so.

---

## Backlog shape (`Docs/WorkItems.txt`)

* 0–12 — MegaX vertical slice: player, tilemaps, physics, shooting, particles, enemies, AI, difficulty, hot reload. **Done.**
* 13–15 — profiling: CPU timing (done), GPU timing + renderer stats (done), CSV perf capture (**next**).
* 16–19 — engine hardening: fixed timestep, deterministic RNG streams, game-defined input IDs, component serialization.
* 20 — the "golden room" benchmark level.
* 21–28 — the lighting arc: parallax, lit materials, point/cone lights, occluders, shadow masks, lit composition, bloom + grading.
* 29–30 — polish and optimize the golden room.
* 31–50 — roguelike run systems: RunState, stats, items, loot, shops, bosses, modifiers.
* 51–58 — procedural generation.
* 59–63 — meta-progression, validation, final performance pass.
