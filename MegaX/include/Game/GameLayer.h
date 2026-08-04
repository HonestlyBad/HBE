#pragma once

#include "HBE/Core/Layer.h"
#include "HBE/Core/FileWatcher.h"
#include "HBE/Renderer/CameraController.h"
#include "HBE/Renderer/DebugDraw2D.h"

#include "Game/Player.h"
#include "Game/Bullet.h"
#include "Game/Effects.h"
#include "Game/EnemyManager.h"
#include "World/World.h"

namespace HBE::Core { class Application; }

namespace MegaX {

	class GameLayer : public HBE::Core::Layer {
	public:
		void onAttach(HBE::Core::Application& app) override;
		void onUpdate(float dt) override;
		void onRender() override;

		Difficulty m_difficulty = Difficulty::Difficult;

	private:
		void buildSpritePipeline();
		void drawHud(HBE::Renderer::Renderer2D& r2d);
		void spawnDemoEnemies();
		bool reloadScene(bool alsoReloadMap);
		void clearTransientEntites();
		void hotReloadShader();
		void setupHotReloadWatches();
		
		HBE::Core::Application* m_app = nullptr;
		
		HBE::Renderer::Mesh* m_quadMesh = nullptr;
		HBE::Renderer::GLShader* m_spriteShader = nullptr;

		HBE::Renderer::CameraController m_camera{};
		Player m_player{};
		World m_world{};
		BulletManager m_bullets{};
		Effects m_effects{};
		EnemyManager m_enemies{};
		const HBE::Renderer::TileMapLayer* m_ground = nullptr;

		HBE::Core::FileWatcher m_watcher{};
		HBE::Renderer::DebugDraw2D m_debug{};
		bool m_showHitBoxes = false;

		std::string m_tileMapPath = "maps/level_01.json";
		std::string m_spriteVsPath = "shaders/sprite.vert";
		std::string m_spriteFsPath = "shaders/sprite.frag";
		float m_startX = 0.0f;
		float m_startY = 0.0f;
	};
}