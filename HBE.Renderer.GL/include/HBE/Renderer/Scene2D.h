#pragma once

#include <cstdint>
#include <functional>

#include "HBE/ECS/Registry.h"
#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Transform2D.h"
#include "HBE/Renderer/SpriteAnimationStateMachine.h"
#include "HBE/Renderer/ECSComponents2D.h"

namespace HBE::Renderer {

    class Renderer2D;

    // simple opaque handle to an entity in the scene
    using EntityID = HBE::ECS::Entity;
    inline constexpr EntityID InvalidEntityID = HBE::ECS::Null;

    class Scene2D {
    public:
        Scene2D() = default;

        // Create an entity by copying a template RenderItem
        EntityID createEntity(const RenderItem& templateItem);

        // Access
        Transform2D* getTransform(EntityID id);

        // Sprite animation access (optional per entity)
        // If you call this, the entity will be updated automatically by Scene2D::update().
        SpriteAnimationStateMachine* addSpriteAnimator(EntityID id, const SpriteRenderer2D::SpriteSheetHandle* sheet);
        SpriteAnimationStateMachine* getSpriteAnimator(EntityID id);

        // remove
        void removeEntity(EntityID id);

        // update animations (call once per frame in your layer)
        void update(float dt, const SpriteAnimationStateMachine::EventCallback& onAnimEvent = {});

        // render all active entities
        void render(Renderer2D& renderer);

        // Expose registry if you want to go full ECS later (optional but useful)
        HBE::ECS::Registry& registry() { return m_reg; }
        const HBE::ECS::Registry& registry() const { return m_reg; }

    private:
        HBE::ECS::Registry m_reg;
    };

} // namespace HBE::Renderer