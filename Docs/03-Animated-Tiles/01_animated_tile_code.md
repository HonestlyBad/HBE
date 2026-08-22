# Item 03 · Doc 01 — Animated‑tile code

Three MegaX files change. Replace each file's contents with the versions below
(they supersede the item‑02 versions of `AnimatedTile.h`, `World.h`, `World.cpp`).

```
MegaX/
  include/World/AnimatedTile.h    (replace)
  include/World/World.h           (replace)
  src/World/World.cpp             (replace)
```

No new files → **no `MegaX.vcxproj` edit needed** (these three are already
registered from item 02).

---

## `include/World/AnimatedTile.h`

We add the **runtime** members: each animation owns its own `Material` (mandatory
— see the batch gotcha in doc 00), caches its texture size, and keeps the list of
map cells where it appears.

```cpp
#pragma once

#include <string>
#include <vector>

#include "HBE/Renderer/Material.h"

namespace HBE::Renderer { class Texture2D; }

namespace MegaX {

    // A single map cell (tile coordinates) that shows an animated tile.
    struct AnimatedTileInstance {
        int tileX = 0;
        int tileY = 0;
    };

    // One animated tile that overrides a static tile in the world.
    //
    // A list of these is parsed from the map JSON "animatedTiles" array. Each
    // owns its spritesheet texture + a persistent Material, and remembers every
    // cell (tilesetIndex, tileId) it must overdraw with the current frame.
    struct AnimatedTile {
        // ---- From the map JSON "animatedTiles" entry ----

        // Which map tileset (0-based index into TileMap::tilesets) and the
        // 1-based LOCAL tile id this animation replaces.
        int tilesetIndex = 0;
        int tileId = 0;

        // Absolute path to the spritesheet (resolved at load, relative to the
        // map file). e.g. ".../tilesets/Military/Animated/tile-8_Anim_spritesheet.png"
        std::string sheetPath;

        // Frame grid inside the spritesheet. The Military animated sheets are
        // 128x64 => 4 cols x 2 rows => 8 frames of 32x32, playing 0..7.
        int   frameW = 32;
        int   frameH = 32;
        int   frameCount = 8;
        float fps = 8.0f;

        // ---- Runtime (filled by World::loadAnimatedTiles) ----

        // The sheet texture + its size in pixels (for UV math).
        HBE::Renderer::Texture2D* texture = nullptr;
        int texW = 0;
        int texH = 0;

        // Persistent material for THIS animation. The sprite batch binds the
        // material pointer at flush time, so every animation MUST own its own
        // material (never share + mutate one). uvRect is chosen per frame on the
        // RenderItem instead (that IS safe -- it's baked into vertices at submit).
        HBE::Renderer::Material material{};

        // Every cell in the map that shows this tile id (built once at load).
        std::vector<AnimatedTileInstance> instances{};
    };

}
```

---

## `include/World/World.h`

We store the sprite shader + quad mesh (needed to build render items each frame),
and declare the two new helpers. `Material` is visible here because `World.h`
includes `TileMapRenderer.h`, which includes `Material.h` (and `AnimatedTile.h`
now includes it directly too).

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

    // Owns a tile world: the loaded TileMap data + its GPU renderer, plus a set
    // of animated tiles that are overdrawn on top of the static layers.
    class World {
    public:
        // Load `logicalMapPath` (e.g. "maps/level_01.json"), build GPU state for
        // every tileset, and prepare animated tiles. On failure the world is left
        // empty but safe to update/render (draws nothing). Returns false on fail.
        bool load(HBE::Renderer::Renderer2D& r2d,
                  HBE::Renderer::ResourceCache& resources,
                  HBE::Renderer::GLShader* spriteShader,
                  HBE::Renderer::Mesh* quadMesh,
                  const std::string& logicalMapPath);

        // Advances animated-tile clocks.
        void update(float dt);

        // Draws all static tile layers, then overdraws animated tiles.
        // MUST be called inside an active beginScene(camera, World) pass.
        void render(HBE::Renderer::Renderer2D& r2d);

        bool loaded() const { return m_loaded; }
        const HBE::Renderer::TileMap& map() const { return m_map; }

        // World-space size of the map in pixels (max over layers). 0 if empty.
        float pixelWidth()  const;
        float pixelHeight() const;

    private:
        // Parse the map JSON's MegaX-specific "animatedTiles" array, load each
        // spritesheet as its own texture + material, and record every cell that
        // uses an animated tile id so we can overdraw it.
        void loadAnimatedTiles(HBE::Renderer::ResourceCache& resources,
                               const std::string& mapAbsPath);

        // Fill outUV = { u0, v0, uScale, vScale } for `frame` of an animated
        // sheet, mirroring TileMapRenderer's atlas UV math (incl. stb's vertical
        // flip and the half-texel inset that prevents bleeding).
        void computeFrameUV(const AnimatedTile& a, int frame, float outUV[4]) const;

        // Overdraw every animated cell with its current frame, at render layer 1
        // (above the static tiles at layer 0, below the player at layer 100).
        void renderAnimatedTiles(HBE::Renderer::Renderer2D& r2d);

        HBE::Renderer::TileMap         m_map{};
        HBE::Renderer::TileMapRenderer m_renderer{};
        bool                           m_loaded = false;

        // Kept from load() so we can build animated-tile render items each frame.
        HBE::Renderer::GLShader*       m_spriteShader = nullptr;
        HBE::Renderer::Mesh*           m_quadMesh = nullptr;

        // ---- Animated tiles ----
        std::vector<AnimatedTile>      m_animatedTiles{};
        float                          m_animClock = 0.0f;
    };

}
```

---

## `src/World/World.cpp`

The engine's `TileMapLoader` ignores the `"animatedTiles"` key, so `World`
re‑reads the map file with `nlohmann/json` (already on MegaX's include path via
`$(SolutionDir)external\nlohmann`) to parse it.

```cpp
#include "World/World.h"

#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/TileMapLoader.h"
#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Texture2D.h"

#include "HBE/Core/AssetPaths.h"
#include "HBE/Core/Log.h"

#include <json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>

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
        m_spriteShader = spriteShader;
        m_quadMesh = quadMesh;

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

        loadAnimatedTiles(resources, absPath);

        m_loaded = true;

        size_t instanceCount = 0;
        for (const auto& a : m_animatedTiles) instanceCount += a.instances.size();

        HBE::Core::LogInfo("World loaded '" + logicalMapPath + "' ("
            + std::to_string(m_map.tilesets.size()) + " tilesets, "
            + std::to_string(m_map.layers.size()) + " layers, "
            + std::to_string(m_animatedTiles.size()) + " animated tiles, "
            + std::to_string(instanceCount) + " instances).");
        return true;
    }

    void World::loadAnimatedTiles(ResourceCache& resources,
                                  const std::string& mapAbsPath) {
        m_animatedTiles.clear();

        // Re-read the map file: the engine loader drops unknown keys, so the
        // MegaX-specific "animatedTiles" array is parsed here.
        std::ifstream f(mapAbsPath);
        if (!f.is_open()) {
            HBE::Core::LogError("World: could not reopen map for animatedTiles: "
                + mapAbsPath);
            return;
        }

        nlohmann::json j;
        try {
            f >> j;
        } catch (const std::exception& e) {
            HBE::Core::LogError(std::string("World: animatedTiles JSON parse error: ")
                + e.what());
            return;
        }

        if (!j.contains("animatedTiles") || !j["animatedTiles"].is_array()) return;

        std::vector<AnimatedTile> defs;
        for (const auto& e : j["animatedTiles"]) {
            AnimatedTile a{};
            a.tilesetIndex = e.value("tileset", 0);
            a.tileId       = e.value("tileId", 0);
            a.frameW       = e.value("frameW", m_map.tileSizeW);
            a.frameH       = e.value("frameH", m_map.tileSizeH);
            a.frameCount   = e.value("frames", 1);
            a.fps          = e.value("fps", 8.0f);

            const std::string rel = e.value("sheet", std::string{});
            if (a.tileId <= 0 || a.frameCount <= 0 || rel.empty()) {
                HBE::Core::LogError("World: skipping invalid animatedTiles entry.");
                continue;
            }

            a.sheetPath = HBE::Core::AssetPaths::ResolveRelativeTo(mapAbsPath, rel);

            // Unique cache name per animation so distinct sheets never collide in
            // the resource cache (the item-01 cache-by-name bug).
            const std::string cacheName = "animtile_ts"
                + std::to_string(a.tilesetIndex) + "_id" + std::to_string(a.tileId);

            a.texture = resources.getOrCreateTextureFromFile(cacheName, a.sheetPath);
            if (!a.texture) {
                HBE::Core::LogError("World: failed to load animated sheet '"
                    + a.sheetPath + "'.");
                continue;
            }
            a.texW = a.texture->getWidth();
            a.texH = a.texture->getHeight();

            // Persistent, per-animation material (see doc 00 batch gotcha).
            a.material.shader  = m_spriteShader;
            a.material.texture = a.texture;

            // Find every cell using this tile id in a layer bound to its tileset.
            for (const auto& layer : m_map.layers) {
                if (layer.tilesetIndex != a.tilesetIndex) continue;
                for (int y = 0; y < layer.h; ++y) {
                    for (int x = 0; x < layer.w; ++x) {
                        if (layer.at(x, y) == a.tileId)
                            a.instances.push_back(AnimatedTileInstance{ x, y });
                    }
                }
            }

            if (a.instances.empty()) {
                HBE::Core::LogInfo("World: animated tile id "
                    + std::to_string(a.tileId) + " (tileset "
                    + std::to_string(a.tilesetIndex)
                    + ") is defined but not placed in any layer.");
            }

            defs.push_back(std::move(a));
        }

        m_animatedTiles = std::move(defs);
    }

    void World::update(float dt) {
        m_animClock += dt;
    }

    void World::render(Renderer2D& r2d) {
        if (!m_loaded) return;
        m_renderer.draw(r2d, m_map);
        renderAnimatedTiles(r2d);
    }

    void World::computeFrameUV(const AnimatedTile& a, int frame, float outUV[4]) const {
        const int cols  = (a.frameW > 0) ? (a.texW / a.frameW) : 1;
        const int col   = (cols > 0) ? (frame % cols) : 0;
        const int row   = (cols > 0) ? (frame / cols) : 0;
        const int origX = col * a.frameW;
        const int origY = row * a.frameH;

        // stb loads images top-down but GL samples bottom-up, so the engine flips
        // Y when computing atlas UVs. Mirror that here.
        const int loadedY = a.texH - (origY + a.frameH);

        // Half-texel inset to avoid sampling neighbouring frames (bleed).
        const float insetU = (a.texW > 0) ? 0.5f / (float)a.texW : 0.0f;
        const float insetV = (a.texH > 0) ? 0.5f / (float)a.texH : 0.0f;

        const float uMin = (float)origX          / (float)a.texW + insetU;
        const float vMin = (float)loadedY         / (float)a.texH + insetV;
        const float uMax = (float)(origX + a.frameW) / (float)a.texW - insetU;
        const float vMax = (float)(loadedY + a.frameH) / (float)a.texH - insetV;

        outUV[0] = uMin;           // u0
        outUV[1] = vMin;           // v0
        outUV[2] = uMax - uMin;    // uScale
        outUV[3] = vMax - vMin;    // vScale
    }

    void World::renderAnimatedTiles(Renderer2D& r2d) {
        if (m_animatedTiles.empty() || !m_quadMesh) return;

        const float tw = std::round(m_map.worldTileW());
        const float th = std::round(m_map.worldTileH());
        if (tw <= 0.0f || th <= 0.0f) return;

        RenderItem item{};
        item.mesh  = m_quadMesh;
        item.pass  = RenderPass::World;
        item.layer = 1;  // above static tiles (0), below player (100)
        item.transform.scaleX = tw;
        item.transform.scaleY = th;

        for (auto& a : m_animatedTiles) {
            if (!a.texture || a.instances.empty() || a.frameCount <= 0) continue;

            const long long ticks = (long long)std::floor(m_animClock * a.fps);
            const int frame = (int)(((ticks % a.frameCount) + a.frameCount)
                                    % a.frameCount);

            computeFrameUV(a, frame, item.uvRect);
            item.material = &a.material;  // persistent per-animation material

            for (const auto& inst : a.instances) {
                item.transform.posX = std::round(inst.tileX * tw + tw * 0.5f);
                item.transform.posY = std::round(inst.tileY * th + th * 0.5f);
                r2d.draw(item);
            }
        }
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

---

## Why it's shaped this way

- **`loadAnimatedTiles` re‑reads the JSON** because `TileMapLoader` silently drops
  the MegaX‑only `"animatedTiles"` key. We reuse `nlohmann/json` (already on the
  include path) and resolve the `sheet` path with `ResolveRelativeTo(mapFile, …)`
  — the same rule the engine uses for a tileset's `texture`, so paths stay
  relative to the map file.
- **Unique cache name per animation** (`animtile_ts0_id8`, …) avoids the
  cache‑by‑name collision that bit us in item 01 (two textures requested under
  the same name return the first one).
- **One `Material` per animation** is mandatory: the batch stores the *material
  pointer* and reads its texture at flush. Per‑frame `uvRect` on the `RenderItem`
  is safe because it's baked into the vertices at submit.
- **`computeFrameUV` mirrors `TileMapRenderer::computeTileUV`** exactly — same
  vertical flip and half‑texel inset — so animated frames line up pixel‑perfectly
  with the static grid.
- **Positioning matches `TileMapRenderer::draw`**: `round(x*tw + tw/2)` /
  `round(y*th + th/2)` with `scale = round(worldTile)`, so the overdraw sits
  exactly on the static cell.
- **`layer = 1`** guarantees animated tiles sort above every static layer (all at
  layer 0) and below the player (layer 100), regardless of texture — the sort key
  weights layer far above texture.
- **Frame index uses `long long` + double‑modulo** so it never overflows on long
  sessions and never goes negative.
