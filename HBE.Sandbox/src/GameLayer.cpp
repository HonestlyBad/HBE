#include "GameLayer.h"

#include "HBE/Core/Application.h"
#include "HBE/Core/Log.h"
#include "HBE/Core/Event.h"

#include "HBE/Platform/Input.h"

#include "HBE/Renderer/GLShader.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/Mesh.h"
#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Transform2D.h"

#include <SDL3/SDL_scancode.h>
#include <cmath>
#include <vector>
#include <string>

using namespace HBE::Core;
using namespace HBE::Renderer;
using namespace HBE::Platform;

namespace {
	constexpr float SPRITE_PIXEL_SCALE = 4.0f;
    // --- COLLIDER SIZE IN PIXELS (tweak these) ---
// This is NOT the frame size (100x100). This is the goblin's "body" size.
    constexpr float PLAYER_BODY_W_PX = 8.0f;
    constexpr float PLAYER_BODY_H_PX = 14.0f;

    // Optional: shift collider down so feet sit on ground nicer
    constexpr float PLAYER_BODY_Y_OFFSET_PX = +0.5f;
}

void GameLayer::onAttach(Application& app) {
	m_app = &app;

	// camera setup (logical resolution)
    m_camera.x = std::round(m_camera.x);
    m_camera.y = std::round(m_camera.y);
	m_camera.zoom = 1.0f;
	m_camera.viewportWidth = LOGICAL_WIDTH;
	m_camera.viewportHeight = LOGICAL_HEIGHT;

	app.gl().setCamera(m_camera);
	app.gl().setClearColor(0.1f, 0.2f, 0.35f, 1.0f);

	buildSpritePipeline();

    // load Tilemap
    std::string err;
    if (!HBE::Renderer::TileMapLoader::loadFromJsonFile("assets/maps/test_map.json", m_tileMap, &err)) {
        LogFatal("Failed to load tilemap: " + err);
        m_app->requestQuit();
        return;
    }

    // build tile renderer (uses same sprite shader + quad mesh)
    if (!m_tileRenderer.build(m_app->renderer2D(), m_app->resources(), m_spriteShader, m_quadMesh, m_tileMap)) {
        LogFatal("Failed to build TileMapRenderer");
        m_app->requestQuit();
        return;
    }

    // choose collision layer by name
    m_collisionLayer = m_tileMap.findLayer("Ground");
    if (!m_collisionLayer) {
        LogFatal("Tilemap missing collision layer named 'Ground'");
        m_app->requestQuit();
        return;
    }

    // Initialize player box to match goblin size (center-based);
    if (auto* tr = m_scene.getTransform(m_goblinEntity)) {
        const float pxScale = SPRITE_PIXEL_SCALE; // or m_tileMap.tilePixelScale if you store it there

        m_playerBox.w = PLAYER_BODY_W_PX * pxScale;
        m_playerBox.h = PLAYER_BODY_H_PX * pxScale;

        m_playerBox.cx = tr->posX;
        m_playerBox.cy = tr->posY + (PLAYER_BODY_Y_OFFSET_PX * pxScale);
    }

	LogInfo("GameLayer attached.");
}

void GameLayer::buildSpritePipeline() {
	auto& resources = m_app->resources();

    // sprite shader
    const char* spriteVs = R"(#version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec2 aUV;

        out vec2 vUV;

        uniform mat4 uMVP;
        uniform vec4 uUVRect; // xy offset, zw scale

        void main() {
            vUV = aUV * uUVRect.zw + uUVRect.xy;
            gl_Position = uMVP * vec4(aPos, 1.0);
        }
    )";

    const char* spriteFs = R"(#version 330 core
        in vec2 vUV;
        out vec4 FragColor;

        uniform sampler2D uTex;

        void main() {
            FragColor = texture(uTex, vUV);
        }
    )";

    m_spriteShader = resources.getOrCreateShader("sprite", spriteVs, spriteFs);
    if (!m_spriteShader) {
        LogFatal("GameLayer: failed to create sprite shader.");
        m_app->requestQuit();
        return;
    }

    // quad mesh (pos + uv)
    std::vector<float> quadVerts = {
        // x,     y,     z,    u,    v
        -0.5f, -0.5f,  0.0f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.0f,  1.0f, 1.0f,

        -0.5f, -0.5f,  0.0f,  0.0f, 0.0f,
         0.5f,  0.5f,  0.0f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.0f,  0.0f, 1.0f
    };

    m_quadMesh = resources.getOrCreateMeshPosUV("quad_pos_uv", quadVerts, 6);
    if (!m_quadMesh) {
        LogFatal("GameLayer: Failed to create quad mesh");
        m_app->requestQuit();
        return;
    }
    // Debug draw setup (uses the same quad mesh)
    if (!m_debug.initialize(resources, m_quadMesh)) {
        LogFatal("GameLayer: DebugDraw2D init failed");
        m_app->requestQuit();
        return;
    }

    if (!m_text.initialize(m_app->resources(), m_spriteShader, m_quadMesh)) {
        LogFatal("GameLayer: TextRenderer2D init failed");
        m_app->requestQuit();
        return;
    }

    // Sprite sheet
    SpriteSheetDesc desc{};
    desc.frameWidth = 100;
    desc.frameHeight = 100;

    m_goblinSheet = SpriteRenderer2D::declareSpriteSheet(resources, "orc_sheet", "assets/Orc.png", desc);

    if (!m_goblinSheet.texture) {
        LogFatal("GameLayer: failed to load assets/Orc.png");
        m_app->requestQuit();
        return;
    }

    // material
    m_goblinMaterial.shader = m_spriteShader;
    m_goblinMaterial.texture = m_goblinSheet.texture;

    // entity
    RenderItem goblin{};
    goblin.mesh = m_quadMesh;
    goblin.material = &m_goblinMaterial;
    goblin.transform.posX = 0.0f;
    goblin.transform.posY = 0.0f;
    goblin.transform.scaleX = desc.frameWidth * SPRITE_PIXEL_SCALE;
    goblin.transform.scaleY = desc.frameHeight * SPRITE_PIXEL_SCALE;

    SpriteRenderer2D::setFrame(goblin, m_goblinSheet, 0, 0);

    m_goblinEntity = m_scene.createEntity(goblin);

    // animator
    m_goblinAnimator.sheet = &m_goblinSheet;

    SpriteAnimationDesc GoblinIdle{};
    GoblinIdle.name = "Idle";
    GoblinIdle.row = 0;
    GoblinIdle.startCol = 0;
    GoblinIdle.frameCount = 6;
    GoblinIdle.frameDuration = 0.15f;
    GoblinIdle.loop = true;

    SpriteAnimationDesc GoblinWalk{};
    GoblinWalk.name = "Walk";
    GoblinWalk.row = 1;
    GoblinWalk.startCol = 0;
    GoblinWalk.frameCount = 6;
    GoblinWalk.frameDuration = 0.10f;
    GoblinWalk.loop = true;

    m_goblinAnimator.addClip(GoblinIdle);
    m_goblinAnimator.addClip(GoblinWalk);
    m_goblinAnimator.play("Idle", true);
}

void GameLayer::onUpdate(float dt) {
    // fullscreen toggle stays in layer for now 
    if (HBE::Platform::Input::IsKeyPressed(SDL_SCANCODE_F11)) {

    }

    // ---- stats ----
    m_statTimer += (double)dt;
    m_updateCount++;

    if (m_statTimer >= 0.5) { // update twice per second
        const double window = m_statTimer;

        m_fps = (float)((double)m_frameCount / window);
        m_ups = (float)((double)m_updateCount / window);

        m_frameCount = 0;
        m_updateCount = 0;
        m_statTimer = 0.0;
    }


    Transform2D* tr = m_scene.getTransform(m_goblinEntity);
    if (!tr) return;

    bool Up = Input::IsKeyDown(SDL_SCANCODE_W) || Input::IsKeyDown(SDL_SCANCODE_UP);
    bool Down = Input::IsKeyDown(SDL_SCANCODE_S) || Input::IsKeyDown(SDL_SCANCODE_DOWN);
    bool Left = Input::IsKeyDown(SDL_SCANCODE_A) || Input::IsKeyDown(SDL_SCANCODE_LEFT);
    bool Right = Input::IsKeyDown(SDL_SCANCODE_D) || Input::IsKeyDown(SDL_SCANCODE_RIGHT);

    float moveX = 0.0f;
    float moveY = 0.0f;
    if (Up) moveY += 1.0f;
    if (Down) moveY -= 1.0f;
    if (Left) moveX -= 1.0f;
    if (Right) moveX += 1.0f;

    if (moveX != 0.0f || moveY != 0.0f) {
        float len = std::sqrt(moveX * moveX + moveY * moveY);
        if (len > 0.0f) { moveX /= len; moveY /= len; }
    }

    const float speed = 300.0f;
    m_velX = moveX * speed;
    m_velY = moveY * speed;

    // sync player box center with transform center before moving
    const float pxScale = SPRITE_PIXEL_SCALE;

    m_playerBox.cx = tr->posX;
    m_playerBox.cy = tr->posY + (PLAYER_BODY_Y_OFFSET_PX * pxScale);

    // collide against tiles
    HBE::Renderer::TileCollision::moveAndCollide(m_tileMap, *m_collisionLayer, m_playerBox, m_velX, m_velY, dt);

    // apply resolved position back to entity
    tr->posX = m_playerBox.cx;
    tr->posY = m_playerBox.cy - (PLAYER_BODY_Y_OFFSET_PX * pxScale);


    // animation logic
    bool isMoving = (moveX != 0.0f || moveY != 0.0f);
    if (isMoving) m_goblinAnimator.play("Walk");
    else m_goblinAnimator.play("Idle");

    m_goblinAnimator.update(dt);
    if (RenderItem* item = m_scene.getRenderItem(m_goblinEntity)) {
        m_goblinAnimator.apply(*item);
    }

    // camera follow
    m_camera.x = tr->posX;
    m_camera.y = tr->posY;
    m_app->gl().setCamera(m_camera);
}

void GameLayer::onRender() {
    m_frameCount++;
    // for now, layer is responsible for rendering its own scene
    Renderer2D& r2d = m_app->renderer2D();

    r2d.beginScene(m_camera);

    // draw map
    m_tileRenderer.draw(r2d, m_tileMap);

    // draw sprites/ entites
    m_scene.render(r2d);

    if (m_debugDraw) {

        // 1) Draw player AABB (based on sprite transform for now)
        // If you already have a real collision AABB, use that values instead.
        Transform2D* tr = m_scene.getTransform(m_goblinEntity);
        if (tr) {
            // Approx player box: use sprite scale but slightly smaller if you want
            float boxW = tr->scaleX * 0.60f;
            float boxH = tr->scaleY * 0.80f;

            m_debug.rect(r2d, m_playerBox.cx, m_playerBox.cy,
                m_playerBox.w, m_playerBox.h,
                1, 0, 0, 1, false); // red
        }

        // 2) OPTIONAL: draw a collision-grid cell size reference (16x16 = current collision code)
        // This is the BIG one: if your rendered tiles are 64x64 (16*4),
        // and collision uses 16x16, you’ll see the mismatch instantly.
        //
        // Example: draw a 16x16 cell at world origin
        m_debug.rect(r2d, 8.0f, 8.0f, 16.0f, 16.0f, 1, 1, 0, 1, false);

        // If you want, also draw what a "rendered tile size" would be (example 64x64):
        m_debug.rect(r2d, 32.0f, 32.0f, 64.0f, 64.0f, 0, 1, 0, 1, false); // green
    }

    if (m_debugDraw) {
        // UI camera: screen-space
        HBE::Renderer::Camera2D uiCam{};
        uiCam.x = LOGICAL_WIDTH * 0.5f;
        uiCam.y = LOGICAL_HEIGHT * 0.5f;
        uiCam.zoom = 1.0f;
        uiCam.viewportWidth = LOGICAL_WIDTH;
        uiCam.viewportHeight = LOGICAL_HEIGHT;

        // Draw overlay with UI camera
        r2d.endScene();          // finish world
        r2d.beginScene(uiCam);   // begin UI

        // top-left: (10, LOGICAL_HEIGHT - 20)
        std::string s = "FPS: " + std::to_string((int)m_fps) + " UPS: " + std::to_string((int)m_ups);
        m_text.drawText(r2d, 10.0f, LOGICAL_HEIGHT - 26.0f, s, 2.0f);

        r2d.endScene();
        return;
    }


    r2d.endScene();
}

bool GameLayer::onEvent(HBE::Core::Event& e) {
    using namespace HBE::Core;

    if (e.type() == EventType::WindowResize) {
        auto& re = static_cast<WindowResizeEvent&>(e);

        /// For now: keep logical camera constant (1280x720).
        // Later: we can use re.vpW/re.vpH if you want camera to match viewport.
        // camera.viewportWidth = LOGICAL_WIDTH;
        // camera.viewportHeight = LOGICAL_HEIGHT;

        return false; // not handled
    }
    return false;
}