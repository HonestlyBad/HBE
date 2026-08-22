# Doc 04 — Minimal Game Code (blank window)

Three tiny files. The engine does all the heavy lifting; MegaX just creates an
`Application`, pushes an empty `GameLayer`, and runs. The window is cleared by
the engine each frame (so it's blank) and **F11** is handled by the engine (so
fullscreen toggle works) — no game code required for either.

Create all three files, then **Add ▸ Existing Item…** them into the project (or
use **Show All Files ▸ Include In Project**).

---

## File 1 — `include\Game\GameLayer.h`

```cpp
#pragma once

#include "HBE/Core/Layer.h"

namespace HBE::Core { class Application; }

namespace MegaX {

    // The game's root layer. Empty for item 00 — the engine clears the window
    // and handles F11 fullscreen, so a blank window already meets the goal.
    // Player, camera, world, etc. get added here in later items.
    class GameLayer : public HBE::Core::Layer {
    public:
        void onAttach(HBE::Core::Application& app) override;
        void onUpdate(float dt) override;
        void onRender() override;
    };

} // namespace MegaX
```

---

## File 2 — `src\Game\GameLayer.cpp`

```cpp
#include "Game/GameLayer.h"

#include "HBE/Core/Application.h"
#include "HBE/Core/Log.h"

namespace MegaX {

    void GameLayer::onAttach(HBE::Core::Application& app) {
        (void)app; // unused for now
        HBE::Core::LogInfo("MegaX GameLayer attached.");
    }

    void GameLayer::onUpdate(float dt) {
        (void)dt; // nothing to update yet
    }

    void GameLayer::onRender() {
        // Intentionally empty for item 00.
        // The engine already cleared the window this frame, so we get a
        // clean blank window. Drawing starts in item 01 (Player ghost state).
    }

} // namespace MegaX
```

---

## File 3 — `src\main.cpp`

```cpp
#include "HBE/Core/Application.h"
#include "HBE/Core/Log.h"

#include "Game/GameLayer.h"

#include <memory>

using namespace HBE::Core;
using namespace HBE::Platform;

int main() {
    SetLogLevel(LogLevel::Info);

    WindowConfig cfg;
    cfg.title     = "MegaX";
    cfg.width     = 1280;
    cfg.height    = 720;
    cfg.useOpenGL = true;
    cfg.mode      = WindowMode::Windowed;
    cfg.vsync     = true;

    // Asset root + user-data (saves/settings) identity for this game.
    AssetPaths::Config assetCfg{};
    assetCfg.organization        = "MegaX";
    assetCfg.application          = "MegaX";
    assetCfg.siblingProjectNames = { "MegaX" };

    Application app;
    if (!app.initialize(cfg, assetCfg)) {
        LogFatal("MegaX: app.initialize failed.");
        return -1;
    }

    app.pushLayer(std::make_unique<MegaX::GameLayer>());
    app.run();

    return 0;
}
```

### Why these choices

- **`SetLogLevel(LogLevel::Info)`** — quieter than the Sandbox's `Trace`; bump
  to `Trace` while debugging if you want the full engine chatter.
- **`useOpenGL = true`** — the renderer needs a GL context (required).
- **`WindowMode::Windowed` + F11** — the engine toggles to
  `FullscreenDesktop` and back on F11 for you.
- **`assetCfg.organization/application = "MegaX"`** — user data (future saves,
  settings) goes to `%APPDATA%\MegaX\MegaX\`, separate from the engine/Sandbox.
- **`siblingProjectNames = { "MegaX" }`** — replaces the engine's default
  `{ "HBE.Sandbox" }` so the asset search never looks at the Sandbox. In
  practice the copy step already puts `assets\` next to the exe, so this is
  belt-and-suspenders.

> The three engine namespaces you touch: `HBE::Core` (Application, Log, Layer,
> AssetPaths) and `HBE::Platform` (WindowConfig, WindowMode). Both are pulled in
> transitively by `Application.h`.

---

## Verify (doc 04)

- [ ] The three files exist at the paths above and are **in the project**
      (`main.cpp` and `GameLayer.cpp` show under **Source Files**, `GameLayer.h`
      under **Header Files**).
- [ ] `#include "Game/GameLayer.h"` resolves (thanks to `$(ProjectDir)include\`
      on the include path from doc 02).
- [ ] No reference anywhere to `HBE.Sandbox` / `DemoGame` / the Sandbox's
      `GameLayer`.

Next: **`05_build_run_and_verify.md`**.
