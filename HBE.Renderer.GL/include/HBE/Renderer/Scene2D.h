#pragma once

#include <vector>
#include <cstdint>

#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Transform2D.h"


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

		// remove (soft delete for now)
		void removeEntity(EntityID id);

		// render all active entities
		void render(Renderer2D& renderer);

	private:
		struct EntityRecord {
			EntityID id = InvalidEntityID;
			RenderItem item;
			bool active = true;
		};

		std::vector<EntityRecord> m_entities;
		EntityID m_nextID = 1;

		EntityRecord* findRecord(EntityID id);
		const EntityRecord* findRecord(EntityID id) const;
	};
}