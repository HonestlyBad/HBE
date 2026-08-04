#include "Game/GameLayer.h"
#include "Game/EnemyBullet.h"   // full type needed for m_enemies.enemyBullets()

#include "HBE/Core/Application.h"
#include "HBE/Core/AssetPaths.h"
#include "HBE/Core/Log.h"

#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/RenderPass.h"

#include "HBE/Platform/Input.h"

#include <vector>
#include <chrono>

using namespace HBE::Core;
using namespace HBE::Renderer;
namespace Input = HBE::Platform::Input;

namespace MegaX {

	static constexpr float kLogicalWidth = 1280.0f;
	static constexpr float kLogicalHeight = 720.0f;
	static constexpr float kCameraZoom = 2.0f;

	void GameLayer::onAttach(HBE::Core::Application& app) {
		m_app = &app;

        buildSpritePipeline();

		app.gl().setClearColor(0.0f, 0.0f, 0.0f, 1.0f);

		Camera2D& cam = m_camera.camera();
		cam.viewportWidth = kLogicalWidth;
		cam.viewportHeight = kLogicalHeight;
		m_camera.pixelSnap = false;
		m_camera.followResponse = 9.0f;
		m_camera.snapZoom(kCameraZoom);

        if (!m_world.load(app.renderer2D(), app.resources(),
            m_spriteShader, m_quadMesh, m_tileMapPath)) {
            LogError("MegaX GameLayer: world load failed (continuing empty.)");
        }

		if (!m_player.init(app.resources(), m_quadMesh, m_spriteShader)) {
			LogError("MegaX GameLayer: player init failed.");
		}

		// --- collision: the "Ground" layer carries the solid tiles ---
		m_ground = m_world.map().findLayer("Ground");
		if (!m_ground) {
			LogError("MegaX GameLayer: no 'Ground' layer in map — player will not collide.");
		}
		m_player.setCollision(&m_world.map(), m_ground);

		if (!m_bullets.init(app.resources(), m_quadMesh, m_spriteShader)) {
			LogError("MegaX GameLayer: bullet manager init failed.");
		}

        if (!m_effects.init(app.resources(), m_quadMesh, m_world.map(), m_ground)) {
            LogError("MegaX GameLayer: effects init failed.");
        }

        if (!m_enemies.init(app.resources(), m_quadMesh, m_spriteShader)) {
            LogError("MegaX GameLayer: enemy manager init failed.");
        }

        if (!m_debug.initialize(app.resources(), m_quadMesh)) {
            LogError("MegaX GameLayer: debug draw init failed (B overlay disabled).");
        }

        m_startX = (m_world.pixelWidth() * 0.5f) - 128.0f; // X
        m_startY = 130.0f; // Y
        m_player.setPosition(m_startX, m_startY);
        m_camera.snapTo(m_startX, m_startY);
		app.gl().setCamera(m_camera.camera());

        m_enemies.setPlayerRef(&m_player);
        m_enemies.setCollision(&m_world.map(), m_ground);
        m_enemies.setDifficulty(m_difficulty);

        spawnDemoEnemies();

		LogInfo("MegaX GameLayer attached (Play mode; press G for Ghost).");

        setupHotReloadWatches();
	}

	void GameLayer::onUpdate(float dt) {
        m_watcher.poll(dt);

		// horizontal run (both modes)
		const float ix = (Input::IsKeyDown(SDL_SCANCODE_D) ? 1.0f : 0.0f)
			- (Input::IsKeyDown(SDL_SCANCODE_A) ? 1.0f : 0.0f);

		// vertical fly intent — only used by Ghost mode; Play mode ignores iy
		const float iy = (Input::IsKeyDown(SDL_SCANCODE_SPACE) ? 1.0f : 0.0f)
			- (Input::IsKeyDown(SDL_SCANCODE_S) ? 1.0f : 0.0f);

		// platformer intents (Play mode)
		const bool jumpPressed = Input::IsKeyPressed(SDL_SCANCODE_SPACE); // one-shot
		const bool jumpHeld    = Input::IsKeyDown(SDL_SCANCODE_SPACE);    // for variable height
		const bool crouchHeld  = Input::IsKeyDown(SDL_SCANCODE_S);

		// shooting (E) — semi/auto-fire handled in Player at a cadence
		const bool firePressed = Input::IsKeyPressed(SDL_SCANCODE_E);
		const bool fireHeld    = Input::IsKeyDown(SDL_SCANCODE_E);

		if (Input::IsKeyPressed(SDL_SCANCODE_H)) {
			m_player.toggleHelmet();
		}

		// G toggles Play <-> Ghost (fly, no gravity/collision — for map building)
		if (Input::IsKeyPressed(SDL_SCANCODE_G)) {
			m_player.toggleMode();
			LogInfo(m_player.mode() == Player::Mode::Ghost
				? "MegaX: Ghost mode (fly, no collision)."
				: "MegaX: Play mode (gravity + collision).");
		}

        if(Input::IsKeyPressed(SDL_SCANCODE_B)){
            m_showHitBoxes = !m_showHitBoxes;
            LogInfo(m_showHitBoxes
                ? "MegaX: hit/hurt box overlay ON."
                : "MegaX: hit/hurt box overlay OFF.");
        }

        if (Input::IsKeyPressed(SDL_SCANCODE_F1)) {
            m_difficulty = Difficulty::Casual;
            m_enemies.setDifficulty(m_difficulty);
            m_player.refillHp();
            LogInfo("Difficulty: Casual");
        }
        if (Input::IsKeyPressed(SDL_SCANCODE_F2)) {
            m_difficulty = Difficulty::Difficult;
            m_enemies.setDifficulty(m_difficulty);
            m_player.refillHp();
            LogInfo("Difficulty: Difficult");
        }
        if (Input::IsKeyPressed(SDL_SCANCODE_F3)) {
            m_difficulty = Difficulty::Challenging;
            m_enemies.setDifficulty(m_difficulty);
            m_player.refillHp();
            LogInfo("Difficulty: Challenging");
        }
        if (Input::IsKeyPressed(SDL_SCANCODE_R)) {
            m_player.refillHp();
            LogInfo("HP refilled");
        }
        if (Input::IsKeyPressed(SDL_SCANCODE_F5)) {
            LogInfo("MegaX: scene reload requested (F5).");
            reloadScene(true);
        }
        if (Input::IsKeyPressed(SDL_SCANCODE_F6)) {
            LogInfo("MegaX: sprite shader hot reload requested (F6).");
            hotReloadShader();
        }
        if (Input::IsKeyPressed(SDL_SCANCODE_F7)) {
            LogInfo("MegaX: soft respawn requested (F7).");
            reloadScene(false);
        }

        m_world.update(dt);
		m_player.setMoveInput(ix, iy);
		m_player.setJumpInput(jumpPressed, jumpHeld);
		m_player.setCrouchInput(crouchHeld);
		m_player.setFireInput(firePressed, fireHeld);
		m_player.update(dt);

        // spawn any bullet the player fired this frame, then advance bullets
		float bx, by; int bdir;
		if (m_player.consumeShot(bx, by, bdir)) {
			m_bullets.spawn(bx, by, bdir);

            m_enemies.notifyGunshot(bx, by);

            m_effects.spawnMuzzleFlash(bx, by, bdir);
            // Casings eject from the ~ejection port near the gun body, not the
            // barrel tip: ~5 px in front of the player center, at gun height.
            const float casingX = m_player.x() + static_cast<float>(bdir) * 5.0f;
            m_effects.spawnCasing(casingX, by, bdir);
		}
		m_bullets.update(dt, &m_world.map(), m_ground, m_camera.camera());

        m_enemies.checkBulletHits(m_bullets, &m_effects, 1);

        {
            std::vector<BulletManager::Impact> impacts;
            if (m_bullets.consumeImpacts(impacts)) {
                for (const auto& imp : impacts) {
                    m_effects.spawnBulletImpact(imp.x, imp.y, imp.tileId);
                }
            }
        }

        if (m_player.landedThisFrame()) {
            m_effects.spawnLandingDust(m_player.x(), m_player.feetY(), m_player.groundTileId());
        }

        {
            const bool moving = (ix != 0.0f);
            const bool grounded = (m_player.velY() == 0.0f) && (m_player.groundTileId() != 0);
            m_effects.tickWalkDust(dt, m_player.x(), m_player.feetY(), m_player.groundTileId(), moving, grounded);
        }

		m_camera.setFollowTarget(m_player.x(), m_player.y());
		m_camera.setFollowVelocity(m_player.velX(), m_player.velY());
		m_camera.update(dt);
		m_app->gl().setCamera(m_camera.camera());

        m_enemies.update(dt);

        auto& ebm = m_enemies.enemyBullets();
        ebm.update(dt, &m_world.map(), m_ground, m_camera.camera());

        {
            const AABB pb = m_player.hurtbox();
            for (auto& b : ebm.bullets()) {
                if (!b.alive) continue;
                if (std::fabs(b.x - pb.cx) > pb.w * 0.5f) continue;
                if (std::fabs(b.y - pb.cy) > pb.h * 0.5f) continue;

                const int kbDir = (b.vx >= 0.0f) ? +1 : -1;
                if (m_player.takeDamage(b.damage, kbDir)) {
                    b.alive = false;
                }
            }
        }

        m_effects.update(dt);
	}

	void GameLayer::onRender() {
		Renderer2D& r2d = m_app->renderer2D();

		r2d.beginScene(m_camera.camera(), RenderPass::World);
        m_world.render(r2d);
        m_enemies.render(r2d);
        m_enemies.renderBubbles(m_debug, r2d);
		m_player.render(r2d);
		m_bullets.render(r2d);
        m_enemies.enemyBullets().render(r2d);

        m_effects.render(r2d);

        if (m_showHitBoxes) {
            m_enemies.debugDrawBoxes(m_debug, r2d);
            m_enemies.debugDrawSenses(m_debug, r2d);
            const HBE::Renderer::AABB pb = m_player.hurtbox();
            m_debug.rect(r2d, pb.cx, pb.cy, pb.w, pb.h, 0.35f, 0.55f, 1.0f, 1.0f, false);
            m_enemies.debugDrawBoxes(m_debug, r2d);
        }

        drawHud(r2d);

		r2d.endScene();
	}

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

    void GameLayer::drawHud(Renderer2D& r2d) {
        const Camera2D& cam = m_camera.camera();
        const float zoom = (cam.zoom <= 0.0f) ? 1.0f : cam.zoom;
        const float halfW = cam.viewportWidth / (2.0f * zoom);
        const float halfH = cam.viewportHeight / (2.0f * zoom);
        const float leftX = cam.x - halfW;
        const float rightX = cam.x + halfW;
        const float topY = cam.y + halfH;

        const int maxHp = m_player.maxHp();
        const int curHp = m_player.hp();
        const float pipSize = 12.0f;
        const float pipGap = 4.0f;
        const float pipY = topY - 20.0f;
        const float pipX0 = leftX + 20.0f;

        for (int i = 0; i < maxHp; ++i) {
            const float cx = pipX0 + (pipSize + pipGap) * i + pipSize * 0.5f;
            const bool filled = (i < curHp);
            if (filled) {
                m_debug.rect(r2d, cx, pipY, pipSize, pipSize, 0.95f, 0.15f, 0.15f, 1.0f, true);
                m_debug.rect(r2d, cx, pipY, pipSize, pipSize, 1.0f, 1.0f, 1.0f, 1.0f, false);
            }
            else {
                m_debug.rect(r2d, cx, pipY, pipSize, pipSize, 0.25f, 0.05f, 0.05f, 0.7f, true);
                m_debug.rect(r2d, cx, pipY, pipSize, pipSize, 0.8f, 0.8f, 0.8f, 1.0f, true);
            }
        }

        const DifficultyProfile& prof = m_enemies.profile();
        const float pillW = 44.0f, pillH = 12.0f;
        const float pillX = rightX - 20.0f - pillW * 0.5f;
        const float pillY = topY - 20.0f;
        m_debug.rect(r2d, pillX, pillY, pillW, pillH, prof.labelR, prof.labelG, prof.labelB, 0.95f, true);
        m_debug.rect(r2d, pillX, pillY, pillW, pillH, 1, 1, 1, 1, false);

        int tier = 2;
        if (m_difficulty == Difficulty::Casual) tier = 1;
        else if (m_difficulty == Difficulty::Challenging) tier = 3;

        const float dotSize = 4.0f;
        const float dotY = pillY;
        const float dotGap = 3.0f;
        const float rowW = tier * dotSize + (tier - 1) * dotGap;
        const float dotX0 = pillX - rowW * 0.5f + dotSize * 0.5f;
        for (int i = 0; i < tier; ++i) {
            const float dx = dotX0 + (dotSize + dotGap) * i;
            m_debug.rect(r2d, dx, dotY, dotSize, dotSize, 0.05f, 0.5f, 0.5f, 1.0f, true);
        }
    }

    bool GameLayer::reloadScene(bool alsoReloadMap) {
        if (!m_app) {
            LogError("MegaX: reloadScene called before onAttach; ignoring;");
            return false;
        }

        const auto tStart = std::chrono::steady_clock::now();
        
        const Player::Mode preservedMode = m_player.mode();
        const bool preservedHelm = m_player.hasHelmet();

        if (alsoReloadMap) {
            if (!m_world.reload(m_app->renderer2D(), m_app->resources())) {
                LogError("MegaX: scene reload FAILED -- tilemap load error (see previous log).");
                LogWarn("MegaX: Keeping previous scene state.");
                return false;
            }
            m_ground = m_world.map().findLayer("Ground");
            if (!m_ground) {
                LogError("MegaX: scene reload FAILED -- reloaded map has no 'Ground' layer.");
                LogWarn("MegaX: keeping previous scene state.");
                return false;
            }
        }

        clearTransientEntites();
        m_player.setCollision(&m_world.map(), m_ground);
        m_player.resetForRespawn();
        m_player.setPosition(m_startX, m_startY);
        m_player.setMode(preservedMode);
        m_player.setHelmet(preservedHelm);

        m_camera.snapTo(m_startX, m_startY);
        m_app->gl().setCamera(m_camera.camera());
        
        m_enemies.setPlayerRef(&m_player);
        m_enemies.setCollision(&m_world.map(), m_ground);
        m_enemies.setDifficulty(m_difficulty);

        spawnDemoEnemies();
        
        const auto tEnd = std::chrono::steady_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

        LogInfo("MegaX: scene reloaded (" + std::string(alsoReloadMap ? "map + entites" : "entites only") + ") in " + std::to_string(ms) + " ms.");
        return true;
    }

    void GameLayer::clearTransientEntites() {
        m_bullets.clear();
        m_enemies.enemyBullets().clear();
        m_enemies.clear();
        m_effects.clear();
    }

    void GameLayer::hotReloadShader() {
        if (!m_app) return;

        const bool ok = m_app->resources().reloadShader("sprite");
        if (ok) {
            LogInfo("Shader reloaded: sprite");
        }
        else {
            LogError("Shader reload FAILED: sprite (see previous log for GLSL error).");
        }
    }

    void GameLayer::setupHotReloadWatches() {
        HBE::Core::FileWatcher::Options opt{};
        opt.pollIntervalSeconds = 0.20f;
        opt.debounceSeconds = 0.25f;
        m_watcher.setOptions(opt);

        namespace ap = HBE::Core::AssetPaths;

        m_watcher.watchFile(ap::Resolve(m_tileMapPath), [this](const std::string&) {
            LogInfo("MegaX: tilemap file changed on disk -> scene reload.");
            reloadScene(true);
            });
        m_watcher.watchFile(ap::Resolve(m_spriteFsPath), [this](const std::string&) {
            LogInfo("MegaX: shader file changed on disk -> hot reload.");
            hotReloadShader();
            });
        LogInfo("MegaX GameLayer: scene watches active (" + m_tileMapPath + ", sprite shader).");
    }

    void GameLayer::spawnDemoEnemies() {
        constexpr float kTilePx = 32.0f;
        const float ex = m_startX + 5.0f * kTilePx;
        const float eGroundY = 130.0f;

        if (Enemy* e = m_enemies.spawn(ex, eGroundY, -1)) {
            e->startHp = 3;
            e->setPatrolPath(ex - 3.0f * kTilePx, ex + 3.0f * kTilePx, 1.0f);
            e->snapshotBaseStats();
            e->applyDifficulty(m_enemies.profile());
            e->spawn(ex, eGroundY, -1);
        }
    }
}