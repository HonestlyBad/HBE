# Item 07 · Doc 01 — Effect Configs & the `Effects` Wrapper

Two new files. Create them first, then wire them up in doc 03.

Everything here is game code — no engine edits. All configs are pure
`HBE::Renderer::EmitterConfig` data; the engine already knows how to
simulate and render them.

---

## Coordinate reminder

- **+Y is up** in engine world space. Use **negative `gravityY`** to pull
  particles down.
- Directions are in **degrees**, `0° = +x (right)`, `90° = +y (up)`.
- All sizes are in **world pixels** (the sprite scale factor is 1.0 in
  MegaX; the player sheet is 75×48 rendered 1:1, and tiles are 32×32).

---

## 1. `include/Game/Effects.h` (new)

> **Important include hygiene.** The engine currently ships **two**
> headers that both define `class HBE::Renderer::SpriteRenderer2D`:
> `HBE/Renderer/Sprite2D.h` (used by `Player.h`) **and**
> `HBE/Renderer/SpriteRenderer2D.h` (pulled in transitively by
> `HBE/Renderer/ParticleSystem.h` → `Scene2D.h` →
> `SpriteAnimationStateMachine.h`). Any TU that sees both fails with
> `C2011: 'SpriteRenderer2D' redefinition`. Until the engine pass
> consolidates them, we keep `ParticleSystem.h` **out** of `Effects.h`
> and hold the system as a `std::unique_ptr` — a pimpl-lite. Only
> `Effects.cpp` includes the real `ParticleSystem.h`.

```cpp
#pragma once

// See note above: do NOT include ParticleSystem.h here — it would drag
// SpriteRenderer2D.h into every TU that includes Effects.h (e.g.
// GameLayer.cpp), which already sees a different SpriteRenderer2D via
// Player.h -> Sprite2D.h.

#include <array>
#include <memory>
#include <unordered_map>

namespace HBE::Renderer {
    class Renderer2D;
    class ResourceCache;
    class Mesh;
    class ParticleSystem;
    struct TileMap;
}

namespace MegaX {

    // Owns the game's ParticleSystem, registers every effect once, and offers
    // small "spawn" helpers that patch colour into the config before spawn.
    //
    // Lifetime: initialize AFTER the sprite pipeline (quad mesh + shader) is
    // built AND after World::load has succeeded (so we can sample tile colours).
    class Effects {
    public:
        Effects();
        ~Effects();

        Effects(const Effects&) = delete;
        Effects& operator=(const Effects&) = delete;

        bool init(HBE::Renderer::ResourceCache& resources,
                  HBE::Renderer::Mesh* quadMesh,
                  const HBE::Renderer::TileMap& map);

        void shutdown();

        // --- one-shot spawns ------------------------------------------------
        //
        // (x, y) is the WORLD position at which the effect appears.
        // `dir` is the player's facing (+1 right, -1 left).
        // `tileId` is the 1-based tile ID whose top colour we want to borrow
        // (0 = "not on any tile, use the fallback tan").

        void spawnMuzzleFlash(float x, float y, int dir);
        void spawnCasing     (float x, float y, int dir);
        void spawnLandingDust(float feetX, float feetY, int tileId);
        void spawnBulletImpact(float x, float y, int tileId);

        // --- continuous walk dust ------------------------------------------
        //
        // Call every frame. Fires a small burst at a fixed cadence while the
        // player is grounded AND horizontally moving. Zero-cost when idle.
        void tickWalkDust(float dt,
                          float feetX, float feetY,
                          int tileId,
                          bool moving, bool grounded);

        // --- lifecycle ------------------------------------------------------
        void update(float dt);
        void render(HBE::Renderer::Renderer2D& r2d) const;

        int liveParticles() const;

        // --- tunables -------------------------------------------------------
        float walkDustPeriod = 0.18f;   // seconds between walk-dust puffs

    private:
        // Look up sampled tile top colour, or fall back to a neutral tan.
        void colourForTile(int tileId, float& r, float& g, float& b) const;

        std::unique_ptr<HBE::Renderer::ParticleSystem> m_ps;
        std::unordered_map<int, std::array<float, 4>> m_tileTop;
        float m_walkAccum = 0.0f;
        bool  m_initialized = false;
    };
}
```

The ctor/dtor **must** be out-of-line in `Effects.cpp` — `std::unique_ptr`
requires the pointee's full type at destruction time, and the pimpl trick
above relies on that only happening inside the .cpp that includes
`ParticleSystem.h`.

Why the helper class exists at all:

- Keeps `GameLayer` free of particle boilerplate.
- Owns the tile-top colour cache — nothing else needs it.
- Gives us **one** place to retune every effect later.

---

## 2. `src/Game/Effects.cpp` (new)

```cpp
#include "Game/Effects.h"

// ParticleSystem.h transitively drags in Scene2D.h -> SpriteAnimationStateMachine.h
// -> SpriteRenderer2D.h. That's fine INSIDE this TU because we don't include
// Player.h / Sprite2D.h here, so the duelling SpriteRenderer2D classes never
// meet. Do NOT move these includes into Effects.h.
#include "HBE/Renderer/ParticleSystem.h"
#include "HBE/Renderer/EmitterConfig.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/TileMap.h"
#include "HBE/Renderer/TileMapLoader.h"
#include "HBE/Core/Log.h"

using namespace HBE::Renderer;

namespace MegaX {

    // Out-of-line so std::unique_ptr<ParticleSystem> is instantiated in a TU
    // that has ParticleSystem's full type available.
    Effects::Effects()  = default;
    Effects::~Effects() = default;

    // ─────────────────────────────────────────────────────────────────────
    // Effect factories.
    //
    // Each returns an EffectDef (a vector of EmitterConfig layers). Colour
    // fields for the tint-from-world effects are neutral placeholders --
    // the spawn helpers overwrite start/end R/G/B just before spawn.
    // ─────────────────────────────────────────────────────────────────────

    static EffectDef makeWalkDust() {
        EmitterConfig c;
        c.name          = "walk_dust";
        c.emissionRate  = 0.0f;
        c.duration      = 0.05f;
        c.worldSpace    = true;
        c.maxParticles  = 24;
        c.bursts.push_back({ 0.0f, 4, 1 });    // 4 particles, once

        c.lifetimeMin   = 0.20f; c.lifetimeMax = 0.40f;
        c.shape         = EmitterConfig::Shape::Line;
        c.shapeWidth    = 10.0f;                // narrow band under the feet

        c.speedMin      = 20.0f; c.speedMax    = 55.0f;
        c.dirMin        = 60.0f; c.dirMax      = 120.0f;  // fan upward
        c.gravityY      = -180.0f;                        // fall back down
        c.drag          = 3.5f;

        c.startSizeMin  = 3.0f;  c.startSizeMax = 5.0f;
        c.endSizeMin    = 0.0f;  c.endSizeMax   = 0.0f;

        // tan fallback; overwritten per spawn from tile colour
        c.startR = 0.82f; c.startG = 0.72f; c.startB = 0.55f; c.startA = 0.75f;
        c.endR   = 0.75f; c.endG   = 0.65f; c.endB   = 0.50f; c.endA   = 0.0f;

        c.sortLayer     = 95;   // just behind the player (player is 100)
        return { c };
    }

    static EffectDef makeLandDust() {
        EmitterConfig c;
        c.name          = "land_dust";
        c.emissionRate  = 0.0f;
        c.duration      = 0.05f;
        c.worldSpace    = true;
        c.maxParticles  = 32;
        c.bursts.push_back({ 0.0f, 12, 1 });

        c.lifetimeMin   = 0.30f; c.lifetimeMax = 0.60f;
        c.shape         = EmitterConfig::Shape::Line;
        c.shapeWidth    = 22.0f;                // wider than walk

        c.speedMin      = 50.0f; c.speedMax    = 140.0f;
        c.dirMin        = 30.0f; c.dirMax      = 150.0f;  // wider fan
        c.gravityY      = -260.0f;
        c.drag          = 3.0f;

        c.startSizeMin  = 5.0f;  c.startSizeMax = 8.0f;
        c.endSizeMin    = 0.0f;  c.endSizeMax   = 0.0f;

        c.startR = 0.82f; c.startG = 0.72f; c.startB = 0.55f; c.startA = 0.80f;
        c.endR   = 0.72f; c.endG   = 0.62f; c.endB   = 0.48f; c.endA   = 0.0f;

        c.sortLayer     = 95;
        return { c };
    }

    static EffectDef makeMuzzleFlash() {
        // Two-layer flash: a bright additive core + a small yellow ring.
        EmitterConfig core;
        core.name          = "muzzle_flash_core";
        core.emissionRate  = 0.0f;
        core.duration      = 0.04f;
        core.worldSpace    = true;
        core.maxParticles  = 8;
        core.additiveBlend = true;
        core.bursts.push_back({ 0.0f, 4, 1 });

        core.lifetimeMin   = 0.05f; core.lifetimeMax = 0.09f;
        core.shape         = EmitterConfig::Shape::Point;
        core.speedMin      = 0.0f;  core.speedMax   = 0.0f;
        core.startSizeMin  = 10.0f; core.startSizeMax = 14.0f;
        core.endSizeMin    = 2.0f;  core.endSizeMax   = 4.0f;

        core.startR = 1.0f; core.startG = 1.0f;  core.startB = 0.85f; core.startA = 1.0f;
        core.endR   = 1.0f; core.endG   = 0.55f; core.endB   = 0.0f;  core.endA   = 0.0f;

        core.sortLayer = 102;    // in front of bullets (101)
        // dirMin/dirMax are unused because speed = 0

        EmitterConfig sparks;
        sparks.name          = "muzzle_flash_sparks";
        sparks.emissionRate  = 0.0f;
        sparks.duration      = 0.05f;
        sparks.worldSpace    = true;
        sparks.maxParticles  = 12;
        sparks.additiveBlend = true;
        sparks.bursts.push_back({ 0.0f, 6, 1 });

        sparks.lifetimeMin   = 0.06f; sparks.lifetimeMax = 0.14f;
        sparks.shape         = EmitterConfig::Shape::Point;
        sparks.speedMin      = 160.0f; sparks.speedMax   = 300.0f;

        // Fires forward in a narrow cone; spawn helper rotates by facing.
        sparks.dirMin        = -18.0f; sparks.dirMax    = 18.0f;

        sparks.gravityY      = 0.0f;
        sparks.drag          = 4.0f;

        sparks.startSizeMin  = 3.0f;  sparks.startSizeMax = 5.0f;
        sparks.endSizeMin    = 0.0f;  sparks.endSizeMax   = 0.0f;

        sparks.startR = 1.0f; sparks.startG = 0.95f; sparks.startB = 0.4f; sparks.startA = 1.0f;
        sparks.endR   = 1.0f; sparks.endG   = 0.35f; sparks.endB   = 0.0f; sparks.endA   = 0.0f;

        sparks.sortLayer = 102;

        return { core, sparks };
    }

    static EffectDef makeCasing() {
        // A single brass casing ejected up + back from the gun. Not tinted;
        // the spawn helper only sets position and flips the horizontal speed
        // by facing (by re-registering the config each shot).
        EmitterConfig c;
        c.name          = "bullet_casing";
        c.emissionRate  = 0.0f;
        c.duration      = 0.05f;
        c.worldSpace    = true;
        c.maxParticles  = 4;
        c.bursts.push_back({ 0.0f, 1, 1 });

        c.lifetimeMin   = 0.55f; c.lifetimeMax = 0.75f;
        c.shape         = EmitterConfig::Shape::Point;
        c.speedMin      = 90.0f; c.speedMax   = 130.0f;
        c.dirMin        = 105.0f; c.dirMax    = 130.0f;  // up + slightly back-left

        c.gravityY      = -900.0f;                        // heavy: falls fast
        c.drag          = 0.2f;

        c.rotVelMin     = -12.0f; c.rotVelMax = 12.0f;    // tumble

        c.startSizeMin  = 3.0f;  c.startSizeMax = 4.0f;
        c.endSizeMin    = 3.0f;  c.endSizeMax   = 4.0f;   // constant size

        // brass
        c.startR = 0.95f; c.startG = 0.78f; c.startB = 0.25f; c.startA = 1.0f;
        c.endR   = 0.75f; c.endG   = 0.60f; c.endB   = 0.15f; c.endA   = 1.0f;

        c.sortLayer     = 102;
        return { c };
    }

    static EffectDef makeBulletImpact() {
        // Two-layer burst:
        //   chunks   -> chunks of the tile the bullet hit (colour tinted per spawn)
        //   sparks   -> additive yellow "metal" from the bullet exploding
        EmitterConfig chunks;
        chunks.name          = "bullet_impact_chunks";
        chunks.emissionRate  = 0.0f;
        chunks.duration      = 0.05f;
        chunks.worldSpace    = true;
        chunks.maxParticles  = 20;
        chunks.bursts.push_back({ 0.0f, 10, 1 });

        chunks.lifetimeMin   = 0.20f; chunks.lifetimeMax = 0.40f;
        chunks.shape         = EmitterConfig::Shape::Point;
        chunks.speedMin      = 120.0f; chunks.speedMax   = 260.0f;
        chunks.dirMin        = 0.0f;   chunks.dirMax     = 360.0f;

        chunks.gravityY      = -420.0f;
        chunks.drag          = 1.4f;

        chunks.startSizeMin  = 3.0f;  chunks.startSizeMax = 5.0f;
        chunks.endSizeMin    = 0.0f;  chunks.endSizeMax   = 0.0f;

        // tinted per-spawn from tile colour
        chunks.startR = 0.75f; chunks.startG = 0.70f; chunks.startB = 0.60f; chunks.startA = 1.0f;
        chunks.endR   = 0.55f; chunks.endG   = 0.50f; chunks.endB   = 0.40f; chunks.endA   = 0.0f;

        chunks.sortLayer = 102;

        EmitterConfig sparks;
        sparks.name          = "bullet_impact_sparks";
        sparks.emissionRate  = 0.0f;
        sparks.duration      = 0.05f;
        sparks.worldSpace    = true;
        sparks.maxParticles  = 20;
        sparks.additiveBlend = true;
        sparks.bursts.push_back({ 0.0f, 12, 1 });

        sparks.lifetimeMin   = 0.10f; sparks.lifetimeMax = 0.22f;
        sparks.shape         = EmitterConfig::Shape::Point;
        sparks.speedMin      = 180.0f; sparks.speedMax   = 340.0f;
        sparks.dirMin        = 0.0f;   sparks.dirMax     = 360.0f;

        sparks.gravityY      = -220.0f;
        sparks.drag          = 1.2f;

        sparks.startSizeMin  = 3.0f;  sparks.startSizeMax = 5.0f;
        sparks.endSizeMin    = 0.0f;  sparks.endSizeMax   = 0.0f;

        // bright yellow "metal" flash
        sparks.startR = 1.0f;  sparks.startG = 0.95f; sparks.startB = 0.35f; sparks.startA = 1.0f;
        sparks.endR   = 1.0f;  sparks.endG   = 0.40f; sparks.endB   = 0.0f;  sparks.endA   = 0.0f;

        sparks.sortLayer = 102;

        return { chunks, sparks };
    }

    // ─────────────────────────────────────────────────────────────────────
    // Effects wrapper.
    // ─────────────────────────────────────────────────────────────────────

    bool Effects::init(ResourceCache& resources, Mesh* quadMesh, const TileMap& map) {
        m_ps = std::make_unique<ParticleSystem>();
        if (!m_ps->initialize(resources, quadMesh)) {
            HBE::Core::LogError("MegaX Effects: ParticleSystem init failed.");
            m_ps.reset();
            return false;
        }

        m_ps->registerEffect("walk_dust",     makeWalkDust());
        m_ps->registerEffect("land_dust",     makeLandDust());
        m_ps->registerEffect("muzzle_flash",  makeMuzzleFlash());
        m_ps->registerEffect("bullet_casing", makeCasing());
        m_ps->registerEffect("bullet_impact", makeBulletImpact());

        // One-time sample: average top-of-tile colour for tileset 0.
        // We continue on failure; the fallback tan is used everywhere.
        if (!TileMapLoader::sampleTileTopColors(map, m_tileTop, /*topRows*/ 3)) {
            HBE::Core::LogInfo("MegaX Effects: tile top colours unavailable "
                "(tile-tinted effects will use the fallback tan).");
        }

        m_initialized = true;
        return true;
    }

    void Effects::shutdown() {
        if (m_ps) m_ps->shutdown();
        m_ps.reset();
        m_tileTop.clear();
        m_initialized = false;
    }

    void Effects::colourForTile(int tileId, float& r, float& g, float& b) const {
        // Neutral tan fallback (matches the base configs).
        r = 0.82f; g = 0.72f; b = 0.55f;
        if (tileId <= 0) return;
        auto it = m_tileTop.find(tileId);
        if (it != m_tileTop.end()) {
            r = it->second[0]; g = it->second[1]; b = it->second[2];
        }
    }

    void Effects::spawnMuzzleFlash(float x, float y, int dir) {
        // Rotate the sparks cone by facing. We simply re-register with a
        // rotated direction range (facing +1 = right, -1 = left).
        EffectDef def = makeMuzzleFlash();
        if (def.size() >= 2) {
            EmitterConfig& sparks = def[1];
            if (dir < 0) {
                // reflect the cone across the vertical axis: 180 - d
                const float lo = 180.0f - sparks.dirMax;
                const float hi = 180.0f - sparks.dirMin;
                sparks.dirMin = lo;
                sparks.dirMax = hi;
            }
        }
        m_ps->registerEffect("muzzle_flash", def);
        m_ps->spawn("muzzle_flash", x, y);
    }

    void Effects::spawnCasing(float x, float y, int dir) {
        EffectDef def = makeCasing();
        if (!def.empty() && dir < 0) {
            // Flip the up-and-back cone to the other side.
            EmitterConfig& c = def[0];
            const float lo = 180.0f - c.dirMax;
            const float hi = 180.0f - c.dirMin;
            c.dirMin = lo;
            c.dirMax = hi;
        }
        m_ps->registerEffect("bullet_casing", def);

        // Caller is expected to pass the ejection-port world coord (typically
        // ~5 px in front of the player center at gun height); no fudging here.
        m_ps->spawn("bullet_casing", x, y);
    }

    void Effects::spawnLandingDust(float feetX, float feetY, int tileId) {
        float r, g, b;
        colourForTile(tileId, r, g, b);

        EffectDef def = makeLandDust();
        EmitterConfig& c = def[0];
        c.startR = r;         c.startG = g;         c.startB = b;
        c.endR   = r * 0.55f; c.endG   = g * 0.55f; c.endB   = b * 0.55f;

        m_ps->registerEffect("land_dust", def);
        m_ps->spawn("land_dust", feetX, feetY);
    }

    void Effects::spawnBulletImpact(float x, float y, int tileId) {
        float r, g, b;
        colourForTile(tileId, r, g, b);

        EffectDef def = makeBulletImpact();
        EmitterConfig& chunks = def[0];  // colour-tinted layer
        chunks.startR = r;         chunks.startG = g;         chunks.startB = b;
        chunks.endR   = r * 0.55f; chunks.endG   = g * 0.55f; chunks.endB   = b * 0.55f;
        // sparks layer keeps its yellow additive palette

        m_ps->registerEffect("bullet_impact", def);
        m_ps->spawn("bullet_impact", x, y);
    }

    void Effects::tickWalkDust(float dt,
                               float feetX, float feetY,
                               int tileId,
                               bool moving, bool grounded)
    {
        if (!moving || !grounded) {
            m_walkAccum = 0.0f;   // reset so the first step puffs immediately
            return;
        }

        m_walkAccum += dt;
        if (m_walkAccum < walkDustPeriod) return;
        m_walkAccum -= walkDustPeriod;

        float r, g, b;
        colourForTile(tileId, r, g, b);

        EffectDef def = makeWalkDust();
        EmitterConfig& c = def[0];
        c.startR = r;         c.startG = g;         c.startB = b;
        c.endR   = r * 0.55f; c.endG   = g * 0.55f; c.endB   = b * 0.55f;

        m_ps->registerEffect("walk_dust", def);
        m_ps->spawn("walk_dust", feetX, feetY);
    }

    void Effects::update(float dt) {
        if (!m_initialized || !m_ps) return;
        m_ps->update(dt);
    }

    void Effects::render(Renderer2D& r2d) const {
        if (!m_initialized || !m_ps) return;
        m_ps->render(r2d);
    }

    int Effects::liveParticles() const {
        return m_ps ? m_ps->totalLiveParticles() : 0;
    }
}
```

### Why we `registerEffect` on every spawn for tinted effects

`ParticleSystem::registerEffect(name, def)` **overwrites** the entry in the
library. That's the officially-supported way to change per-spawn colour (see
`HBE.Sandbox/src/GameLayer.cpp` `spawnPlayerDustAtFeet` — same pattern).
Cheap enough for our rates (a handful of spawns per second worst case).

### Why muzzle sparks / casing get "flipped" for `dir == -1`

The base config's cone is authored for `+x` facing. `EmitterConfig` doesn't
have a global rotation, so we reflect the direction range across the
vertical axis (`d' = 180 − d`) when the player faces left. This keeps the
spark cone pointing *forward* and the casing ejecting *up + back* in both
facings, without touching engine code.

> Engine note (for the future engine pass):
> 1. A `dirOffset` / `dirMirror` field on `EmitterConfig` would remove
>    this book-keeping.
> 2. There are two headers each defining `class HBE::Renderer::SpriteRenderer2D`
>    (`Sprite2D.h` and `SpriteRenderer2D.h`). Consolidate them so game
>    code can freely include both `Player.h` and `ParticleSystem.h` in
>    the same TU without the pimpl workaround.
> 3. `TileMap` is a `struct` but `Scene2D.h` forward-declares it as
>    `class`, triggering C4099 anywhere both headers meet. Change the
>    forward decl to `struct TileMap;`.

---

Next: `02_player_and_bullet_hooks.md` — the small getters and impact queue
that `Effects` needs from `Player` and `BulletManager`.
