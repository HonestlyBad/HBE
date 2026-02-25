#include "GameLayer.h"

#include "HBE/Core/Application.h"
#include "HBE/Core/Log.h"
#include "HBE/Core/Event.h"

#include "HBE/Platform/Input.h"
#include "HBE/Input/InputMap.h"

#include "HBE/Renderer/GLShader.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/Mesh.h"
#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Transform2D.h"

#include "HBE/ECS/Components.h"

#include <SDL3/SDL_scancode.h>
#include <cmath>
#include <vector>
#include <string>

#include <algorithm> // remove_if
#include <utility>   // move

using namespace HBE::Core;
using namespace HBE::Renderer;
using namespace HBE::Platform;

namespace {
    constexpr float SPRITE_PIXEL_SCALE = 4.0f;

    // --- COLLIDER SIZE IN PIXELS (tweak these) ---
    // These are NOT frame sizes, they are "body" sizes.
    constexpr float PLAYER_BODY_W_PX = 8.0f;
    constexpr float PLAYER_BODY_H_PX = 14.0f;
    constexpr float PLAYER_BODY_Y_OFFSET_PX = +0.5f;

    constexpr float GOBLIN_BODY_W_PX = 10.0f;
    constexpr float GOBLIN_BODY_H_PX = 14.0f;
    constexpr float GOBLIN_BODY_Y_OFFSET_PX = +0.5f;

    // Where user overrides live (next to exe while developing)
    constexpr const char* BINDINGS_FILE = "bindings.cfg";

    static float Approach(float v, float target, float maxDelta) {
        if (v < target) return std::min(v + maxDelta, target);
        if (v > target) return std::max(v - maxDelta, target);
        return target;
    }

    // Game-owned default bindings (engine stays universal).
    static void RegisterDefaultBindings(HBE::Input::InputMap& map)
    {
        using namespace HBE::Input;

        // --- Actions ---
        map.bindAction(Action::Jump, Binding::Key(SDL_SCANCODE_SPACE), true);
        map.bindAction(Action::Jump, Binding::GamepadButton(SDL_GAMEPAD_BUTTON_SOUTH), false);

        // Attack default: E on keyboard, X/Square on controller
        map.bindAction(Action::Attack, Binding::Key(SDL_SCANCODE_E), true);
        map.bindAction(Action::Attack, Binding::GamepadButton(SDL_GAMEPAD_BUTTON_WEST), false);

        map.bindAction(Action::UIConfirm, Binding::Key(SDL_SCANCODE_RETURN), true);
        map.bindAction(Action::UIConfirm, Binding::GamepadButton(SDL_GAMEPAD_BUTTON_SOUTH), false);

        map.bindAction(Action::UICancel, Binding::Key(SDL_SCANCODE_ESCAPE), true);
        map.bindAction(Action::UICancel, Binding::GamepadButton(SDL_GAMEPAD_BUTTON_EAST), false);

        map.bindAction(Action::Pause, Binding::Key(SDL_SCANCODE_ESCAPE), true);
        map.bindAction(Action::Pause, Binding::GamepadButton(SDL_GAMEPAD_BUTTON_START), false);

        map.bindAction(Action::FullscreenToggle, Binding::Key(SDL_SCANCODE_F11), true);
        map.bindAction(Action::FullscreenToggle, Binding::None(), false);

        // --- Axes ---
        AxisBinding moveX{};
        moveX.negative = Binding::Key(SDL_SCANCODE_A);
        moveX.positive = Binding::Key(SDL_SCANCODE_D);
        moveX.negative2 = Binding::Key(SDL_SCANCODE_LEFT);
        moveX.positive2 = Binding::Key(SDL_SCANCODE_RIGHT);
        moveX.useGamepadAxis = true;
        moveX.gamepadAxis = SDL_GAMEPAD_AXIS_LEFTX;
        moveX.deadzone = 0.20f;
        moveX.invert = false;
        moveX.scale = 1.0f;
        map.bindAxis(Axis::MoveX, moveX);

        AxisBinding moveY{};
        moveY.negative = Binding::Key(SDL_SCANCODE_W); // up
        moveY.positive = Binding::Key(SDL_SCANCODE_S); // down
        moveY.negative2 = Binding::Key(SDL_SCANCODE_UP);
        moveY.positive2 = Binding::Key(SDL_SCANCODE_DOWN);
        moveY.useGamepadAxis = true;
        moveY.gamepadAxis = SDL_GAMEPAD_AXIS_LEFTY;
        moveY.deadzone = 0.20f;
        moveY.invert = false;
        moveY.scale = 1.0f;
        map.bindAxis(Axis::MoveY, moveY);
    }
}

void GameLayer::onAttach(Application& app) {
    m_app = &app;

    // ------------------------------------------------------------
    // Input Mapping Layer:
    // - Game defines defaults here
    // - Player overrides loaded from bindings.cfg
    // ------------------------------------------------------------
    HBE::Input::Initialize(&RegisterDefaultBindings);
    HBE::Input::Get().loadFromFile(BINDINGS_FILE); // safe if missing (returns false)

    // camera setup (logical resolution)
    m_camera.x = std::round(m_camera.x);
    m_camera.y = std::round(m_camera.y);
    m_camera.zoom = 1.0f;
    m_camera.viewportWidth = LOGICAL_WIDTH;
    m_camera.viewportHeight = LOGICAL_HEIGHT;

    m_text.setCullInset(8.0f);

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

    // Hook Scene2D physics/collision to this tilemap layer
    m_scene.setTileCollisionContext(&m_tileMap, m_collisionLayer);

    // Physics-lite tuning (gravity + substeps)
    {
        HBE::Renderer::Physics2DSettings phys{};
        phys.gravityY = -1800.0f;         // world units / s^2 (negative = down)
        phys.maxSubSteps = 4;             // stability
        phys.maxStepDt = 1.0f / 120.0f;   // stability
        m_scene.setPhysics2DSettings(phys);
    }

    // -----------------------------
    // ECS: attach gameplay components + scripts
    // -----------------------------
    auto& reg = m_scene.registry();

    // Player (soldier): Collider + Rigidbody + Script controller
    if (reg.valid(m_soldierEntity)) {
        const float pxScale = SPRITE_PIXEL_SCALE;

        HBE::ECS::Collider2D col{};
        col.halfW = 0.5f * (PLAYER_BODY_W_PX * pxScale);
        col.halfH = 0.5f * (PLAYER_BODY_H_PX * pxScale);
        col.offsetX = 0.0f;
        col.offsetY = (PLAYER_BODY_Y_OFFSET_PX * pxScale);

        if (!reg.has<HBE::ECS::Collider2D>(m_soldierEntity))
            reg.emplace<HBE::ECS::Collider2D>(m_soldierEntity, col);

        HBE::ECS::RigidBody2D rb{};
        rb.linearDamping = 0.0f;
        rb.isStatic = false;

        // ---- PLATFORMER DEFAULTS ----
        rb.useGravity = true;
        rb.gravityScale = 1.0f;
        rb.maxFallSpeed = -2200.0f; // clamp falling speed (negative down)

        // Step-up a bit less than half a tile feels good
        rb.maxStepUp = m_tileMap.worldTileH() * 0.35f;

        rb.enableOneWay = true;
        rb.enableSlopes = true;

        if (!reg.has<HBE::ECS::RigidBody2D>(m_soldierEntity))
            reg.emplace<HBE::ECS::RigidBody2D>(m_soldierEntity, rb);

        HBE::ECS::Script sc{};
        sc.name = "PlayerController";
        sc.onUpdate = [this](HBE::ECS::Entity e, float dt) {
            auto& r = m_scene.registry();
            if (!r.has<HBE::ECS::RigidBody2D>(e) || !r.has<Transform2D>(e)) return;

            auto& body = r.get<HBE::ECS::RigidBody2D>(e);

            // -------- INPUT (mapped) --------
            const float inputX = HBE::Input::AxisValue(HBE::Input::Axis::MoveX);
            const float inputY = HBE::Input::AxisValue(HBE::Input::Axis::MoveY); // if you want it later

            const bool Down = (inputY > 0.5f); // keeps your old "Down" behavior if you need it

            const bool JumpPressed = HBE::Input::ActionPressed(HBE::Input::Action::Jump);
            const bool AttackPressed = HBE::Input::ActionPressed(HBE::Input::Action::Attack);

            // -------- TUNING --------
            const float moveSpeed = 520.0f;     // max run speed
            const float accelGround = 5200.0f;  // reach speed quickly on ground
            const float accelAir = 3200.0f;     // air control
            const float friction = 6200.0f;     // ground stop when no input
            const float jumpVel = 780.0f;       // initial jump velocity (up)

            // We let Scene2D apply gravity; don't fight it
            body.accelY = 0.0f;

            // Horizontal movement: accelerate toward target velocity
            const float targetVX = inputX * moveSpeed;
            const float ax = body.grounded ? accelGround : accelAir;

            if (inputX != 0.0f) {
                body.velX = Approach(body.velX, targetVX, ax * dt);
            }
            else if (body.grounded) {
                body.velX = Approach(body.velX, 0.0f, friction * dt);
            }

            // Jump (only if grounded)
            if (JumpPressed && body.grounded && !Down) {
                body.velY = jumpVel;
                body.grounded = false;
            }

            // Drop-through one-way: hold Down + press Jump
            if (JumpPressed && Down) {
                body.oneWayDisableTimer = 0.20f; // seconds
                body.velY = std::min(body.velY, -120.0f); // nudge down
                body.grounded = false;
            }

            // Anim parameters
            if (auto* sAnim = m_scene.getSpriteAnimator(e)) {
                const bool moving = (std::fabs(body.velX) > 5.0f);
                sAnim->setBool("moving", moving);

                if (AttackPressed) {
                    sAnim->trigger("attack");
                }
            }
            };

        if (!reg.has<HBE::ECS::Script>(m_soldierEntity))
            reg.emplace<HBE::ECS::Script>(m_soldierEntity, std::move(sc));
    }

    // Goblin: Collider + STATIC Rigidbody + optional script
    if (reg.valid(m_goblinEntity)) {
        const float pxScale = SPRITE_PIXEL_SCALE;

        HBE::ECS::Collider2D col{};
        col.halfW = 0.5f * (GOBLIN_BODY_W_PX * pxScale);
        col.halfH = 0.5f * (GOBLIN_BODY_H_PX * pxScale);
        col.offsetX = 0.0f;
        col.offsetY = (GOBLIN_BODY_Y_OFFSET_PX * pxScale);

        if (!reg.has<HBE::ECS::Collider2D>(m_goblinEntity))
            reg.emplace<HBE::ECS::Collider2D>(m_goblinEntity, col);

        HBE::ECS::RigidBody2D rb{};
        rb.isStatic = true;     // IMPORTANT: stays put, acts like a blocker
        rb.linearDamping = 0.0f;

        if (!reg.has<HBE::ECS::RigidBody2D>(m_goblinEntity))
            reg.emplace<HBE::ECS::RigidBody2D>(m_goblinEntity, rb);

        // Goblin test script (mapped UIConfirm = Enter / A)
        HBE::ECS::Script sc{};
        sc.name = "GoblinTest";
        sc.onUpdate = [this](HBE::ECS::Entity e, float dt) {
            (void)dt;
            if (auto* gAnim = m_scene.getSpriteAnimator(e)) {
                gAnim->setBool("moving", false);
                if (HBE::Input::ActionPressed(HBE::Input::Action::UIConfirm)) {
                    gAnim->trigger("attack");
                }
            }
            };

        if (!reg.has<HBE::ECS::Script>(m_goblinEntity))
            reg.emplace<HBE::ECS::Script>(m_goblinEntity, std::move(sc));
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

    uniform int uIsSDF;
    uniform float uSDFSoftness;

    void main() {
        vec4 tex = texture(uTex, vUV);

        if (uIsSDF == 0) {
            FragColor = tex * uColor;
            return;
        }

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

    // quad mesh (pos + uv) as 2 triangles (6 verts) for the current Mesh API
    std::vector<float> quadVerts = {
        // tri 1
        -0.5f, -0.5f, 0.0f,   0.0f, 0.0f,
         0.5f, -0.5f, 0.0f,   1.0f, 0.0f,
         0.5f,  0.5f, 0.0f,   1.0f, 1.0f,

         // tri 2
          0.5f,  0.5f, 0.0f,   1.0f, 1.0f,
         -0.5f,  0.5f, 0.0f,   0.0f, 1.0f,
         -0.5f, -0.5f, 0.0f,   0.0f, 0.0f,
    };

    m_quadMesh = resources.getOrCreateMeshPosUV("quad", quadVerts, 6);
    if (!m_quadMesh) {
        LogFatal("GameLayer: failed to create quad mesh.");
        m_app->requestQuit();
        return;
    }

    // Debug draw setup (uses the same quad mesh)
    if (!m_debug.initialize(resources, m_quadMesh)) {
        LogFatal("GameLayer: DebugDraw2D init failed");
        m_app->requestQuit();
        return;
    }

    // Text renderer
    if (!m_text.initialize(m_app->resources(), m_spriteShader, m_quadMesh)) {
        LogFatal("GameLayer: TextRenderer2D init failed");
        m_app->requestQuit();
        return;
    }

    // Font
    if (!m_text.loadSDFont(m_app->resources(),
        "ui",
        "assets/fonts/BoldPixels.ttf",
        16.0f,
        1024, 1024,
        12))
    {
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
    m_soldierSheet = SpriteRenderer2D::declareSpriteSheet(resources, "soldier_sheet", "assets/Soldier.png", desc);

    if (!m_goblinSheet.texture) {
        LogFatal("GameLayer: failed to load assets/Orc.png");
        m_app->requestQuit();
        return;
    }
    if (!m_soldierSheet.texture) {
        LogFatal("GameLayer: failed to load assets/Soldier.png");
        m_app->requestQuit();
        return;
    }

    // materials
    m_goblinMaterial.shader = m_spriteShader;
    m_goblinMaterial.texture = m_goblinSheet.texture;

    m_soldierMaterial.shader = m_spriteShader;
    m_soldierMaterial.texture = m_soldierSheet.texture;

    // entities
    RenderItem goblin{};
    goblin.mesh = m_quadMesh;
    goblin.material = &m_goblinMaterial;
    goblin.transform.posX = 1080.0f;
    goblin.transform.posY = 95.0f;
    goblin.transform.scaleX = desc.frameWidth * SPRITE_PIXEL_SCALE;
    goblin.transform.scaleY = desc.frameHeight * SPRITE_PIXEL_SCALE;
    goblin.layer = 100; // base entity layer
    goblin.sortKey = goblin.transform.posY;

    RenderItem soldier{};
    soldier.mesh = m_quadMesh;
    soldier.material = &m_soldierMaterial;
    soldier.transform.posX = 64.0f;
    soldier.transform.posY = 95.0f;
    soldier.transform.scaleX = desc.frameWidth * SPRITE_PIXEL_SCALE;
    soldier.transform.scaleY = desc.frameHeight * SPRITE_PIXEL_SCALE;

    // Make player render above goblin:
    soldier.layer = 200;
    soldier.sortKey = soldier.transform.posY;

    SpriteRenderer2D::setFrame(goblin, m_goblinSheet, 0, 0);
    SpriteRenderer2D::setFrame(soldier, m_soldierSheet, 0, 0);

    m_goblinEntity = m_scene.createEntity(goblin);
    m_soldierEntity = m_scene.createEntity(soldier);

    // -----------------------------
    // Per-entity anim SM setup
    // -----------------------------

    // Goblin
    if (auto* gAnim = m_scene.addSpriteAnimator(m_goblinEntity, &m_goblinSheet)) {
        gAnim->addClip({ "Idle",   0, 0, 6, 0.10f, true,  1.0f });
        gAnim->addClip({ "Run",    1, 0, 6, 0.08f, true,  1.0f });
        gAnim->addClip({ "Attack", 2, 0, 6, 0.07f, false, 1.0f });

        gAnim->addEvent("Run", 1, "footstep");
        gAnim->addEvent("Run", 4, "footstep");
        gAnim->addEvent("Attack", 3, "hitframe");

        gAnim->addState("Idle", "Idle");
        gAnim->addState("Run", "Run");
        gAnim->addState("Attack", "Attack");

        gAnim->addTransitionTrigger("*", "Attack", "attack");
        gAnim->addTransitionBool("Idle", "Run", "moving", true);
        gAnim->addTransitionBool("Run", "Idle", "moving", false);
        gAnim->addTransitionFinished("Attack", "Idle");

        gAnim->setState("Idle", true);
    }

    // Soldier
    if (auto* sAnim = m_scene.addSpriteAnimator(m_soldierEntity, &m_soldierSheet)) {
        sAnim->addClip({ "Idle",   0, 0, 6, 0.10f, true,  1.0f });
        sAnim->addClip({ "Run",    1, 0, 8, 0.10f, true,  1.0f });
        sAnim->addClip({ "Attack", 2, 0, 7, 0.07f, false, 1.0f });

        sAnim->addEvent("Run", 2, "footstep");
        sAnim->addEvent("Run", 6, "footstep");
        sAnim->addEvent("Attack", 4, "hitframe");

        sAnim->addState("Idle", "Idle");
        sAnim->addState("Run", "Run");
        sAnim->addState("Attack", "Attack");

        sAnim->addTransitionTrigger("*", "Attack", "attack");
        sAnim->addTransitionBool("Idle", "Run", "moving", true);
        sAnim->addTransitionBool("Run", "Idle", "moving", false);
        sAnim->addTransitionFinished("Attack", "Idle");

        sAnim->setState("Idle", true);
    }
}

void GameLayer::onUpdate(float dt) {
    // fullscreen toggle stays in layer for now
    if (HBE::Input::ActionPressed(HBE::Input::Action::FullscreenToggle)) {
        // (left blank like your current file)
    }

    // ---- stats ----
    m_uiAnimT += dt;
    m_statTimer += (double)dt;
    m_updateCount++;
    m_lastDt = dt;

    if (m_statTimer >= 0.5) {
        const double window = m_statTimer;

        m_fps = (float)((double)m_frameCount / window);
        m_ups = (float)((double)m_updateCount / window);

        m_frameCount = 0;
        m_updateCount = 0;
        m_statTimer = 0.0;
    }

    // Controlled entity (for camera follow)
    Transform2D* playerTr = m_scene.getTransform(m_soldierEntity);
    if (!playerTr) return;

    // Tick scripts + physics + animators.
    // Catch animation events here.
    m_scene.update(dt, [&](const std::string& ev) {
        Transform2D* tr = m_scene.getTransform(m_soldierEntity);
        const float px = tr ? tr->posX : m_camera.x;
        const float py = tr ? tr->posY : m_camera.y;

        if (ev == "footstep") {
            spawnPopup(px, py - 30.0f, "step", Color4{ 0.8f,0.9f,1.0f,1.0f }, 0.35f, 35.0f);
        }
        else if (ev == "hitframe") {
            spawnPopup(px, py + 50.0f, "HIT!", Color4{ 1.0f,0.3f,0.2f,1.0f }, 0.55f, 25.0f);
        }
        });

    // camera follow
    m_camera.x = playerTr->posX;
    m_camera.y = playerTr->posY;
    m_app->gl().setCamera(m_camera);

    // debug popup aging / movement
    for (auto& p : m_popups) {
        p.life -= dt;
        p.y += p.floatSpeed * dt;
    }

    // remove dead
    m_popups.erase(
        std::remove_if(m_popups.begin(), m_popups.end(),
            [](const DebugPopup& p) { return p.life <= 0.0f; }),
        m_popups.end()
    );
}

void GameLayer::onRender() {
    m_frameCount++;

    // Reset per-frame text renderer state
    m_text.beginFrame(m_frameCount);

    Renderer2D& r2d = m_app->renderer2D();
    m_ui.beginFrame(m_lastDt);

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

    Transform2D* trg = m_scene.getTransform(m_goblinEntity);
    if (trg) {
        m_text.drawTextAnimated(r2d,
            trg->posX, trg->posY + 40.0f,
            "-25",
            1.0f,
            { 255,0.0f,0.0f,1 },
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
        auto& reg = m_scene.registry();

        // Player collider (red)
        if (reg.valid(m_soldierEntity) &&
            reg.has<Transform2D>(m_soldierEntity) &&
            reg.has<HBE::ECS::Collider2D>(m_soldierEntity))
        {
            const auto& tr = reg.get<Transform2D>(m_soldierEntity);
            const auto& col = reg.get<HBE::ECS::Collider2D>(m_soldierEntity);

            const float cx = tr.posX + col.offsetX;
            const float cy = tr.posY + col.offsetY;
            const float w = col.halfW * 2.0f;
            const float h = col.halfH * 2.0f;

            // DebugDraw2D::rect takes CENTER coords in your build
            m_debug.rect(r2d, cx, cy, w, h, 1, 0, 0, 1, false);
        }

        // Goblin collider (green)
        if (reg.valid(m_goblinEntity) &&
            reg.has<Transform2D>(m_goblinEntity) &&
            reg.has<HBE::ECS::Collider2D>(m_goblinEntity))
        {
            const auto& tr = reg.get<Transform2D>(m_goblinEntity);
            const auto& col = reg.get<HBE::ECS::Collider2D>(m_goblinEntity);

            const float cx = tr.posX + col.offsetX;
            const float cy = tr.posY + col.offsetY;
            const float w = col.halfW * 2.0f;
            const float h = col.halfH * 2.0f;

            m_debug.rect(r2d, cx, cy, w, h, 0, 1, 0, 1, false);
        }
    }

    // world popups
    for (const auto& p : m_popups) {
        float t = (p.maxLife > 0.0f) ? (p.life / p.maxLife) : 0.0f;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;

        Color4 c = p.color;
        c.a *= t;

        m_text.drawText(r2d, p.x, p.y, p.text, 1.0f, c);
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

    r2d.endScene();

    m_ui.endFrame();
}

bool GameLayer::onEvent(HBE::Core::Event& e) {
    using namespace HBE::Core;

    m_ui.onEvent(e);

    if (e.type() == EventType::WindowResize) {
        auto& re = static_cast<WindowResizeEvent&>(e);
        (void)re;
        return false;
    }

    return false;
}

void GameLayer::spawnPopup(float x, float y, const std::string& text,
    const HBE::Renderer::Color4& color, float lifetimeSeconds, float floatUpSpeed)
{
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