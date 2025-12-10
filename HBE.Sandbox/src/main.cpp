#include "HBE/Core/Log.h"
#include "HBE/Core/Time.h"

#include "HBE/Platform/SDLPlatform.h"
#include "HBE/Platform/Input.h"

#include "HBE/Renderer/GLRenderer.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/Mesh.h"
#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Camera2D.h"
#include "HBE/Renderer/Texture2D.h"
#include "HBE/Renderer/Scene2D.h"
#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/Transform2D.h"
#include "HBE/Renderer/SpriteRenderer2D.h"

#include <SDL3/SDL.h>
#include <cmath>
#include <vector>
#include <string>

using namespace HBE::Core;
using namespace HBE::Platform;
using namespace HBE::Renderer;

// Logical play area (doesn't change with window / monitor resolution)
static constexpr float LOGICAL_WIDTH = 1280.0f;
static constexpr float LOGICAL_HEIGHT = 720.0f;

int main() {
    SetLogLevel(LogLevel::Trace);

    // ---------------------------------------------------------------------
    // Window / platform setup
    // ---------------------------------------------------------------------
    WindowConfig windowCfg;
    windowCfg.title = "HBE Sandbox - OpenGL";
    windowCfg.width = static_cast<int>(LOGICAL_WIDTH);
    windowCfg.height = static_cast<int>(LOGICAL_HEIGHT);
    windowCfg.useOpenGL = true;
    windowCfg.mode = WindowMode::Windowed;          // needs to exist in SDLPlatform.h
    windowCfg.vsync = true;

    SDLPlatform platform;
    if (!platform.initialize(windowCfg)) {
        LogFatal("Failed to initialize SDLPlatform. Exiting.");
        return -1;
    }

    // ---------------------------------------------------------------------
    // Renderer backend + 2D renderer
    // ---------------------------------------------------------------------
    GLRenderer renderer;
    if (!renderer.initialize(platform)) {
        LogFatal("Failed to initialize GLRenderer. Exiting.");
        return -1;
    }

    renderer.setClearColor(0.1f, 0.2f, 0.35f, 1.0f);

    // Track actual window size and keep GL viewport in sync
    int winW = 0;
    int winH = 0;
    SDL_GetWindowSizeInPixels(platform.getWindow(), &winW, &winH);
    if (winW <= 0 || winH <= 0) {
        winW = windowCfg.width;
        winH = windowCfg.height;
    }
    renderer.resizeViewport(winW, winH);

    Renderer2D    renderer2D(renderer);
    ResourceCache resources;

    // ---------------------------------------------------------------------
    // Sprite shader (textured quad with UVRect)
    // ---------------------------------------------------------------------
    const char* spriteVs = R"(#version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec2 aUV;

        out vec2 vUV;

        uniform mat4 uMVP;
        // xy = offset, zw = scale in UV space
        uniform vec4 uUVRect;

        void main()
        {
            vec2 baseUV = aUV;                 // 0..1 from the quad mesh
            vUV = baseUV * uUVRect.zw + uUVRect.xy;
            gl_Position = uMVP * vec4(aPos, 1.0);
        }
    )";

    const char* spriteFs = R"(#version 330 core
        in vec2 vUV;
        out vec4 FragColor;

        uniform sampler2D uTex;

        void main()
        {
            FragColor = texture(uTex, vUV);
        }
    )";

    GLShader* spriteShader =
        resources.getOrCreateShader("sprite", spriteVs, spriteFs);
    if (!spriteShader) {
        LogFatal("Failed to create sprite shader via ResourceCache.");
        return -1;
    }

    // ---------------------------------------------------------------------
    // Quad mesh (pos + UV) for sprites
    // ---------------------------------------------------------------------
    std::vector<float> quadVerts = {
        // x,     y,     z,    u,    v
        -0.5f, -0.5f,  0.0f,  0.0f, 0.0f, // bottom-left
         0.5f, -0.5f,  0.0f,  1.0f, 0.0f, // bottom-right
         0.5f,  0.5f,  0.0f,  1.0f, 1.0f, // top-right

        -0.5f, -0.5f,  0.0f,  0.0f, 0.0f, // bottom-left
         0.5f,  0.5f,  0.0f,  1.0f, 1.0f, // top-right
        -0.5f,  0.5f,  0.0f,  0.0f, 1.0f  // top-left
    };

    Mesh* quadMesh =
        resources.getOrCreateMeshPosUV("quad_pos_uv", quadVerts, 6);
    if (!quadMesh) {
        LogFatal("Failed to create quad mesh via ResourceCache.");
        return -1;
    }

    // ---------------------------------------------------------------------
    // Goblin sprite sheet via SpriteRenderer2D
    // Your sheet: 800x600, 8 columns x 6 rows, each frame 100x100
    // ---------------------------------------------------------------------
    SpriteSheetDesc goblinDesc;
    goblinDesc.frameWidth = 100;
    goblinDesc.frameHeight = 100;
    goblinDesc.marginX = 0;
    goblinDesc.marginY = 0;
    goblinDesc.spacingX = 0;
    goblinDesc.spacingY = 0;

    auto goblinSheet = SpriteRenderer2D::declareSpriteSheet(
        resources,
        "orc_sheet",                // cache key
        "assets/Orc.png",   // actual file path
        goblinDesc
    );
    if (!goblinSheet.texture) {
        LogFatal("Failed to load Orc.png");
        return -1;
    }

    // ---------------------------------------------------------------------
    // Camera setup (logical resolution)
    // ---------------------------------------------------------------------
    Camera2D camera;
    camera.x = 0.0f;
    camera.y = 0.0f;
    camera.zoom = 1.0f;
    camera.viewportWidth = LOGICAL_WIDTH;
    camera.viewportHeight = LOGICAL_HEIGHT;
    renderer.setCamera(camera);

    // ---------------------------------------------------------------------
    // Materials
    // ---------------------------------------------------------------------
    Material goblinMaterial;
    goblinMaterial.shader = spriteShader;
    goblinMaterial.texture = goblinSheet.texture;

    // ---------------------------------------------------------------------
    // Scene + goblin entity
    // ---------------------------------------------------------------------
    Scene2D scene;

    RenderItem goblinTemplate;
    goblinTemplate.mesh = quadMesh;
    goblinTemplate.material = &goblinMaterial;
    goblinTemplate.transform.posX = 0.0f;
    goblinTemplate.transform.posY = 0.0f;

    constexpr float SPRITE_PIXEL_SCALE = 4.0f;
    goblinTemplate.transform.scaleX = goblinDesc.frameWidth * SPRITE_PIXEL_SCALE;
    goblinTemplate.transform.scaleY = goblinDesc.frameHeight * SPRITE_PIXEL_SCALE;
    goblinTemplate.transform.rotation = 0.0f;

    // Initial frame: first cell (col 0, row 0)
    SpriteRenderer2D::setFrame(goblinTemplate, goblinSheet, 0, 0);

    EntityID goblinEntity = scene.createEntity(goblinTemplate);

    // ---------------------------------------------------------------------
    // Goblin Animator
    // (tweak rows / frame ranges to match your sheet layout)
    // ---------------------------------------------------------------------
    SpriteRenderer2D::Animator goblinAnimator;
    goblinAnimator.sheet = &goblinSheet;

    // Example:
    // Row 0: idle (cols 0..5)
    // Row 1: walk (cols 0..5)
    SpriteAnimationDesc idleClip;
    idleClip.name = "Idle";
    idleClip.row = 0;
    idleClip.startCol = 0;
    idleClip.frameCount = 6;
    idleClip.frameDuration = 0.15f;
    idleClip.loop = true;

    SpriteAnimationDesc walkClip;
    walkClip.name = "Walk";
    walkClip.row = 1;
    walkClip.startCol = 0;
    walkClip.frameCount = 6;
    walkClip.frameDuration = 0.10f;
    walkClip.loop = true;

    goblinAnimator.addClip(idleClip);
    goblinAnimator.addClip(walkClip);
    goblinAnimator.play("Idle", true);

    // ---------------------------------------------------------------------
    // Main loop
    // ---------------------------------------------------------------------
    bool   running = true;
    double prevTime = GetTimeSeconds();

    while (running) {
        // Start new input frame
        Input::NewFrame();

        // Process OS events (also feeds Input::HandleEvent)
        if (platform.pollQuitRequested()) {
            running = false;
            break;
        }

        // Time step
        double now = GetTimeSeconds();
        float  dt = static_cast<float>(now - prevTime);
        if (dt < 0.0f) dt = 0.0f;
        prevTime = now;

        // -----------------------------------------------------------------
        // Detect window resize (drag edges, maximize, etc.)
        // -----------------------------------------------------------------
        int curW = winW;
        int curH = winH;
        SDL_GetWindowSizeInPixels(platform.getWindow(), &curW, &curH);
        if (curW != winW || curH != winH) {
            winW = curW;
            winH = curH;
            renderer.resizeViewport(winW, winH);
        }

        // -----------------------------------------------------------------
        // Fullscreen toggle (F11)
        // -----------------------------------------------------------------
        using HBE::Platform::Input::IsKeyPressed;
        if (IsKeyPressed(SDL_SCANCODE_F11)) {
            if (windowCfg.mode == WindowMode::Windowed) {
                windowCfg.mode = WindowMode::FullscreenDesktop;
            }
            else {
                windowCfg.mode = WindowMode::Windowed;
            }

            platform.applyGraphicsSettings(windowCfg);

            // Update viewport after mode change
            SDL_GetWindowSizeInPixels(platform.getWindow(), &winW, &winH);
            renderer.resizeViewport(winW, winH);
        }

        // -----------------------------------------------------------------
        // Goblin movement (WASD / arrows) in logical space
        // -----------------------------------------------------------------
        using HBE::Platform::Input::IsKeyDown;

        bool moveUp = IsKeyDown(SDL_SCANCODE_W) || IsKeyDown(SDL_SCANCODE_UP);
        bool moveDown = IsKeyDown(SDL_SCANCODE_S) || IsKeyDown(SDL_SCANCODE_DOWN);
        bool moveLeft = IsKeyDown(SDL_SCANCODE_A) || IsKeyDown(SDL_SCANCODE_LEFT);
        bool moveRight = IsKeyDown(SDL_SCANCODE_D) || IsKeyDown(SDL_SCANCODE_RIGHT);

        Transform2D* goblinTr = scene.getTransform(goblinEntity);
        if (goblinTr) {
            float moveX = 0.0f;
            float moveY = 0.0f;

            if (moveUp)    moveY += 1.0f;
            if (moveDown)  moveY -= 1.0f;
            if (moveLeft)  moveX -= 1.0f;
            if (moveRight) moveX += 1.0f;

            if (moveX != 0.0f || moveY != 0.0f) {
                float len = std::sqrt(moveX * moveX + moveY * moveY);
                if (len > 0.0f) {
                    moveX /= len;
                    moveY /= len;
                }
            }

            bool isMoving = (moveX != 0.0f || moveY != 0.0f);

            const float speed = 300.0f; // units per second in logical space
            goblinTr->posX += moveX * speed * dt;
            goblinTr->posY += moveY * speed * dt;

            // Choose animation
            if (isMoving) {
                goblinAnimator.play("Walk");
            }
            else {
                goblinAnimator.play("Idle");
            }

            // Update animator and apply frame
            goblinAnimator.update(dt);
            if (RenderItem* goblinItem = scene.getRenderItem(goblinEntity)) {
                goblinAnimator.apply(*goblinItem);
            }

            // Camera follow goblin in logical space
            camera.x = goblinTr->posX;
            camera.y = goblinTr->posY;
            renderer.setCamera(camera);
        }

        // -----------------------------------------------------------------
        // Rendering
        // -----------------------------------------------------------------
        renderer.beginFrame();

        renderer2D.beginScene(camera);
        scene.render(renderer2D);
        renderer2D.endScene();

        renderer.endFrame(platform);

        // Basic frame cap
        platform.delayMillis(1);
    }

    LogInfo("Exiting Sandbox.");
    return 0;
}
