# Doc 02 — Wire the `Player` into `GameLayer`

`GameLayer` now does five things: build the **sprite pipeline** (shader + quad
mesh), set the clear color to **black**, own a **`CameraController`** and the
**`Player`**, read **input** each frame, and **render** the player through the
camera.

Replace the item-00 `GameLayer.h` / `GameLayer.cpp` with the versions below.

---

## Key engine APIs used here

- `ResourceCache::getOrCreateShaderFromFiles(name, vsPath, fsPath)` (with an
  inline `getOrCreateShader` fallback) and `getOrCreateMeshPosUV(name, verts, n)`.
- `Renderer2D::setSpriteQuadMesh(mesh)`, `beginScene(cam, pass)`, `draw(item)`,
  `endScene()`.
- `GLRenderer::setClearColor(r,g,b,a)` and `setCamera(cam)`.
- `HBE::Renderer::CameraController` — `camera()`, `snapTo(x,y)`,
  `setFollowTarget(x,y)`, `setFollowVelocity(vx,vy)`, `update(dt)`, `snapZoom(z)`,
  plus public fields `followResponse`, `pixelSnap`.
- `HBE::Platform::Input::IsKeyDown / IsKeyPressed(SDL_Scancode)` — raw keyboard.
  The engine already calls `Input::Initialize()` and pumps it every frame, so
  no setup is needed.

---

## File 1 — `include\Game\GameLayer.h`

```cpp
#pragma once

#include "HBE/Core/Layer.h"
#include "HBE/Renderer/CameraController.h"

#include "Game/Player.h"

namespace HBE::Core     { class Application; }
namespace HBE::Renderer { class Mesh; class GLShader; }

namespace MegaX {

    // Root game layer: owns the camera, the ghost player, and the sprite
    // rendering pipeline (shader + quad mesh).
    class GameLayer : public HBE::Core::Layer {
    public:
        void onAttach(HBE::Core::Application& app) override;
        void onUpdate(float dt) override;
        void onRender() override;

    private:
        void buildSpritePipeline();

        HBE::Core::Application* m_app = nullptr;

        HBE::Renderer::Mesh*     m_quadMesh     = nullptr;
        HBE::Renderer::GLShader* m_spriteShader = nullptr;

        HBE::Renderer::CameraController m_camera{};
        Player m_player{};
    };

} // namespace MegaX
```

---

## File 2 — `src\Game\GameLayer.cpp`

```cpp
#include "Game/GameLayer.h"

#include "HBE/Core/Application.h"
#include "HBE/Core/AssetPaths.h"
#include "HBE/Core/Log.h"

#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/RenderPass.h"

#include "HBE/Platform/Input.h"

#include <vector>

using namespace HBE::Core;
using namespace HBE::Renderer;
namespace Input = HBE::Platform::Input;

namespace MegaX {

    // ---- View settings -------------------------------------------------------
    static constexpr float kLogicalWidth  = 1280.0f;
    static constexpr float kLogicalHeight = 720.0f;
    static constexpr float kCameraZoom    = 3.0f; // bigger = closer

    void GameLayer::onAttach(Application& app) {
        m_app = &app;

        buildSpritePipeline();

        // Ghost state = black void.
        app.gl().setClearColor(0.0f, 0.0f, 0.0f, 1.0f);

        // Camera: world units = pixels, zoomed in, smooth follow.
        Camera2D& cam       = m_camera.camera();
        cam.viewportWidth   = kLogicalWidth;
        cam.viewportHeight  = kLogicalHeight;
        // pixelSnap rounds the camera to whole WORLD units. At zoom 3 that's
        // 3-screen-pixel steps -> the camera stair-steps while the player glides
        // (judder/blur). Leave it OFF for a magnified camera; textures are
        // GL_NEAREST so sprites stay crisp anyway.
        m_camera.pixelSnap      = false;
        m_camera.followResponse = 9.0f;   // ~ lerp(0.15) @ 60fps
        m_camera.snapZoom(kCameraZoom);

        // Spawn the player at the origin and center the camera on it.
        if (!m_player.init(app.resources(), m_quadMesh, m_spriteShader)) {
            LogError("MegaX GameLayer: player init failed.");
        }
        m_player.setPosition(0.0f, 0.0f);
        m_camera.snapTo(0.0f, 0.0f);
        app.gl().setCamera(m_camera.camera());

        LogInfo("MegaX GameLayer attached (player ghost state).");
    }

    void GameLayer::onUpdate(float dt) {
        // --- Input: A/D = left/right, SPACE = up (+Y), S = down (-Y) ----------
        const float ix = (Input::IsKeyDown(SDL_SCANCODE_D) ? 1.0f : 0.0f)
                       - (Input::IsKeyDown(SDL_SCANCODE_A) ? 1.0f : 0.0f);
        const float iy = (Input::IsKeyDown(SDL_SCANCODE_SPACE) ? 1.0f : 0.0f)
                       - (Input::IsKeyDown(SDL_SCANCODE_S)     ? 1.0f : 0.0f);

        if (Input::IsKeyPressed(SDL_SCANCODE_H)) {
            m_player.toggleHelmet();
        }

        m_player.setMoveInput(ix, iy);
        m_player.update(dt);

        // --- Camera lerp-follows the player ----------------------------------
        m_camera.setFollowTarget(m_player.x(), m_player.y());
        m_camera.setFollowVelocity(m_player.velX(), m_player.velY());
        m_camera.update(dt);
        m_app->gl().setCamera(m_camera.camera());
    }

    void GameLayer::onRender() {
        Renderer2D& r2d = m_app->renderer2D();

        r2d.beginScene(m_camera.camera(), RenderPass::World);
        m_player.render(r2d);
        r2d.endScene();
    }

    // -------------------------------------------------------------------------
    // Sprite pipeline: sprite shader + unit quad mesh. Mirrors the engine/Sandbox
    // setup. Tries the seeded shader files first, falls back to inline source.
    // -------------------------------------------------------------------------
    void GameLayer::buildSpritePipeline() {
        auto& resources = m_app->resources();

        m_spriteShader = resources.getOrCreateShaderFromFiles(
            "sprite",
            AssetPaths::Resolve("shaders/sprite.vert"),
            AssetPaths::Resolve("shaders/sprite.frag"));

        if (!m_spriteShader) {
            const char* spriteVs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

out vec2 vUV;
out vec4 vColor;

uniform mat4 uMVP;
uniform vec4 uUVRect; // xy offset, zw scale

void main() {
    vUV = aUV * uUVRect.zw + uUVRect.xy;
    vColor = aColor;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";
            const char* spriteFs = R"(#version 330 core
in vec2 vUV;
in vec4 vColor;
out vec4 FragColor;

uniform sampler2D uTex;
uniform vec4 uColor;

uniform int uIsSDF;
uniform float uSDFSoftness;

void main() {
    vec4 tex = texture(uTex, vUV);
    if (uIsSDF == 0) {
        FragColor = tex * uColor * vColor;
        return;
    }
    float dist = tex.a;
    float w = fwidth(dist) * max(uSDFSoftness, 0.001);
    float alpha = smoothstep(0.5 - w, 0.5 + w, dist);
    vec4 tinted = uColor * vColor;
    FragColor = vec4(tinted.rgb, tinted.a * alpha);
}
)";
            m_spriteShader = resources.getOrCreateShader("sprite", spriteVs, spriteFs);
        }

        if (!m_spriteShader) {
            LogFatal("MegaX GameLayer: failed to create sprite shader.");
            m_app->requestQuit();
            return;
        }

        // Unit quad centered on origin, with UVs. pos.xyz, uv.xy — 6 verts.
        const std::vector<float> quadVerts = {
            -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
             0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
             0.5f,  0.5f, 0.0f,  1.0f, 1.0f,

             0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
            -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,
            -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
        };

        m_quadMesh = resources.getOrCreateMeshPosUV("quad", quadVerts, 6);
        if (!m_quadMesh) {
            LogFatal("MegaX GameLayer: failed to create quad mesh.");
            m_app->requestQuit();
            return;
        }

        m_app->renderer2D().setSpriteQuadMesh(m_quadMesh);
    }

} // namespace MegaX
```

### Why these choices

- **`buildSpritePipeline` first, then everything else.** The player's material
  needs `m_spriteShader`, and its render item needs `m_quadMesh`, so the
  pipeline must exist before `m_player.init(...)`.
- **File-first shader, inline fallback.** Item 00 seeded
  `assets\shaders\sprite.vert/.frag`, so `getOrCreateShaderFromFiles` normally
  wins; the inline copy guarantees MegaX still runs if those files are missing.
  This is the exact shader the engine's `Material::apply` feeds
  (`uMVP`, `uUVRect`, `uTex`, `uColor`, `uIsSDF`, `uSDFSoftness`).
- **Camera each frame.** `setCamera` takes a *pointer* to the camera internally,
  but re-calling it every frame after `update(dt)` is cheap and unambiguous.
- **Raw `Platform::Input`** (not the semantic `HBE::Input` map) keeps item 1
  dependency-free — no default-binding registration needed. We can migrate to
  `HBE::Input` actions/axes in a later item when rebinding matters.
- **Zoom = 3** in one constant so you can tune the on-screen size in one place.

> **To swap the SPACE/S direction** (if you prefer S = up), change the `iy` line
> to `... IsKeyDown(SDL_SCANCODE_S) ... - ... IsKeyDown(SDL_SCANCODE_SPACE) ...`.

---

## Verify (doc 02)

- [ ] `include\Game\GameLayer.h` and `src\Game\GameLayer.cpp` match the above.
- [ ] `GameLayer.h` includes `Game/Player.h` and
      `HBE/Renderer/CameraController.h`.
- [ ] No include/reference of anything under `HBE.Sandbox`.
- [ ] `main.cpp` is unchanged from item 00 (still pushes `MegaX::GameLayer`).

Next: **`03_build_run_and_verify.md`**.
