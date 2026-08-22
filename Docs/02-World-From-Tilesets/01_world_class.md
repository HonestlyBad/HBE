# Item 02 · Doc 01 — The `World` class

Create three files under MegaX. They wrap the engine's tilemap system and add a
documented seam for animated tiles (item 03).

```
MegaX/
  include/World/AnimatedTile.h    (new)
  include/World/World.h           (new)
  src/World/World.cpp             (new)
```

> These use the same `include/` + `src/` split as `Game/` from item 01, and rely
> on the project include dir `$(ProjectDir)include\`, so `#include "World/..."`
> resolves.

---

## `include/World/AnimatedTile.h`

Pure data. Item 02 defines it; **item 03 fills a list of these** from the map
JSON and renders them. Left unused (but present) in item 02.

```cpp
#pragma once

#include <string>

namespace HBE::Renderer { class Texture2D; }

namespace MegaX {

    // One animated tile that overrides a static tile in the world.
    //
    // ITEM 03 will: parse a list of these from the map JSON "animatedTiles"
    // array, load each spritesheet as its own texture, and (in World::render)
    // overdraw every cell whose (tilesetIndex, tileId) matches with the current
    // animation frame. Item 02 only defines the record + reserves the seam.
    struct AnimatedTile {
        // Which map tileset (0-based index into TileMap::tilesets) this
        // animation belongs to, and the 1-based LOCAL tile id it replaces.
        int tilesetIndex = 0;
        int tileId = 0;

        // Logical path (asset-root relative) to the animation spritesheet,
        // e.g. "tilesets/Military/Animated/tile-8_Anim_spritesheet.png".
        std::string sheetPath;

        // Frame grid inside the spritesheet. The Military animated sheets are
        // 128x64 => 4 cols x 2 rows => 8 frames of 32x32.
        int   frameW = 32;
        int   frameH = 32;
        int   frameCount = 8;
        float fps = 8.0f;

        // Runtime handle, filled when the sheet is loaded (item 03). Null until
        // then.
        HBE::Renderer::Texture2D* texture = nullptr;
    };

}
```

---

## `include/World/World.h`

```cpp
#pragma once

#include <string>
#include <vector>

#include "HBE/Renderer/TileMap.h"
#include "HBE/Renderer/TileMapRenderer.h"

#include "World/AnimatedTile.h"

namespace HBE::Renderer {
    class Renderer2D;
    class ResourceCache;
    class GLShader;
    class Mesh;
}

namespace MegaX {

    // Owns a tile world: the loaded TileMap data + its GPU renderer.
    // GameLayer keeps one World, loads it once, updates it each frame, and
    // renders it inside the World render pass (behind the player).
    class World {
    public:
        // Load `logicalMapPath` (e.g. "maps/level_01.json") and build GPU state
        // for every tileset. On failure the world is left empty but safe to
        // update/render (draws nothing). Returns false on failure.
        bool load(HBE::Renderer::Renderer2D& r2d,
                  HBE::Renderer::ResourceCache& resources,
                  HBE::Renderer::GLShader* spriteShader,
                  HBE::Renderer::Mesh* quadMesh,
                  const std::string& logicalMapPath);

        // Advances animated-tile clocks. Safe no-op until item 03 adds tiles.
        void update(float dt);

        // Draws all static tile layers (and, from item 03, animated tiles).
        // MUST be called inside an active beginScene(camera, World) pass.
        void render(HBE::Renderer::Renderer2D& r2d);

        bool loaded() const { return m_loaded; }
        const HBE::Renderer::TileMap& map() const { return m_map; }

        // World-space size of the map in pixels (max over layers), for camera
        // bounds / spawn placement. Returns 0 if nothing is loaded.
        float pixelWidth()  const;
        float pixelHeight() const;

    private:
        // ITEM 03 seam: overdraw animated cells with their current frame.
        // Intentionally empty in item 02.
        void renderAnimatedTiles(HBE::Renderer::Renderer2D& r2d);

        HBE::Renderer::TileMap         m_map{};
        HBE::Renderer::TileMapRenderer m_renderer{};
        bool                           m_loaded = false;

        // ---- Animated-tile prep (populated in item 03) ----
        std::vector<AnimatedTile>      m_animatedTiles{};
        float                          m_animClock = 0.0f;
    };

}
```

---

## `src/World/World.cpp`

```cpp
#include "World/World.h"

#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/TileMapLoader.h"

#include "HBE/Core/AssetPaths.h"
#include "HBE/Core/Log.h"

#include <algorithm>

using namespace HBE::Renderer;

namespace MegaX {

    bool World::load(Renderer2D& r2d,
                     ResourceCache& resources,
                     GLShader* spriteShader,
                     Mesh* quadMesh,
                     const std::string& logicalMapPath) {
        m_loaded = false;
        m_map = TileMap{};
        m_animatedTiles.clear();
        m_animClock = 0.0f;

        const std::string absPath = HBE::Core::AssetPaths::Resolve(logicalMapPath);

        std::string err;
        if (!TileMapLoader::loadFromJsonFile(absPath, m_map, &err)) {
            HBE::Core::LogError("World::load: failed to load '" + logicalMapPath
                + "': " + err);
            m_map = TileMap{};
            return false;
        }

        // build() prefixes each tileset cache key with "tileset_" + name, so our
        // Military_tileset_1/2/3 names stay unique across the resource cache.
        if (!m_renderer.build(r2d, resources, spriteShader, quadMesh, m_map)) {
            HBE::Core::LogError("World::load: TileMapRenderer::build failed for '"
                + logicalMapPath + "'.");
            return false;
        }

        // ITEM 03: parse m_map's "animatedTiles" and load each sheet here.

        m_loaded = true;
        HBE::Core::LogInfo("World loaded '" + logicalMapPath + "' ("
            + std::to_string(m_map.tilesets.size()) + " tilesets, "
            + std::to_string(m_map.layers.size()) + " layers).");
        return true;
    }

    void World::update(float dt) {
        // Drives animated-tile frames in item 03. Harmless until then.
        m_animClock += dt;
    }

    void World::render(Renderer2D& r2d) {
        if (!m_loaded) return;
        m_renderer.draw(r2d, m_map);
        renderAnimatedTiles(r2d);
    }

    void World::renderAnimatedTiles(Renderer2D& /*r2d*/) {
        // ITEM 03: for each AnimatedTile, compute the current frame from
        // m_animClock * fps, then overdraw every matching cell in its layer(s)
        // using the animation sheet's UVs. Intentionally empty in item 02.
    }

    float World::pixelWidth() const {
        if (!m_loaded || m_map.layers.empty()) return 0.0f;
        int maxW = 0;
        for (const auto& l : m_map.layers) maxW = std::max(maxW, l.w);
        return maxW * m_map.worldTileW();
    }

    float World::pixelHeight() const {
        if (!m_loaded || m_map.layers.empty()) return 0.0f;
        int maxH = 0;
        for (const auto& l : m_map.layers) maxH = std::max(maxH, l.h);
        return maxH * m_map.worldTileH();
    }

}
```

### Why it's shaped this way

- **`World` owns both `TileMap` and `TileMapRenderer`** — the exact pair the
  Sandbox keeps, but packaged so `GameLayer` stays small and item 03 has one
  obvious place to add animation.
- **`load` takes `Renderer2D&`** because `TileMapRenderer::build(...)`'s first
  parameter is a `Renderer2D&` (even though the current impl doesn't use it, we
  pass `app.renderer2D()` exactly like the Sandbox).
- **Failure is soft**: a missing/broken map logs an error and leaves an empty
  world; `render`/`update` become no-ops so the game still runs (black screen +
  player), which is handy while you iterate on the JSON.
- **`m_animClock` / `m_animatedTiles` / `renderAnimatedTiles`** are the reserved
  item‑03 seam. They compile as harmless no-ops now.
