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

    HBE::Renderer::UI::UIStyle style;
    style.textScale = 1.0f;
    style.itemH = 34.0f;
    style.padding = 12.0f;
    style.spacing = 10.0f;
    m_ui.setStyle(style);

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
    uniform vec4 uColor;

    // Optional: SDF font rendering (TextRenderer2D toggles these via Material)
    uniform int uIsSDF;
    uniform float uSDFSoftness;

    void main() {
        vec4 tex = texture(uTex, vUV);

        // Normal sprite path
        if (uIsSDF == 0) {
            FragColor = tex * uColor;
            return;
        }

        // SDF path: distance is stored in alpha (0..1), edge is at 0.5
        float dist = tex.a;
        float w = fwidth(dist) * max(uSDFSoftness, 0.001);
        float alpha = smoothstep(0.5 - w, 0.5 + w, dist);
        FragColor = vec4(uColor.rgb, uColor.a * alpha);
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
    m_app->renderer2D().setSpriteQuadMesh(m_quadMesh);
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
    // Example font load (put a .ttf in your sandbox assets folder)
    if (!m_text.loadSDFont(m_app->resources(),
        "ui",
        "assets/fonts/BoldPixels.ttf",
        16.0f,   // bake size (bigger = better at huge scaling)
        1024, 1024,
        12       // padding (bigger = safer for extreme scaling)
    )) {
        LogError("FAILED to load font: assets/fonts/BoldPixels.ttf");
    }
    else {
        m_text.setActiveFont("ui");
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
    m_uiAnimT += dt;
    m_statTimer += (double)dt;
    m_updateCount++;

    m_ui.beginFrame(dt);

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

    // debug popup aging / movement
    for (auto& p : m_popups) {
        p.life -= dt;
        p.y += p.floatSpeed * dt; // float upward
    }

    // remove dead
    m_popups.erase(std::remove_if(m_popups.begin(), m_popups.end(), [](const DebugPopup& p) {return p.life <= 0.0f; }), m_popups.end());
}

void GameLayer::onRender() {
    m_frameCount++;

    Renderer2D& r2d = m_app->renderer2D();

    // -------------------------
    // PASS 1: WORLD
    // -------------------------
    r2d.beginScene(m_camera);

    // draw map
    m_tileRenderer.draw(r2d, m_tileMap);

    // --- DAMAGE TEXT (WORLD SPACE) ---
    HBE::Renderer::TextRenderer2D::TextAnim dmg{};
    dmg.t = std::fmod(m_uiAnimT, 1.0f);
    dmg.duration = 1.0f;
    dmg.autoExpire = true;
    dmg.fadeIn = true;
    dmg.fadeInTime = 0.05f;
    dmg.fadeOut = true;
    dmg.fadeOutTime = 0.35f;
    dmg.velY = 40.0f;
    dmg.startScale = 1.2f;
    dmg.endScale = 1.0f;

    Transform2D* tr = m_scene.getTransform(m_goblinEntity);
    if (tr) {
        m_text.drawTextAnimated(r2d,
            tr->posX, tr->posY + 40.0f,
            "-25",
            1.0f,
            { 1,0.2f,0.2f,1 },
            TextRenderer2D::TextAlignH::Center,
            TextRenderer2D::TextAlignV::Baseline,
            0.0f,
            1.0f,
            dmg);
    }

    // draw sprites/entities
    m_scene.render(r2d);

    // debug draw (WORLD)
    if (m_debugDraw) {
        m_debug.rect(r2d, m_playerBox.cx, m_playerBox.cy,
            m_playerBox.w, m_playerBox.h,
            1, 0, 0, 1, false);

        m_debug.rect(r2d, 8.0f, 8.0f, 16.0f, 16.0f, 1, 1, 0, 1, false);
        m_debug.rect(r2d, 32.0f, 32.0f, 64.0f, 64.0f, 0, 1, 0, 1, false);
    }

    r2d.endScene();


    // -------------------------
    // PASS 2: UI OVERLAY
    // -------------------------
    HBE::Renderer::Camera2D uiCam{};
    uiCam.x = LOGICAL_WIDTH * 0.5f;
    uiCam.y = LOGICAL_HEIGHT * 0.5f;
    uiCam.zoom = 1.0f;
    uiCam.viewportWidth = LOGICAL_WIDTH;
    uiCam.viewportHeight = LOGICAL_HEIGHT;

    r2d.beginScene(uiCam);

    // Bind UI renderer dependencies now that we’re in UI space
    m_ui.bind(&m_app->renderer2D(), &m_debug, &m_text);
    
    using namespace HBE::Renderer::UI;

    UIRect panel;
    panel.x = 20.0f;
    panel.y = 320.0f;
    panel.w = 260.0f;
    panel.h = 360.0f;

    m_ui.beginPanel("main_panel", panel, "HBE UI");

    m_ui.label("Widgets online.", true);
    m_ui.spacing(6.0f);

    if (m_ui.button("btn_1", "Print Hello")) {
        HBE::Core::LogInfo("Hello from UI button!");
    }

    m_ui.checkbox("cb_debug", "Debug Draw", m_uiDebugDraw);
    m_debugDraw = m_uiDebugDraw;

    m_ui.spacing(6.0f);

    m_ui.sliderFloat("sdl_volume", "Volume", m_volume, 0.0f, 1.0f, 0.01f);
    m_ui.sliderFloat("sld_bright", "Brightness", m_brightness, 0.2f, 2.0f, 0.01f);
    m_ui.sliderInt("sld_bars", "Stat Bars", m_statBars, 1, 10);

    m_ui.spacing(6.0f);

    if (m_ui.button("btn_3", "Quit")) {
        m_app->requestQuit();
    }

    m_ui.endPanel();

    // (Optional) click/scroll popups (these are UI space too)
    for (const auto& p : m_popups) {
        float t = (p.maxLife > 0.0f) ? (p.life / p.maxLife) : 0.0f;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;

        auto c = p.color;
        c.a *= t;
        m_text.drawText(r2d, p.x, p.y, p.text, 1.0f, c);
    }

    // End UI scene
    r2d.endScene();

    // Finish frame bookkeeping
    m_ui.endFrame();
}


bool GameLayer::onEvent(HBE::Core::Event& e) {
    using namespace HBE::Core;

    m_ui.onEvent(e);

    if (e.type() == EventType::WindowResize) {
        auto& re = static_cast<WindowResizeEvent&>(e);

        /// For now: keep logical camera constant (1280x720).
        // Later: we can use re.vpW/re.vpH if you want camera to match viewport.
        // camera.viewportWidth = LOGICAL_WIDTH;
        // camera.viewportHeight = LOGICAL_HEIGHT;

        return false; // not handled
    }
    //else if (e.type() == EventType::MouseButtonPressed) {
    //    auto& mb = static_cast<MouseButtonPressedEvent&>(e);

    //    if (mb.inViewport) {
    //        // left = 1, middle = 2, right = 3 in SDL mouse button numbering
    //        if (mb.button == 1) {
    //            spawnPopup(mb.logicalX, mb.logicalY, "Click!", HBE::Renderer::Color4{ 0,1,0,1 }); // green
    //            return true;
    //        }
    //        else if (mb.button == 3) {
    //            spawnPopup(mb.logicalX, mb.logicalY, "Click!", HBE::Renderer::Color4{ 1,0,0,1 }); // red
    //            return true;
    //        }
    //        else if (mb.button == 2) {
    //            spawnPopup(mb.logicalX, mb.logicalY, "Click!", HBE::Renderer::Color4{ 0,0,1,1 }); // blue
    //            return true;
    //        }
    //    }
    //    return false;
    //}
    //else if (e.type() == EventType::MouseScrolled) {
    //    auto& ms = static_cast<MouseScrolledEvent&>(e);

    //    if (ms.inViewport) {
    //        // SDL Wheel convention: +Y = away from user (often "scroll up")
    //        if (ms.wheelY > 0.0f) {
    //            spawnPopup(ms.logicalX, ms.logicalY, "Scrolled!", HBE::Renderer::Color4{ 0,1,0,1 }); // green
    //            return true;
    //        }
    //        else if (ms.wheelY < 0.0f) {
    //            spawnPopup(ms.logicalX, ms.logicalY, "Scrolled!", HBE::Renderer::Color4{ 1,0,0,1 });// red
    //            return true;
    //        }
    //    }
    //    return false;
    //}
    return false;
}

void GameLayer::spawnPopup(float x, float y, const std::string& text, const HBE::Renderer::Color4& color, float lifetimeSeconds, float floatUpSpeed) {
    DebugPopup p;
    p.text = text;
    p.x = x;
    p.y = y;
    p.color = color;
    p.life = lifetimeSeconds;
    p.maxLife = lifetimeSeconds;
    p.floatSpeed = floatUpSpeed;

    m_popups.push_back(std::move(p));
}