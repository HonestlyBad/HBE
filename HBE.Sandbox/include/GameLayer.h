#pragma once

#include "HBE/Core/Layer.h"

#include "HBE/Renderer/Camera2D.h"
#include "HBE/Renderer/Scene2D.h"
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/SpriteRenderer2D.h"

namespace HBE::Core { class Application; }

class GameLayer final : public HBE::Core::Layer {
public:
	void onAttach(HBE::Core::Application& app) override;
	void onUpdate(float dt) override;
	void onRender() override;

	bool onEvent(HBE::Core::Event& e);

private:
	HBE::Core::Application* m_app = nullptr;

	// Logical area
	static constexpr float LOGICAL_WIDTH = 1280.0f;
	static constexpr float LOGICAL_HEIGHT = 720.0f;

	// renderer side
	HBE::Renderer::Camera2D m_camera{};
	HBE::Renderer::Scene2D m_scene{};

	// resources
	HBE::Renderer::GLShader* m_spriteShader = nullptr;
	HBE::Renderer::Mesh* m_quadMesh = nullptr;

	HBE::Renderer::Material m_goblinMaterial{};
	HBE::Renderer::SpriteRenderer2D::SpriteSheetHandle m_goblinSheet{};
	HBE::Renderer::SpriteRenderer2D::Animator m_goblinAnimator{};

	HBE::Renderer::EntityID m_goblinEntity = HBE::Renderer::InvalidEntityID;

	void buildSpritePipeline(); // shader + quad mesh + texture + entity
};