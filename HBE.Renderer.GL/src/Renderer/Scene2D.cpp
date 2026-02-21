#include "HBE/Renderer/Scene2D.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Core/Log.h"
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/Mesh.h"

#include "HBE/ECS/Components.h"
#include "HBE/ECS/RuntimeComponents.h"

#include "HBE/Renderer/TileMap.h"
#include "HBE/Renderer/TileCollision.h"

#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>

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

    void Scene2D::setTileCollisionContext(const TileMap* map, const TileMapLayer* collisionLayer) {
        m_tileMap = map;
        m_collisionLayer = collisionLayer;
    }

    void Scene2D::removeEntity(EntityID id) {
        m_reg.destroy(id);
    }

    static inline float applyDamping(float v, float damping, float dt) {
        if (damping <= 0.0f) return v;
        // Exponential-ish decay that is stable for large dt
        const float k = 1.0f / (1.0f + damping * dt);
        return v * k;
    }

    void Scene2D::update(float dt, const SpriteAnimationStateMachine::EventCallback& onAnimEvent) {
        // -----------------------------
        // 1) Script system
        // -----------------------------
        for (auto e : m_reg.view<HBE::ECS::Script>()) {
            auto& sc = m_reg.get<HBE::ECS::Script>(e);

            // One-time create
            if (!m_reg.has<HBE::ECS::ScriptRuntimeState>(e)) {
                auto& rt = m_reg.emplace<HBE::ECS::ScriptRuntimeState>(e);
                rt.created = false;
            }

            auto& rt = m_reg.get<HBE::ECS::ScriptRuntimeState>(e);
            if (!rt.created) {
                rt.created = true;
                if (sc.onCreate) sc.onCreate(e);
            }

            if (sc.onUpdate) sc.onUpdate(e, dt);
        }

        // -----------------------------
        // 2) Physics + tile collision system
        // -----------------------------
        const bool canTileCollide = (m_tileMap != nullptr && m_collisionLayer != nullptr);

        for (auto e : m_reg.view<Transform2D, HBE::ECS::RigidBody2D>()) {
            auto& tr = m_reg.get<Transform2D>(e);
            auto& rb = m_reg.get<HBE::ECS::RigidBody2D>(e);

            if (rb.isStatic) continue;

            // integrate acceleration -> velocity
            rb.velX += rb.accelX * dt;
            rb.velY += rb.accelY * dt;

            rb.velX = applyDamping(rb.velX, rb.linearDamping, dt);
            rb.velY = applyDamping(rb.velY, rb.linearDamping, dt);

            const bool hasCollider = m_reg.has<HBE::ECS::Collider2D>(e);

            if (canTileCollide && hasCollider) {
                auto& col = m_reg.get<HBE::ECS::Collider2D>(e);

                // build center-based AABB in world space
                HBE::Renderer::AABB box;
                box.w = col.halfW * 2.0f;
                box.h = col.halfH * 2.0f;
                box.cx = tr.posX + col.offsetX;
                box.cy = tr.posY + col.offsetY;

                // Move with collision resolution (updates box + velocities)
                HBE::Renderer::TileCollision::moveAndCollide(*m_tileMap, *m_collisionLayer, box, rb.velX, rb.velY, dt);

                // write back resolved position (undo collider offset)
                tr.posX = box.cx - col.offsetX;
                tr.posY = box.cy - col.offsetY;
            }
            else {
                // simple Euler integrate
                tr.posX += rb.velX * dt;
                tr.posY += rb.velY * dt;
            }
        }

        // -----------------------------
        // 2.5) Entity-vs-Entity collision (AABB vs AABB)
        // Dynamic colliders push out of static colliders.
        // -----------------------------
        struct WorldAABB {
            float cx, cy; // center
            float hx, hy; // half extents
        };

        auto makeAABB = [&](HBE::ECS::Entity e) -> WorldAABB {
            const auto& tr = m_reg.get<Transform2D>(e);
            const auto& col = m_reg.get<HBE::ECS::Collider2D>(e);

            WorldAABB a;
            a.cx = tr.posX + col.offsetX;
            a.cy = tr.posY + col.offsetY;
            a.hx = col.halfW;
            a.hy = col.halfH;
            return a;
            };

        auto overlap = [&](const WorldAABB& a, const WorldAABB& b, float& outDx, float& outDy) -> bool {
            const float dx = b.cx - a.cx;
            const float px = (a.hx + b.hx) - std::fabs(dx);
            if (px <= 0.0f) return false;

            const float dy = b.cy - a.cy;
            const float py = (a.hy + b.hy) - std::fabs(dy);
            if (py <= 0.0f) return false;

            // Choose minimum penetration axis
            if (px < py) {
                // If b is to the right of a (dx > 0), push a left (negative)
                // If b is to the left of a (dx < 0), push a right (positive)
                outDx = (dx < 0.0f) ? +px : -px;
                outDy = 0.0f;
            }
            else {
                // If b is above a (dy > 0), push a down (negative)
                // If b is below a (dy < 0), push a up (positive)
                outDx = 0.0f;
                outDy = (dy < 0.0f) ? +py : -py;
            }
            return true;
            };

        // Build a list of static colliders
        std::vector<HBE::ECS::Entity> statics;
        statics.reserve(128);

        for (auto e : m_reg.view<Transform2D, HBE::ECS::Collider2D>()) {
            bool isStatic = true;

            if (m_reg.has<HBE::ECS::RigidBody2D>(e)) {
                isStatic = m_reg.get<HBE::ECS::RigidBody2D>(e).isStatic;
            }

            if (isStatic) {
                statics.push_back(e);
            }
        }

        // Dynamic bodies collide against statics
        for (auto e : m_reg.view<Transform2D, HBE::ECS::RigidBody2D, HBE::ECS::Collider2D>()) {
            auto& tr = m_reg.get<Transform2D>(e);
            auto& rb = m_reg.get<HBE::ECS::RigidBody2D>(e);

            if (rb.isStatic) continue;

            // Iteratively resolve (a couple passes helps prevent tunneling when overlapping)
            for (int pass = 0; pass < 2; ++pass) {
                bool anyResolved = false;

                for (auto s : statics) {
                    if (s == e) continue;

                    float pushX = 0.0f, pushY = 0.0f;

                    WorldAABB a = makeAABB(e);
                    WorldAABB b = makeAABB(s);

                    if (!overlap(a, b, pushX, pushY)) continue;

                    // Apply push-out to transform (note: transform is sprite center; collider has offset)
                    tr.posX += pushX;
                    tr.posY += pushY;

                    // Kill velocity along the push axis
                    if (pushX != 0.0f) rb.velX = 0.0f;
                    if (pushY != 0.0f) rb.velY = 0.0f;

                    anyResolved = true;
                }

                if (!anyResolved) break;
            }
        }

        // -----------------------------
        // 3) Animation system (UV updates)
        // -----------------------------
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

        // -----------------------------
        // 4) Keep sortKey in sync with transform (useful for y-sorting)
        // -----------------------------
        for (auto e : m_reg.view<Transform2D, SpriteComponent2D>()) {
            auto& tr = m_reg.get<Transform2D>(e);
            auto& spr = m_reg.get<SpriteComponent2D>(e);
            spr.sortKey = tr.posY + spr.sortOffsetY;
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
            viewL = cam->x - halfW; viewR = cam->x + halfW;
            viewB = cam->y - halfH; viewT = cam->y + halfH;
            canCull = true;
        }

        // Build a list of pointers to sprites for sorting without pointer invalidation issues.
        // We sort by (layer, sortKey).
        struct DrawRef {
            EntityID e;
            int layer;
            float sortKey;
        };

        std::vector<DrawRef> drawList;
        drawList.reserve(1024);

        for (auto e : m_reg.view<Transform2D, SpriteComponent2D>()) {
            const auto& tr = m_reg.get<Transform2D>(e);
            const auto& spr = m_reg.get<SpriteComponent2D>(e);

            if (canCull) {
                // very cheap cull using transform scale as AABB size
                const float hw = 0.5f * std::fabs(tr.scaleX);
                const float hh = 0.5f * std::fabs(tr.scaleY);
                const float l = tr.posX - hw;
                const float r = tr.posX + hw;
                const float b = tr.posY - hh;
                const float t = tr.posY + hh;
                if (r < viewL || l > viewR || t < viewB || b > viewT) continue;
            }

            drawList.push_back({ e, spr.layer, spr.sortKey });
        }

        std::sort(drawList.begin(), drawList.end(), [](const DrawRef& a, const DrawRef& b) {
            if (a.layer != b.layer) return a.layer < b.layer;
            return a.sortKey < b.sortKey;
            });

        for (const auto& d : drawList) {
            auto& tr = m_reg.get<Transform2D>(d.e);
            auto& spr = m_reg.get<SpriteComponent2D>(d.e);

            RenderItem item;
            item.transform = tr;
            item.mesh = spr.mesh;
            item.material = spr.material;
            item.layer = spr.layer;
            item.sortKey = spr.sortKey;
            std::memcpy(item.uvRect, spr.uvRect, sizeof(item.uvRect));

            renderer.draw(item);
        }
    }

} // namespace HBE::Renderer