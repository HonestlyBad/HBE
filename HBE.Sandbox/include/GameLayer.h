#pragma once

#include "HBE/Core/Layer.h"

#include "HBE/Renderer/Camera2D.h"
#include "HBE/Renderer/Scene2D.h"
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/SpriteRenderer2D.h"
#include "HBE/Renderer/DebugDraw2D.h"
#include "HBE/Renderer/TileMap.h"
#include "HBE/Renderer/TileMapRenderer.h"
#include "HBE/Renderer/TileMapLoader.h"
#include "HBE/Renderer/TileCollision.h"
#include "HBE/Renderer/TextRenderer2D.h"
#include "HBE/Renderer/UI/UIContext.h"

#include <vector>
#include <string>

namespace HBE::Core { class Application; }

class GameLayer final : public HBE::Core::Layer {
public:
	void onAttach(HBE::Core::Application& app) override;
	void onUpdate(float dt) override;
	void onRender() override;
	HBE::Renderer::DebugDraw2D m_debug{};
	HBE::Renderer::TextRenderer2D m_text{};

	// --- stats (FPS/UPS) ---
	double m_statTimer = 0.0;  // seconds accumulated
	int    m_frameCount = 0;   // frames rendered in window
	int    m_updateCount = 0;  // updates in window
	float  m_fps = 0.0f;
	float  m_ups = 0.0f;

	float m_uiAnimT = 0.0f;

	bool onEvent(HBE::Core::Event& e);
	bool m_debugDraw = true;
	bool m_uiDebugDraw = false;
	bool m_uiGodMode = false;

private:
	HBE::Renderer::UI::UIContext m_ui{};

	struct DebugPopup {
		std::string text;
		float x = 0.0f;
		float y = 0.0f;
		HBE::Renderer::Color4 color{ 1,1,1,1 };

		float life = 0.0f; // seconds remaining
		float maxLife = 0.0f; // seconds total
		float floatSpeed = 0.0f;
	};

	std::vector<DebugPopup> m_popups;
	void spawnPopup(float x, float y, const std::string& text, const HBE::Renderer::Color4& color, float lifetimeSeconds = 1.0f, float floatUpSpeed = 30.0f);

	HBE::Core::Application* m_app = nullptr;

	// Logical area
	static constexpr float LOGICAL_WIDTH = 1280.0f;
	static constexpr float LOGICAL_HEIGHT = 720.0f;

	// renderer side
	HBE::Renderer::Camera2D m_camera{};
	HBE::Renderer::Scene2D m_scene{};

	// tilemap
	HBE::Renderer::TileMap m_tileMap{};
	HBE::Renderer::TileMapRenderer m_tileRenderer{};
	const HBE::Renderer::TileMapLayer* m_collisionLayer = nullptr;

	// player collision box (center-based)
	HBE::Renderer::AABB m_playerBox{};
	float m_velX = 0.0f;
	float m_velY = 0.0f;

	// resources
	HBE::Renderer::GLShader* m_spriteShader = nullptr;
	HBE::Renderer::Mesh* m_quadMesh = nullptr;

	HBE::Renderer::Material m_goblinMaterial{};
	HBE::Renderer::SpriteRenderer2D::SpriteSheetHandle m_goblinSheet{};
	HBE::Renderer::SpriteRenderer2D::Animator m_goblinAnimator{};

	HBE::Renderer::EntityID m_goblinEntity = HBE::Renderer::InvalidEntityID;

	void buildSpritePipeline(); // shader + quad mesh + texture + entity
};