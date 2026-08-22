# 03 — FileWatcher wiring + shader hot reload (F6)

Doc 02 gave you `F5` / `F7` and everything they need. Doc 03
finishes Item 11 by:

1. Adding **F6** = sprite shader hot reload (calls
   `ResourceCache::reloadShader("sprite")`).
2. Setting up `HBE::Core::FileWatcher` in `onAttach` to watch
   the tilemap JSON and both sprite shader source files.
3. Polling the watcher from `onUpdate` so those watches fire.

None of this requires engine changes — `FileWatcher` and
`ResourceCache::reloadShader(...)` are already in the engine
(see the memory: *"HBE::Core::FileWatcher polls files with
debounce; used for hot-reloading shaders / tilemaps in
Sandbox."*).

---

## 1. `include/Game/GameLayer.h` — one more method + two more path caches

Open `G:\Dev\HBE\MegaX\include\Game\GameLayer.h` again.

### 1a. Add the shader hot-reload helper decl

Inside the private block, right below the doc 02 declarations,
add:

```cpp
        void spawnDemoEnemies();                // Item 11
        bool reloadScene(bool alsoReloadMap);   // Item 11
        void clearTransientEntities();          // Item 11
        void hotReloadShader();                 // Item 11 (F6 + watcher)
        void setupHotReloadWatches();           // Item 11 (called from onAttach)
```

### 1b. Add the shader path caches

Right below the existing `m_tileMapPath` cache (from doc 02):

```cpp
        std::string m_tileMapPath  = "maps/level_01.json";       // Item 11
        std::string m_spriteVsPath = "shaders/sprite.vert";      // Item 11
        std::string m_spriteFsPath = "shaders/sprite.frag";      // Item 11
        float m_startX = 0.0f;                                    // Item 11
        float m_startY = 0.0f;                                    // Item 11
```

The literal paths match what `buildSpritePipeline()` already
passes to `resources.getOrCreateShaderFromFiles(...)` — grep
`sprite.vert` in `GameLayer.cpp` to confirm they're the same
strings you're loading against. If your project stores the
shaders under a different logical folder, use the same string
here.

### 1c. Confirm the `FileWatcher` include + member (added in doc 02)

You already added these in doc 02 §1c:

```cpp
#include "HBE/Core/FileWatcher.h"
...
        HBE::Core::FileWatcher m_watcher{};             // Item 11
```

Sanity-check they're still there. If you skipped doc 02 §1c,
add them now.

---

## 2. `src/Game/GameLayer.cpp` — `hotReloadShader()`

Add this helper right below `clearTransientEntities()` (from
doc 02), still inside `namespace MegaX {`:

```cpp
    void GameLayer::hotReloadShader() {
        if (!m_app) return;

        const bool ok = m_app->resources().reloadShader("sprite");
        if (ok) {
            LogInfo("Shader reloaded: sprite");
        } else {
            LogError("Shader reload FAILED: sprite (see previous log for GLSL error).");
        }
    }
```

Why this is safe:

* `ResourceCache::reloadShader("sprite")` recompiles the shader
  in place — the `GLShader*` that `m_spriteShader` points at
  stays valid. Every material that already binds the shader
  keeps working with no rebinding needed.
* On failure the engine keeps the previously compiled shader
  live, so a broken save doesn't turn the whole scene black.

---

## 3. `src/Game/GameLayer.cpp` — `setupHotReloadWatches()`

Add this helper right below `hotReloadShader()`:

```cpp
    void GameLayer::setupHotReloadWatches() {
        HBE::Core::FileWatcher::Options opt{};
        opt.pollIntervalSeconds = 0.20f;
        opt.debounceSeconds     = 0.25f;
        m_watcher.setOptions(opt);

        namespace ap = HBE::Core::AssetPaths;

        // Tilemap JSON -> full scene reload.
        m_watcher.watchFile(ap::Resolve(m_tileMapPath),
            [this](const std::string&) {
                LogInfo("MegaX: tilemap file changed on disk -> scene reload.");
                reloadScene(/*alsoReloadMap=*/true);
            });

        // Sprite shader vert / frag -> shader hot reload.
        m_watcher.watchFile(ap::Resolve(m_spriteVsPath),
            [this](const std::string&) {
                LogInfo("MegaX: shader file changed on disk -> hot reload.");
                hotReloadShader();
            });
        m_watcher.watchFile(ap::Resolve(m_spriteFsPath),
            [this](const std::string&) {
                LogInfo("MegaX: shader file changed on disk -> hot reload.");
                hotReloadShader();
            });

        LogInfo("MegaX GameLayer: scene watches active (" + m_tileMapPath + ", sprite shader).");
    }
```

**Do NOT** add these watches from a place other than
`onAttach` — the watcher captures `this`, and a duplicate call
(e.g. from `reloadScene`) would double-register every file.
The watcher's `watchFile(...)` replaces an existing entry for a
given path, so double-adding wouldn't crash, but it would fire
the callback twice per change.

The `AssetPaths` include is already at the top of
`GameLayer.cpp` (Item 04+ uses it for shader paths); double-
check `#include "HBE/Core/AssetPaths.h"` is still there. If
not, add it now.

---

## 4. `src/Game/GameLayer.cpp` — call `setupHotReloadWatches()` from `onAttach`

Open `GameLayer::onAttach(HBE::Core::Application& app)`.

Find the current last line of the body:

```cpp
        spawnDemoEnemies();

        LogInfo("MegaX GameLayer attached (Play mode; press G for Ghost).");
    }
```

Add the watcher setup call **immediately below** the
`LogInfo(...)` line, still inside the `}` that closes
`onAttach`:

```cpp
        spawnDemoEnemies();

        LogInfo("MegaX GameLayer attached (Play mode; press G for Ghost).");

        setupHotReloadWatches();   // Item 11: after all init is done.
    }
```

Placement matters. `setupHotReloadWatches()` reads
`m_tileMapPath` and the shader path members and resolves them
with `AssetPaths::Resolve`. Those members are already
constructed by the time `onAttach` runs (they're default-
initialized on the class), so calling the setup any time
during / after init is fine — end of `onAttach` is the
cleanest spot.

---

## 5. `src/Game/GameLayer.cpp` — poll the watcher every frame

Open `GameLayer::onUpdate(float dt)`.

Find the first line of the update body. Right now that's:

```cpp
    void GameLayer::onUpdate(float dt) {
        // horizontal run (both modes)
        const float ix = ...
```

Add the poll **as the very first line inside the function**:

```cpp
    void GameLayer::onUpdate(float dt) {
        m_watcher.poll(dt);

        // horizontal run (both modes)
        const float ix = ...
```

Why the very top of the function:

* If a watcher callback fires a `reloadScene(true)`, everything
  after `poll(dt)` runs against the freshly-reloaded scene
  (correct behavior — a mid-frame reload should behave the
  same as pressing F5 at the start of the frame).
* Polling *after* the input/AI/render loop would leave one
  frame of stale enemies visible before the reload took
  effect. Not a bug, but visibly jankier.

---

## 6. `src/Game/GameLayer.cpp` — F6 hotkey (shader hot reload)

Still in `onUpdate`, find the F5 / F7 block you added in doc 02:

```cpp
        if (Input::IsKeyPressed(SDL_SCANCODE_F5)) {
            LogInfo("MegaX: scene reload requested (F5).");
            reloadScene(/*alsoReloadMap=*/true);
        }
        if (Input::IsKeyPressed(SDL_SCANCODE_F7)) {
            LogInfo("MegaX: soft respawn requested (F7).");
            reloadScene(/*alsoReloadMap=*/false);
        }
```

Insert the F6 handler **between** the F5 and F7 blocks (keeps
the three in numeric order and makes future readers less
confused):

```cpp
        if (Input::IsKeyPressed(SDL_SCANCODE_F5)) {
            LogInfo("MegaX: scene reload requested (F5).");
            reloadScene(/*alsoReloadMap=*/true);
        }
        if (Input::IsKeyPressed(SDL_SCANCODE_F6)) {
            LogInfo("MegaX: sprite shader hot reload requested (F6).");
            hotReloadShader();
        }
        if (Input::IsKeyPressed(SDL_SCANCODE_F7)) {
            LogInfo("MegaX: soft respawn requested (F7).");
            reloadScene(/*alsoReloadMap=*/false);
        }
```

---

## 7. Optional: also refresh the shader watch after a reload

If you ever hit a case where the shader watch stops firing
after a *manual* shader edit (very rare — the FileWatcher
recovers by itself on the next poll), the fix is to reset the
watcher's cached write-time by re-calling `watchFile` for that
path. You do **not** need to do this for Item 11; the engine
watcher already handles atomic-save behavior. Just be aware of
the escape hatch if you ever chase a "why didn't the watch
fire?" bug.

---

## 8. Sanity check at end of doc 03

At this point:

* `F5` full scene reload — works (doc 02).
* `F6` sprite shader hot reload — works (this doc).
* `F7` soft respawn — works (doc 02).
* Save `MegaX\assets\maps\level_01.json` — reload fires
  automatically within ~0.25s.
* Save `MegaX\assets\shaders\sprite.frag` — shader reload
  fires automatically within ~0.25s.

If the watcher never fires:

| Symptom | Cause | Fix |
|---|---|---|
| Log says "scene watches active" but nothing fires on save | The file being edited is not the one the watcher resolved. Windows sometimes edits a shadow copy. | Grep for the file path the watcher logged. `AssetPaths::Resolve("maps/level_01.json")` should resolve to `MegaX/assets/maps/level_01.json` inside the build's working directory (not the source folder). The `<MegaXAsset>` copy rule already copies your source assets there each build; if you're editing the *source* copy but the game reads the *copied* copy, you need to edit the copied copy or rebuild after editing. Easiest fix: run `msbuild ... /t:MegaX` after an asset edit; the build's copy step will overwrite the runtime asset, and the watcher will fire. |
| "setupHotReloadWatches" never logs | You forgot to call `setupHotReloadWatches()` at the bottom of `onAttach`. Re-check doc 03 §4. |
| Watcher fires every frame in a loop | Debounce is 0.0. Verify `opt.debounceSeconds = 0.25f;` is in `setupHotReloadWatches`. |
| Shader reload logs "failed" but the shader is correct | You saved during a partial write. Save again. If persistent, the shader has a real GLSL error — check the preceding log line for the compiler output. |

---

Next: `04_build_run_and_verify.md` — the build command, the
verify checklist, and the troubleshooting matrix.
