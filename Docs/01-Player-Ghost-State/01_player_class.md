# Doc 01 — The `Player` class (sprite + animation + movement)

Create two files. `Player` owns everything about the ghost character: both
sprite sheets, the idle/walk animations, its position/velocity/facing, and a
single `RenderItem` it draws each frame. `GameLayer` (doc 02) just feeds it
input and asks it to update/render.

---

## Key engine APIs used here

- `HBE::Renderer::SpriteSheet` — `{ texture, texWidth, texHeight, frameWidth,
  frameHeight, margins… }`. Describes a **uniform grid**.
- `SpriteRenderer2D::DeclareSpriteSheet(cache, name, path, frameW, frameH)` —
  loads the PNG via the `ResourceCache` and fills a `SpriteSheet`. Returns an
  invalid sheet (`texture == nullptr`) on failure.
- `SpriteAnimation(sheet*, colStart, colEnd, row, fps, loop)` — plays one **row**
  of frames. `play()/stop()/update(dt)`; `apply(RenderItem&)` writes the current
  frame's `uvRect`.
- `RenderItem` — `{ mesh, material, transform, layer, pass, tint, uvRect }`.
- `Material` — `{ shader, texture, color, useSDF… }`.
- `Transform2D` — `{ posX, posY, rotation, scaleX, scaleY }`. **Negative
  `scaleX` mirrors horizontally** (that's our facing flip).

> **Why one `m_activeSheet` member?** `SpriteAnimation` stores a *pointer* to a
> `SpriteSheet`. We point both animations at a stable member, `m_activeSheet`,
> and swap helmets by **overwriting its contents** (same 75×48 grid, different
> texture) and re-pointing the material's texture. The animation pointers stay
> valid and keep their current frame — no rebuild needed.

---

## File 1 — `include\Game\Player.h`

```cpp
#pragma once

#include "HBE/Renderer/Sprite2D.h"
#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Material.h"

namespace HBE::Renderer {
    class ResourceCache;
    class Mesh;
    class GLShader;
    class Renderer2D;
}

namespace MegaX {

    // The free-flying "ghost" player: no physics, no collision. Just a sprite
    // that moves where input tells it and animates idle vs walk.
    class Player {
    public:
        // Loads both sheets and builds the render item + animations.
        // quadMesh + spriteShader come from GameLayer's sprite pipeline.
        // Returns false if a sheet failed to load.
        bool init(HBE::Renderer::ResourceCache& resources,
                  HBE::Renderer::Mesh* quadMesh,
                  HBE::Renderer::GLShader* spriteShader);

        void  setPosition(float x, float y) { m_x = x; m_y = y; }
        float x()    const { return m_x; }
        float y()    const { return m_y; }
        float velX() const { return m_vx; }
        float velY() const { return m_vy; }

        // Per-frame move intent; each component in [-1, +1].
        void setMoveInput(float ix, float iy) { m_inX = ix; m_inY = iy; }

        // Helmet on  = player.png ; off = player-no-helm.png.
        void setHelmet(bool on);
        void toggleHelmet() { setHelmet(!m_helmet); }
        bool hasHelmet() const { return m_helmet; }

        void update(float dt);
        void render(HBE::Renderer::Renderer2D& r2d);

        // Ghost fly speed, world units (= pixels) per second.
        float moveSpeed = 180.0f;

    private:
        float m_x = 0.0f, m_y = 0.0f;   // world position
        float m_vx = 0.0f, m_vy = 0.0f; // world velocity (for camera look-ahead)
        float m_inX = 0.0f, m_inY = 0.0f;
        int   m_facing = 1;             // +1 right, -1 left
        bool  m_helmet = true;
        bool  m_moving = false;

        HBE::Renderer::SpriteSheet m_helmSheet{};
        HBE::Renderer::SpriteSheet m_noHelmSheet{};
        HBE::Renderer::SpriteSheet m_activeSheet{}; // what the animations point at

        HBE::Renderer::SpriteAnimation m_idleAnim;
        HBE::Renderer::SpriteAnimation m_walkAnim;

        HBE::Renderer::Material   m_material{};
        HBE::Renderer::RenderItem m_item{};
    };

} // namespace MegaX
```

---

## File 2 — `src\Game\Player.cpp`

```cpp
#include "Game/Player.h"

#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Core/AssetPaths.h"
#include "HBE/Core/Log.h"

#include <cmath>

using namespace HBE::Renderer;

namespace MegaX {

    // ---- Sprite-sheet grid (player.png / player-no-helm.png, both 750x672) ----
    // 10 columns x 14 rows  =>  each cell is 75 x 48 px.
    static constexpr int   kFrameW = 75;
    static constexpr int   kFrameH = 48;

    // Idle = row 3, cols 0..3 (4 frames).  Walk = row 6, cols 0..9 (10 frames).
    static constexpr int   kIdleRow = 3, kIdleCol0 = 0, kIdleCol1 = 3;
    static constexpr int   kWalkRow = 6, kWalkCol0 = 0, kWalkCol1 = 9;
    static constexpr float kIdleFps = 8.0f;
    static constexpr float kWalkFps = 14.0f;

    // Draw 1 world unit per source pixel.
    static constexpr float kPixelScale = 1.0f;

    bool Player::init(ResourceCache& resources, Mesh* quadMesh, GLShader* spriteShader) {
        m_helmSheet = SpriteRenderer2D::DeclareSpriteSheet(
            resources, "player_helm",
            HBE::Core::AssetPaths::Resolve("sprites/Player/player.png"),
            kFrameW, kFrameH);

        m_noHelmSheet = SpriteRenderer2D::DeclareSpriteSheet(
            resources, "player_no_helm",
            HBE::Core::AssetPaths::Resolve("sprites/Player/player-no-helm.png"),
            kFrameW, kFrameH);

        if (!m_helmSheet.isValid() || !m_noHelmSheet.isValid()) {
            HBE::Core::LogError("Player::init: failed to load player sprite sheet(s). "
                "Asset root: " + HBE::Core::AssetPaths::AssetRootString());
            return false;
        }

        // The animations point at m_activeSheet; swapping helmets overwrites it.
        m_activeSheet = m_helmet ? m_helmSheet : m_noHelmSheet;

        m_idleAnim = SpriteAnimation(&m_activeSheet, kIdleCol0, kIdleCol1, kIdleRow, kIdleFps, true);
        m_walkAnim = SpriteAnimation(&m_activeSheet, kWalkCol0, kWalkCol1, kWalkRow, kWalkFps, true);
        m_idleAnim.play(true);

        m_material.shader  = spriteShader;
        m_material.texture = m_activeSheet.texture;

        m_item.mesh     = quadMesh;
        m_item.material = &m_material;
        m_item.layer    = 100;
        m_item.pass     = RenderPass::World;
        m_item.transform.scaleX = kFrameW * kPixelScale;
        m_item.transform.scaleY = kFrameH * kPixelScale;

        // Seed a valid first frame so we draw something on frame 0.
        SpriteRenderer2D::SetStaticSpriteFrame(m_item, m_activeSheet, kIdleCol0, kIdleRow);
        return true;
    }

    void Player::setHelmet(bool on) {
        m_helmet = on;
        m_activeSheet      = on ? m_helmSheet : m_noHelmSheet; // same grid, new texture
        m_material.texture = m_activeSheet.texture;
    }

    void Player::update(float dt) {
        // Normalize so diagonal flight isn't faster than cardinal.
        float ix = m_inX, iy = m_inY;
        const float mag = std::sqrt(ix * ix + iy * iy);
        if (mag > 1.0f) { ix /= mag; iy /= mag; }

        m_vx = ix * moveSpeed;
        m_vy = iy * moveSpeed;
        m_x += m_vx * dt;
        m_y += m_vy * dt;

        // Facing: flip on horizontal input, keep last when neutral.
        if (m_inX > 0.0f)      m_facing = +1;
        else if (m_inX < 0.0f) m_facing = -1;

        // Idle vs walk: any movement plays the walk cycle.
        const bool moving = (m_inX != 0.0f || m_inY != 0.0f);
        if (moving != m_moving) {
            m_moving = moving;
            if (moving) m_walkAnim.play(true);
            else        m_idleAnim.play(true);
        }

        SpriteAnimation& anim = moving ? m_walkAnim : m_idleAnim;
        anim.update(dt);
        anim.apply(m_item); // writes uvRect for the current frame

        m_item.transform.posX   = m_x;
        m_item.transform.posY   = m_y;
        m_item.transform.scaleX = kFrameW * kPixelScale * static_cast<float>(m_facing);
        m_item.transform.scaleY = kFrameH * kPixelScale;
    }

    void Player::render(Renderer2D& r2d) {
        r2d.draw(m_item);
    }

} // namespace MegaX
```

### Why these choices

- **`DeclareSpriteSheet` names (`"player_helm"`, `"player_no_helm"`)** are
  `ResourceCache` keys — they **must be unique**. The cache loads each texture
  once and returns it by *name*, ignoring the path on a cache hit. If you give
  both sheets the same name, the second call returns the first texture, so
  `m_noHelmSheet.texture == m_helmSheet.texture` and the **H helmet toggle
  silently does nothing**. Double-check these two strings differ.
- **Idle/Walk fps (8 / 14)** are starting values; tweak for feel.
- **`moveSpeed = 180`** ≈ 5.6 tiles/sec at 32 px — brisk ghost flight. Public,
  so you can change it live from `GameLayer` later.
- **`m_item.layer = 100`** just gives the player a definite draw order; nothing
  else is drawn yet, so any value works.
- **Facing via `scaleX` sign** matches the engine/Sandbox convention
  (`facing = scaleX < 0 ? -1 : +1`), so future systems read facing the same way.

> The character content sits inside a larger transparent cell, so it may appear
> a hair off dead-center — that's normal for a trimmed atlas on a uniform grid
> (the Sandbox soldier behaves identically). It's fine for the ghost state.

---

## Verify (doc 01)

- [ ] `include\Game\Player.h` and `src\Game\Player.cpp` exist at those paths.
- [ ] No `#include` of anything under `HBE.Sandbox`.
- [ ] The only engine headers referenced are under `HBE/Renderer/…` and
      `HBE/Core/…` (public engine includes).
- [ ] It won't *link* until doc 03 adds the files to the project — that's
      expected. Continue to doc 02 first.

Next: **`02_gamelayer_wiring.md`**.
