#include "HBE/Renderer/Scene2D.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Core/Log.h"
#include "HBE/Renderer/Material.h"

namespace HBE::Renderer {

	using HBE::Core::LogError;
	
	Scene2D::EntityRecord* Scene2D::findRecord(EntityID id) {
		if (id == InvalidEntityID) return nullptr;
		// simple index-based lookup: id = index + 1
		std::size_t index = static_cast<std::size_t>(id - 1);
		if (index >= m_entities.size()) return nullptr;
		EntityRecord& rec = m_entities[index];
		if (!rec.active || rec.id != id) return nullptr;
		return &rec;
	}

	const Scene2D::EntityRecord* Scene2D::findRecord(EntityID id) const {
		if (id == InvalidEntityID) return nullptr;
		std::size_t index = static_cast<std::size_t>(id - 1);
		if (index >= m_entities.size()) return nullptr;
		const EntityRecord& rec = m_entities[index];
		if (!rec.active || rec.id != id) return nullptr;
		return &rec;
	}

	EntityID Scene2D::createEntity(const RenderItem& templateItem) {
		EntityRecord rec;
		rec.id = m_nextID++;
		rec.item = templateItem;
		rec.active = true;
		m_entities.push_back(rec);

		return rec.id;
	}

	RenderItem* Scene2D::getRenderItem(EntityID id) {
		EntityRecord* rec = findRecord(id);
		return rec ? &rec->item : nullptr;
	}

	Transform2D* Scene2D::getTransform(EntityID id) {
		EntityRecord* rec = findRecord(id);
		return rec ? &rec->item.transform : nullptr;
	}

	void Scene2D::removeEntity(EntityID id) {
		EntityRecord* rec = findRecord(id);
		if (rec) {
			rec->active = false;
		}
	}

	void Scene2D::render(Renderer2D& renderer) {
		for (auto& rec : m_entities) {
			if (!rec.active) continue;

			// old (now wrong):
			// if (!rec.item.mesh || !rec.item.shader) { ... }

			if (!rec.item.mesh || !rec.item.material || !rec.item.material->shader) {
				LogError("Scene2D: render failed for an entity (missing mesh / material / shader)");
				continue;
			}

			renderer.draw(rec.item);
		}
	}

}