#pragma once

#include <vector>
#include <cstdint>
#include <optional>

#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Transform2D.h"
#include "HBE/Renderer/SpriteAnimationStateMachine.h"

namespace HBE::Renderer {
	class Renderer2D;

	// simple opaque handle to an entity in the scene
	using EntityID = std::uint32_t;
	inline constexpr EntityID InvalidEntityID = 0;

	class Scene2D {
	public:
		Scene2D() = default;

		// Create an entity by copying a template RenderItem
		EntityID createEntity(const RenderItem& templateItem);

		// Access
		RenderItem* getRenderItem(EntityID id);
		Transform2D* getTransform(EntityID id);

		// Sprite animation access (optional per entity)
		// If you call this, the entity will be updated automatically by Scene2D::update().
		SpriteAnimationStateMachine* addSpriteAnimator(EntityID id, const SpriteRenderer2D::SpriteSheetHandle* sheet);
		SpriteAnimationStateMachine* getSpriteAnimator(EntityID id);

		// remove (soft delete for now)
		void removeEntity(EntityID id);

		// update animations (call once per frame in your layer)
		void update(float dt, const SpriteAnimationStateMachine::EventCallback& onAnimEvent = {});

		// render all active entities
		void render(Renderer2D& renderer);

	private:
		struct EntityRecord {
			EntityID id = InvalidEntityID;
			RenderItem item;
			bool active = true;

			// Optional per-entity sprite animation state machine
			std::optional<SpriteAnimationStateMachine> anim;
		};

		std::vector<EntityRecord> m_entities;
		EntityID m_nextID = 1;

		EntityRecord* findRecord(EntityID id);
		const EntityRecord* findRecord(EntityID id) const;
	};
}