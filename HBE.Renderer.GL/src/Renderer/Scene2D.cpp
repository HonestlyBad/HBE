#include "HBE/Renderer/Scene2D.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Core/Log.h"
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/Mesh.h"

#include <cmath>
#include <cstring>

namespace HBE::Renderer {

    using HBE::Core::LogError;

    EntityID Scene2D::createEntity(const RenderItem& templateItem) {
        EntityID e = m_reg.create();

        // Transform component
        m_reg.emplace<Transform2D>(e, templateItem.transform);

        // Sprite component
        SpriteComponent2D sprite;
        sprite.mesh = templateItem.mesh;
        sprite.material = templateItem.material;
        sprite.layer = templateItem.layer;
        sprite.sortKey = templateItem.sortKey;
        
        // Copy UVs, but guard against "all zeros" (invisible) defaults.
        const float u0 = templateItem.uvRect[0];
        const float v0 = templateItem.uvRect[1];
        const float u1 = templateItem.uvRect[2];
        const float v1 = templateItem.uvRect[3];

        const bool allZero = (u0 == 0.0f && v0 == 0.0f && u1 == 0.0f && v1 == 0.0f);

        if (allZero) {
            // Default to full texture until an animation applies proper atlas UVs.
            sprite.uvRect[0] = 0.0f; sprite.uvRect[1] = 0.0f;
            sprite.uvRect[2] = 1.0f; sprite.uvRect[3] = 1.0f;
        }
        else {
            sprite.uvRect[0] = u0; sprite.uvRect[1] = v0;
            sprite.uvRect[2] = u1; sprite.uvRect[3] = v1;
        }

        m_reg.emplace<SpriteComponent2D>(e, sprite);

        return e;
    }

    Transform2D* Scene2D::getTransform(EntityID id) {
        if (!m_reg.valid(id)) return nullptr;
        if (!m_reg.has<Transform2D>(id)) return nullptr;
        return &m_reg.get<Transform2D>(id);
    }

    SpriteAnimationStateMachine* Scene2D::addSpriteAnimator(EntityID id, const SpriteRenderer2D::SpriteSheetHandle* sheet) {
        if (!m_reg.valid(id)) return nullptr;

        auto& anim = m_reg.emplace<AnimationComponent2D>(id);
        anim.sm.sheet = sheet;

        // Force an immediate UV apply so the sprite is visible on the first frame.
        if (m_reg.has<SpriteComponent2D>(id)) {
            auto& spr = m_reg.get<SpriteComponent2D>(id);

            RenderItem tmp;
            tmp.mesh = spr.mesh;
            tmp.material = spr.material;
            tmp.layer = spr.layer;
            tmp.sortKey = spr.sortKey;
            std::memcpy(tmp.uvRect, spr.uvRect, sizeof(tmp.uvRect));

            // Update with dt=0 to latch first frame, then apply.
            anim.sm.update(0.0f, {});
            anim.sm.apply(tmp);

            std::memcpy(spr.uvRect, tmp.uvRect, sizeof(spr.uvRect));
        }

        return &anim.sm;
    }

    SpriteAnimationStateMachine* Scene2D::getSpriteAnimator(EntityID id) {
        if (!m_reg.valid(id)) return nullptr;
        if (!m_reg.has<AnimationComponent2D>(id)) return nullptr;
        return &m_reg.get<AnimationComponent2D>(id).sm;
    }

    void Scene2D::removeEntity(EntityID id) {
        m_reg.destroy(id);
    }

    void Scene2D::update(float dt, const SpriteAnimationStateMachine::EventCallback& onAnimEvent) {
        // Entities that have animation + sprite
        for (auto e : m_reg.view<AnimationComponent2D, SpriteComponent2D>()) {
            auto& anim = m_reg.get<AnimationComponent2D>(e).sm;
            auto& spr = m_reg.get<SpriteComponent2D>(e);

            anim.update(dt, onAnimEvent);

            // Apply animation to UVs.
            // SpriteAnimationStateMachine::apply currently targets RenderItem, so we adapt:
            RenderItem tmp;
            tmp.mesh = spr.mesh;
            tmp.material = spr.material;
            tmp.layer = spr.layer;
            tmp.sortKey = spr.sortKey;
            std::memcpy(tmp.uvRect, spr.uvRect, sizeof(tmp.uvRect));

            anim.apply(tmp);

            std::memcpy(spr.uvRect, tmp.uvRect, sizeof(spr.uvRect));
        }
    }

    void Scene2D::render(Renderer2D& renderer) {
        const Camera2D* cam = renderer.activeCamera();

        // Camera view rect in world space (no per-entity pad here)
        float viewL = -1e9f, viewR = 1e9f, viewB = -1e9f, viewT = 1e9f;
        bool canCull = false;

        if (cam) {
            const float zoom = (cam->zoom > 0.0001f) ? cam->zoom : 0.0001f;
            const float halfW = 0.5f * cam->viewportWidth / zoom;
            const float halfH = 0.5f * cam->viewportHeight / zoom;

            viewL = cam->x - halfW;
            viewR = cam->x + halfW;
            viewB = cam->y - halfH;
            viewT = cam->y + halfH;

            canCull = true;
        }

        for (auto e : m_reg.view<Transform2D, SpriteComponent2D>()) {
            auto& tr = m_reg.get<Transform2D>(e);
            auto& spr = m_reg.get<SpriteComponent2D>(e);

            if (!spr.mesh || !spr.material || !spr.material->shader) {
                LogError("Scene2D: render failed for an entity (missing mesh / material / shader)");
                continue;
            }

            // ---- CULLING ----
            if (canCull) {
                const float cx = tr.posX;
                const float cy = tr.posY;

                const float hw = std::fabs(tr.scaleX) * 0.5f;
                const float hh = std::fabs(tr.scaleY) * 0.5f;

                // Per-entity cull padding (tweak 0.25f)
                const float pad = 1.0f * std::max(std::fabs(tr.scaleX), std::fabs(tr.scaleY));

                const float aL = cx - hw;
                const float aR = cx + hw;
                const float aB = cy - hh;
                const float aT = cy + hh;

                if (aR < (viewL - pad) || aL >(viewR + pad) ||
                    aT < (viewB - pad) || aB >(viewT + pad)) {
                    continue;
                }
            }

            // UVRect must be {u0, v0, uScale, vScale}. If scale is zero, it will vanish in batching.
            if (spr.uvRect[2] <= 0.0f || spr.uvRect[3] <= 0.0f) {
                spr.uvRect[0] = 0.0f; spr.uvRect[1] = 0.0f;
                spr.uvRect[2] = 1.0f; spr.uvRect[3] = 1.0f;
            }

            RenderItem item;
            item.mesh = spr.mesh;
            item.material = spr.material;
            item.transform = tr;
            item.layer = spr.layer;
            item.sortKey = spr.sortKey;
            std::memcpy(item.uvRect, spr.uvRect, sizeof(item.uvRect));

            renderer.draw(item);
        }
    }

} // namespace HBE::Renderer