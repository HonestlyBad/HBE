#include "GameLayer.h"

#include "HBE/Core/Application.h"
#include "HBE/Core/Log.h"

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
}

void GameLayer::onAttach(Application& app) {
	m_app = &app;

	// camera setup (logical resolution)
	m_camera.x = 0.0f;
	m_camera.y = 0.0f;
	m_camera.zoom = 1.0f;
	m_camera.viewportWidth = LOGICAL_WIDTH;
	m_camera.viewportHeight = LOGICAL_HEIGHT;

	app.gl().setCamera(m_camera);
	app.gl().setClearColor(0.1f, 0.2f, 0.35f, 1.0f);

	buildSpritePipeline();

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

    SpriteAnimationDesc idle{};
    idle.name = "Idle";
    idle.row = 0;
    idle.startCol = 0;
    idle.frameCount = 6;
    idle.frameDuration = 0.15f;
    idle.loop = true;

    SpriteAnimationDesc walk{};
    walk.name = "Walk";
    walk.row = 1;
    walk.startCol = 0;
    walk.frameCount = 6;
    walk.frameDuration = 0.10f;
    walk.loop = true;

    m_goblinAnimator.addClip(idle);
    m_goblinAnimator.addClip(walk);
    m_goblinAnimator.play("Idle", true);
}

void GameLayer::onUpdate(float dt) {
    // fullscreen toggle stays in layer for now 
    if (HBE::Platform::Input::IsKeyPressed(SDL_SCANCODE_F11)) {

    }

    bool moveUp = Input::IsKeyDown(SDL_SCANCODE_W) || Input::IsKeyDown(SDL_SCANCODE_UP);
    bool moveDown = Input::IsKeyDown(SDL_SCANCODE_S) || Input::IsKeyDown(SDL_SCANCODE_DOWN);
    bool moveLeft = Input::IsKeyDown(SDL_SCANCODE_A) || Input::IsKeyDown(SDL_SCANCODE_LEFT);
    bool moveRight = Input::IsKeyDown(SDL_SCANCODE_D) || Input::IsKeyDown(SDL_SCANCODE_RIGHT);

    Transform2D* tr = m_scene.getTransform(m_goblinEntity);
    if (!tr) return;

    float moveX = 0.0f;
    float moveY = 0.0f;
    if (moveUp) moveY += 1.0f;
    if (moveDown) moveY -= 1.0f;
    if (moveLeft) moveX -= 1.0f;
    if (moveRight) moveX += 1.0f;

    if (moveX != 0.0f || moveY != 0.0f) {
        float len = std::sqrt(moveX * moveX + moveY * moveY);
        if (len > 0.0f) { moveX /= len; moveY /= len; }
    }

    bool isMoving = (moveX != 0.0f || moveY != 0.0f);

    const float speed = 300.0f;
    tr->posX += moveX * speed * dt;
    tr->posY += moveY * speed * dt;

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
    // for now, layer is responsible for rendering its own scene
    Renderer2D& r2d = m_app->renderer2D();

    r2d.beginScene(m_camera);
    m_scene.render(r2d);
    r2d.endScene();
}