# Item 04 · Doc 01 — The Player Physics Code

Two files change here: `include/Game/Player.h` (new API + state) and
`src/Game/Player.cpp` (the physics + animation state machine). Replace each file
with the version below.

---

## 1. `include/Game/Player.h`

```cpp
#pragma once

#include "HBE/Renderer/Sprite2D.h"
#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/TileCollision.h"   // AABB, TileMap, TileMapLayer, MoveResult2D

namespace HBE::Renderer {
    class ResourceCache;
    class Mesh;
    class GLShader;
    class Renderer2D;
}

namespace MegaX {

    class Player {
    public:
        enum class Mode { Play, Ghost };

        bool init(HBE::Renderer::ResourceCache& resources,
            HBE::Renderer::Mesh* quadMesh,
            HBE::Renderer::GLShader* spriteShader);

        // Collision world (owned by World). `solidLayer` is the layer whose
        // solid tiles the player collides with (the "Ground" layer).
        void setCollision(const HBE::Renderer::TileMap* map,
                          const HBE::Renderer::TileMapLayer* solidLayer) {
            m_map = map;
            m_collLayer = solidLayer;
        }

        void setPosition(float x, float y);   // x,y = sprite center, world space
        float x() const { return m_x; }
        float y() const { return m_y; }
        float velX() const { return m_vx; }
        float velY() const { return m_vy; }

        // --- per-frame input intents (set by GameLayer) ---
        void setMoveInput(float ix, float iy) { m_inX = ix; m_inY = iy; }
        void setJumpInput(bool pressed, bool held) {
            if (pressed) m_jumpPressed = true;   // latched until consumed in update()
            m_jumpHeld = held;
        }
        void setCrouchInput(bool held) { m_crouchHeld = held; }

        // --- mode (item 05 flips this on a hot-key) ---
        void setMode(Mode m) { m_mode = m; }
        Mode mode() const { return m_mode; }
        void toggleMode() { m_mode = (m_mode == Mode::Play) ? Mode::Ghost : Mode::Play; }

        void setHelmet(bool on);
        void toggleHelmet() { setHelmet(!m_helmet); }
        bool hasHelmet() const { return m_helmet; }

        void update(float dt);
        void render(HBE::Renderer::Renderer2D& r2d);

        // --- tunables (world px, seconds) ---
        float moveSpeed = 200.0f;   // ghost fly speed AND play-mode run speed
        float gravity   = 2100.0f;  // downward acceleration
        float jumpSpeed = 640.0f;   // initial jump velocity (up)
        float maxFall   = 900.0f;   // terminal velocity

    private:
        void updateGhost(float dt);
        void updatePlay(float dt);
        void syncRenderFromBox();                 // box -> m_x/m_y (feet aligned)
        void setAnimState(int s);
        HBE::Renderer::SpriteAnimation& animForState(int s);
        bool boxOverlapsSolid(const HBE::Renderer::AABB& b) const;

        // position = sprite center (world space)
        float m_x = 0.0f, m_y = 0.0f;
        float m_vx = 0.0f, m_vy = 0.0f;
        float m_inX = 0.0f, m_inY = 0.0f;

        // play-mode physics box (center-based, world space)
        HBE::Renderer::AABB m_box{};
        bool  m_grounded  = false;
        bool  m_crouching = false;
        float m_coyote    = 0.0f;   // time left to still jump after leaving ground
        float m_jumpBuf   = 0.0f;   // time left for a buffered jump press
        float m_landTimer = 0.0f;   // time left to show the landing animation

        // latched input intents
        bool m_jumpPressed = false;
        bool m_jumpHeld    = false;
        bool m_crouchHeld  = false;

        int  m_facing = 1;
        bool m_helmet = true;
        Mode m_mode = Mode::Play;   // item 04 tests Play; item 05 adds the toggle

        // collision world (not owned)
        const HBE::Renderer::TileMap*      m_map = nullptr;
        const HBE::Renderer::TileMapLayer* m_collLayer = nullptr;

        // animation
        int m_animState = -1;   // 0 idle,1 walk,2 crouch,3 rise,4 fall,5 land
        HBE::Renderer::SpriteSheet m_helmSheet{};
        HBE::Renderer::SpriteSheet m_noHelmSheet{};
        HBE::Renderer::SpriteSheet m_activeSheet{};

        HBE::Renderer::SpriteAnimation m_idleAnim;
        HBE::Renderer::SpriteAnimation m_walkAnim;
        HBE::Renderer::SpriteAnimation m_crouchAnim;
        HBE::Renderer::SpriteAnimation m_riseAnim;
        HBE::Renderer::SpriteAnimation m_fallAnim;
        HBE::Renderer::SpriteAnimation m_landAnim;

        HBE::Renderer::Material   m_material{};
        HBE::Renderer::RenderItem m_item{};
    };
}
```

### What changed vs item 01
- New `Mode` enum + `m_mode` (defaults to `Play`).
- `setCollision()` + `m_map` / `m_collLayer` pointers.
- New input intents: `setJumpInput`, `setCrouchInput` (plus the existing
  `setMoveInput`).
- Physics tunables + the `AABB m_box`, grounded/crouch/coyote/jump‑buffer/land state.
- Four new animations: `m_crouchAnim`, `m_riseAnim`, `m_fallAnim`, `m_landAnim`.

---

## 2. `src/Game/Player.cpp`

```cpp
#include "Game/Player.h"

#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Core/AssetPaths.h"
#include "HBE/Core/Log.h"

#include <algorithm>
#include <cmath>

using namespace HBE::Renderer;

namespace MegaX {

    // ---- sprite sheet grid (10 cols x 14 rows, 75x48 px cells) --------------
    static constexpr int kFrameW = 75;
    static constexpr int kFrameH = 48;
    static constexpr float kPixelScale = 1.0f;

    static constexpr int kIdleRow = 3, kIdleCol0 = 0, kIdleCol1 = 3;
    static constexpr int kWalkRow = 6, kWalkCol0 = 0, kWalkCol1 = 9;
    static constexpr int kCrouchRow = 1, kCrouchCol0 = 0, kCrouchCol1 = 5;   // prone crawl (6 frames)
    static constexpr int kJumpRow = 13;                     // last row (6 frames)
    static constexpr int kRiseCol0 = 0, kRiseCol1 = 2;      // ascent -> apex
    static constexpr int kFallCol0 = 3, kFallCol1 = 3;      // fall (single frame)
    static constexpr int kLandCol0 = 4, kLandCol1 = 5;      // landing (2 frames)

    static constexpr float kIdleFps   = 8.0f;
    static constexpr float kWalkFps   = 14.0f;
    static constexpr float kCrouchFps = 10.0f;
    static constexpr float kJumpFps   = 12.0f;

    // ---- physics tunables --------------------------------------------------
    static constexpr float kBoxW       = 24.0f;
    static constexpr float kBoxStandH  = 40.0f;
    static constexpr float kBoxCrouchH = 24.0f;

    static constexpr float kCrouchSpeedMul = 0.5f;   // half speed while crouched
    static constexpr float kLandTime = 0.16f;        // landing anim hold (~2 frames @12fps)

    static constexpr float kCoyote  = 0.08f;   // seconds
    static constexpr float kJumpBuf = 0.10f;   // seconds
    static constexpr float kJumpCut = 180.0f;  // velY clamp when jump released early

    // Feet stay at the box bottom; the sprite's frame-bottom is placed there.
    static float feetToCenterOffset(float boxH) {
        return kFrameH * kPixelScale * 0.5f - boxH * 0.5f;   // add to box.cy -> posY
    }

    // ------------------------------------------------------------------ init

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

        m_activeSheet = m_helmet ? m_helmSheet : m_noHelmSheet;

        // All animations reference &m_activeSheet, whose *contents* are swapped
        // by setHelmet() — so a helmet toggle re-skins every animation at once.
        m_idleAnim   = SpriteAnimation(&m_activeSheet, kIdleCol0,   kIdleCol1,   kIdleRow,   kIdleFps,   true);
        m_walkAnim   = SpriteAnimation(&m_activeSheet, kWalkCol0,   kWalkCol1,   kWalkRow,   kWalkFps,   true);
        m_crouchAnim = SpriteAnimation(&m_activeSheet, kCrouchCol0, kCrouchCol1, kCrouchRow, kCrouchFps, true);
        m_riseAnim   = SpriteAnimation(&m_activeSheet, kRiseCol0,   kRiseCol1,   kJumpRow,   kJumpFps,   false);
        m_fallAnim   = SpriteAnimation(&m_activeSheet, kFallCol0,   kFallCol1,   kJumpRow,   kJumpFps,   false);
        m_landAnim   = SpriteAnimation(&m_activeSheet, kLandCol0,   kLandCol1,   kJumpRow,   kJumpFps,   false);

        m_material.shader  = spriteShader;
        m_material.texture = m_activeSheet.texture;

        m_item.mesh = quadMesh;
        m_item.material = &m_material;
        m_item.layer = 100;
        m_item.pass = RenderPass::World;
        m_item.transform.scaleX = kFrameW * kPixelScale;
        m_item.transform.scaleY = kFrameH * kPixelScale;

        m_box.w = kBoxW;
        m_box.h = kBoxStandH;

        m_animState = 0;
        m_idleAnim.play(true);
        SpriteRenderer2D::SetStaticSpriteFrame(m_item, m_activeSheet, kIdleCol0, kIdleRow);
        return true;
    }

    void Player::setHelmet(bool on) {
        m_helmet = on;
        m_activeSheet = on ? m_helmSheet : m_noHelmSheet;
        m_material.texture = m_activeSheet.texture;
    }

    void Player::setPosition(float x, float y) {
        m_x = x; m_y = y;
        m_vx = m_vy = 0.0f;
        m_grounded = false;
        m_crouching = false;
        m_coyote = m_jumpBuf = 0.0f;
        m_landTimer = 0.0f;

        m_box.w = kBoxW;
        m_box.h = kBoxStandH;
        m_box.cx = x;
        m_box.cy = y - feetToCenterOffset(m_box.h);   // inverse of render sync
    }

    // ------------------------------------------------------------------ helpers

    bool Player::boxOverlapsSolid(const AABB& b) const {
        if (!m_map || !m_collLayer) return false;

        const float tw = m_map->worldTileW();
        const float th = m_map->worldTileH();
        const float minX = b.cx - b.w * 0.5f, maxX = b.cx + b.w * 0.5f;
        const float minY = b.cy - b.h * 0.5f, maxY = b.cy + b.h * 0.5f;

        const int minTX = (int)std::floor(minX / tw);
        const int maxTX = (int)std::floor((maxX - 0.001f) / tw);
        const int minTY = (int)std::floor(minY / th);
        const int maxTY = (int)std::floor((maxY - 0.001f) / th);

        const auto& ts = m_map->tilesets[m_collLayer->tilesetIndex];
        for (int ty = minTY; ty <= maxTY; ++ty)
            for (int tx = minTX; tx <= maxTX; ++tx) {
                const int id = m_collLayer->at(tx, ty);
                if (id != 0 && ts.isSolid(id)) return true;
            }
        return false;
    }

    void Player::syncRenderFromBox() {
        m_x = m_box.cx;
        m_y = m_box.cy + feetToCenterOffset(m_box.h);
    }

    SpriteAnimation& Player::animForState(int s) {
        switch (s) {
            case 1: return m_walkAnim;
            case 2: return m_crouchAnim;
            case 3: return m_riseAnim;
            case 4: return m_fallAnim;
            case 5: return m_landAnim;
            default: return m_idleAnim;
        }
    }

    void Player::setAnimState(int s) {
        if (s == m_animState) return;
        m_animState = s;
        animForState(s).play(true);   // rewind on entering a state
    }

    // ------------------------------------------------------------------ update

    void Player::update(float dt) {
        if (m_mode == Mode::Ghost) updateGhost(dt);
        else                       updatePlay(dt);

        m_jumpPressed = false;   // consume the latched one-shot press

        // shared: write transform + advance the current animation
        m_item.transform.posX = m_x;
        m_item.transform.posY = m_y;
        m_item.transform.scaleX = kFrameW * kPixelScale * static_cast<float>(m_facing);
        m_item.transform.scaleY = kFrameH * kPixelScale;

        SpriteAnimation& a = animForState(m_animState < 0 ? 0 : m_animState);
        a.update(dt);
        a.apply(m_item);
    }

    void Player::updateGhost(float dt) {
        float ix = m_inX, iy = m_inY;
        const float mag = std::sqrt(ix * ix + iy * iy);
        if (mag > 1.0f) { ix /= mag; iy /= mag; }

        m_vx = ix * moveSpeed;
        m_vy = iy * moveSpeed;
        m_x += m_vx * dt;
        m_y += m_vy * dt;

        // keep the physics box synced so a switch to Play mode is seamless
        m_box.h = kBoxStandH;
        m_box.cx = m_x;
        m_box.cy = m_y - feetToCenterOffset(m_box.h);

        if (m_inX > 0.0f) m_facing = +1;
        else if (m_inX < 0.0f) m_facing = -1;

        setAnimState((m_inX != 0.0f || m_inY != 0.0f) ? 1 : 0);
    }

    void Player::updatePlay(float dt) {
        // --- timers (coyote uses last frame's grounded state) ---
        m_coyote  = std::max(0.0f, m_coyote - dt);
        m_jumpBuf = m_jumpPressed ? kJumpBuf : std::max(0.0f, m_jumpBuf - dt);
        const bool onGroundPrev = m_grounded;
        if (onGroundPrev) m_coyote = kCoyote;

        // --- crouch intent (ground only) ---
        bool wantCrouch = m_crouchHeld && onGroundPrev;

        // --- horizontal (full air control, snappy; half speed while crouched) ---
        const float speed = wantCrouch ? moveSpeed * kCrouchSpeedMul : moveSpeed;
        float ix = m_inX;
        m_vx = ix * speed;
        if (ix > 0.0f) m_facing = +1;
        else if (ix < 0.0f) m_facing = -1;

        // --- jump (buffered + coyote) ---
        if (m_jumpBuf > 0.0f && m_coyote > 0.0f && !wantCrouch) {
            m_vy = jumpSpeed;
            m_grounded = false;
            m_coyote = 0.0f;
            m_jumpBuf = 0.0f;
        }
        // variable height: releasing jump while rising cuts the climb
        if (!m_jumpHeld && m_vy > kJumpCut) m_vy = kJumpCut;

        // --- gravity ---
        m_vy -= gravity * dt;
        if (m_vy < -maxFall) m_vy = -maxFall;

        // --- crouch / stand box height (keep the box bottom fixed) ---
        const float targetH = wantCrouch ? kBoxCrouchH : kBoxStandH;
        if (targetH != m_box.h) {
            const float bottom = m_box.cy - m_box.h * 0.5f;
            if (targetH > m_box.h) {
                // standing up: only if the taller box is clear of solids
                AABB test = m_box;
                test.h = targetH;
                test.cy = bottom + targetH * 0.5f;
                if (!boxOverlapsSolid(test)) { m_box = test; }
                else { wantCrouch = true; }      // blocked -> stay crouched
            } else {
                m_box.cy = bottom + targetH * 0.5f;
                m_box.h = targetH;
            }
        }
        m_crouching = wantCrouch;

        // --- move + collide (X then Y, engine solver) ---
        const float prevBottom = m_box.cy - m_box.h * 0.5f;
        MoveResult2D res{};
        if (m_map && m_collLayer) {
            res = TileCollision::moveAndCollideEx(
                *m_map, *m_collLayer, m_box, m_vx, m_vy, dt,
                /*maxStepUp*/ 0.0f, /*oneWay*/ true, /*slopes*/ true, prevBottom);
        } else {
            m_box.cx += m_vx * dt;
            m_box.cy += m_vy * dt;
        }
        m_grounded = res.grounded;

        syncRenderFromBox();

        // --- landing timer (start when we touch down after being airborne) ---
        const bool landedThisFrame = (!onGroundPrev && res.grounded);
        m_landTimer = std::max(0.0f, m_landTimer - dt);
        if (landedThisFrame) m_landTimer = kLandTime;

        // --- animation selection ---
        const bool onGround = m_grounded || m_coyote > 0.0f;   // debounce ground jitter
        int st;
        if (!onGround)                              st = (m_vy > 0.0f) ? 3 : 4;  // rise / fall
        else if (m_crouching)                       st = 2;                       // crouch / crawl
        else if (m_landTimer > 0.0f && ix == 0.0f)  st = 5;                       // landing recovery
        else if (ix != 0.0f)                        st = 1;                       // walk
        else                                        st = 0;                       // idle
        setAnimState(st);
    }

    void Player::render(Renderer2D& r2d) {
        r2d.draw(m_item);
    }
}
```

### Notes on correctness
- **`moveAndCollideEx` integrates position itself** (`box += vel*dt`). We only
  set velocities and read the box back — never integrate `m_x/m_y` ourselves in
  Play mode.
- **Grounded debounce.** The solver can momentarily report "not grounded" for a
  single frame while resting exactly on a tile edge; `m_coyote` bridges that so
  the jump/fall animation doesn't flicker and jumps stay reliable.
- **Helmet swap still works** because every `SpriteAnimation` was built with
  `&m_activeSheet`; `setHelmet()` swaps that struct's contents (same grid) and
  re‑points the material texture.
- **`enableSlopes/enableOneWay = true`** are harmless on this map (no slope or
  one‑way tiles yet) and future‑proof the mover.

Next: `02_gamelayer_wiring.md` wires the collision layer and the inputs.
