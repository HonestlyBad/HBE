#include "HBE/Renderer/TileCollision.h"
#include <cmath>
#include <algorithm>

namespace HBE::Renderer {

    static void getExtents(const AABB& b, float& minX, float& minY, float& maxX, float& maxY) {
        const float hw = b.w * 0.5f;
        const float hh = b.h * 0.5f;
        minX = b.cx - hw;
        maxX = b.cx + hw;
        minY = b.cy - hh;
        maxY = b.cy + hh;
    }

    static bool overlapsSolidLike(const TileMap& map, const TileMapLayer& layer, const AABB& box) {
        float minX, minY, maxX, maxY;
        getExtents(box, minX, minY, maxX, maxY);

        const float tw = map.worldTileW();
        const float th = map.worldTileH();

        const int minTX = (int)std::floor(minX / tw);
        const int maxTX = (int)std::floor((maxX - 0.001f) / tw);
        const int minTY = (int)std::floor(minY / th);
        const int maxTY = (int)std::floor((maxY - 0.001f) / th);

        for (int ty = minTY; ty <= maxTY; ++ty) {
            for (int tx = minTX; tx <= maxTX; ++tx) {
                const int tileId = layer.at(tx, ty);
                if (tileId == 0) continue;

                const auto& ts = map.tilesets[layer.tilesetIndex];
                if (ts.isSolid(tileId) || ts.slopeType(tileId) != SlopeType::None) {
                    return true;
                }
            }
        }
        return false;
    }

    bool TileCollision::isSolidTile(const TileMap& map, const TileMapLayer& layer, int tx, int ty) {
        const int tileId = layer.at(tx, ty);
        if (tileId == 0) return false;

        const auto& ts = map.tilesets[layer.tilesetIndex];
        // Fully-solid only (not one-way, not slope)
        return ts.isSolid(tileId);
    }

    bool TileCollision::isOneWayTile(const TileMap& map, const TileMapLayer& layer, int tx, int ty) {
        const int tileId = layer.at(tx, ty);
        if (tileId == 0) return false;
        const auto& ts = map.tilesets[layer.tilesetIndex];
        return ts.isOneWay(tileId);
    }

    SlopeType TileCollision::slopeTypeAt(const TileMap& map, const TileMapLayer& layer, int tx, int ty) {
        const int tileId = layer.at(tx, ty);
        if (tileId == 0) return SlopeType::None;
        const auto& ts = map.tilesets[layer.tilesetIndex];
        return ts.slopeType(tileId);
    }

    static void resolveX(
        const TileMap& map,
        const TileMapLayer& layer,
        AABB& box,
        float& velX,
        MoveResult2D& out,
        float maxStepUp
    ) {
        if (velX == 0.0f) return;

        float minX, minY, maxX, maxY;
        getExtents(box, minX, minY, maxX, maxY);

        const float tw = map.worldTileW();
        const float th = map.worldTileH();

        const int minTY = (int)std::floor(minY / th);
        const int maxTY = (int)std::floor((maxY - 0.001f) / th);

        if (velX > 0.0f) {
            const int tx = (int)std::floor((maxX - 0.001f) / tw);

            for (int ty = minTY; ty <= maxTY; ++ty) {
                const int tileId = layer.at(tx, ty);
                if (tileId == 0) continue;

                const auto& ts = map.tilesets[layer.tilesetIndex];
                const bool blocks = ts.isSolid(tileId) || ts.slopeType(tileId) != SlopeType::None;
                if (!blocks) continue;

                // Step-up attempt: raise, then see if we can exist there without overlap
                if (maxStepUp > 0.0f) {
                    AABB stepBox = box;
                    stepBox.cy += maxStepUp;

                    if (!overlapsSolidLike(map, layer, stepBox)) {
                        box = stepBox;
                        out.steppedUp = true;
                        return;
                    }
                }

                // Clamp against the tile's left face
                box.cx = tx * tw - box.w * 0.5f;
                velX = 0.0f;
                out.hitX = true;
                return;
            }
        }
        else { // velX < 0
            const int tx = (int)std::floor(minX / tw);

            for (int ty = minTY; ty <= maxTY; ++ty) {
                const int tileId = layer.at(tx, ty);
                if (tileId == 0) continue;

                const auto& ts = map.tilesets[layer.tilesetIndex];
                const bool blocks = ts.isSolid(tileId) || ts.slopeType(tileId) != SlopeType::None;
                if (!blocks) continue;

                if (maxStepUp > 0.0f) {
                    AABB stepBox = box;
                    stepBox.cy += maxStepUp;

                    if (!overlapsSolidLike(map, layer, stepBox)) {
                        box = stepBox;
                        out.steppedUp = true;
                        return;
                    }
                }

                // Clamp against the tile's right face
                box.cx = (tx + 1) * tw + box.w * 0.5f;
                velX = 0.0f;
                out.hitX = true;
                return;
            }
        }
    }

    static void resolveY(
        const TileMap& map,
        const TileMapLayer& layer,
        AABB& box,
        float& velY,
        MoveResult2D& out,
        bool enableOneWay,
        float prevBottom
    ) {
        if (velY == 0.0f) return;

        float minX, minY, maxX, maxY;
        getExtents(box, minX, minY, maxX, maxY);

        const float tw = map.worldTileW();
        const float th = map.worldTileH();

        const int minTX = (int)std::floor(minX / tw);
        const int maxTX = (int)std::floor((maxX - 0.001f) / tw);

        if (velY > 0.0f) {
            const int ty = (int)std::floor((maxY - 0.001f) / th);

            for (int tx = minTX; tx <= maxTX; ++tx) {
                const int tileId = layer.at(tx, ty);
                if (tileId == 0) continue;

                const auto& ts = map.tilesets[layer.tilesetIndex];
                const bool blocks = ts.isSolid(tileId) || ts.slopeType(tileId) != SlopeType::None;
                if (!blocks) continue;

                // Clamp against the tile's bottom face
                box.cy = ty * th - box.h * 0.5f;
                velY = 0.0f;
                out.hitY = true;
                out.ceiling = true;
                return;
            }
        }
        else { // velY < 0  (falling)
            const int ty = (int)std::floor(minY / th);
            const float newBottom = minY;

            for (int tx = minTX; tx <= maxTX; ++tx) {
                const int tileId = layer.at(tx, ty);
                if (tileId == 0) continue;

                const auto& ts = map.tilesets[layer.tilesetIndex];

                const bool isSolid = ts.isSolid(tileId);
                const bool isSlope = ts.slopeType(tileId) != SlopeType::None;
                const bool isOneWay = enableOneWay && ts.isOneWay(tileId);

                if (!isSolid && !isSlope && !isOneWay) continue;

                // One-way: collide only if we were above the platform top last frame and we're now crossing it.
                if (isOneWay && !isSolid && !isSlope) {
                    const float platformTop = (ty + 1) * th;
                    const float eps = 0.05f;
                    const bool wasAbove = prevBottom >= platformTop - eps;
                    const bool crossedDown = newBottom <= platformTop + eps;

                    if (!(wasAbove && crossedDown)) {
                        continue;
                    }

                    // Land on top
                    box.cy = platformTop + box.h * 0.5f;
                    velY = 0.0f;
                    out.hitY = true;
                    out.grounded = true;
                    return;
                }

                // Solid (and slope tiles are treated as blocking in axis-pass; final snap happens in slope pass)
                box.cy = (ty + 1) * th + box.h * 0.5f;
                velY = 0.0f;
                out.hitY = true;
                out.grounded = true;
                return;
            }
        }
    }

    static void applySlopeSnap(
        const TileMap& map,
        const TileMapLayer& layer,
        AABB& box,
        float& velY,
        MoveResult2D& out
    ) {
        // Only snap to slopes when falling or already basically grounded
        if (velY > 0.0f) return;

        float minX, minY, maxX, maxY;
        getExtents(box, minX, minY, maxX, maxY);

        const float tw = map.worldTileW();
        const float th = map.worldTileH();

        const int minTX = (int)std::floor(minX / tw);
        const int maxTX = (int)std::floor((maxX - 0.001f) / tw);

        // Look at the tile row directly under the box bottom
        const float bottom = minY;
        const int ty = (int)std::floor((bottom - 0.001f) / th);

        float bestSurfaceY = -1e9f;
        bool found = false;

        for (int tx = minTX; tx <= maxTX; ++tx) {
            const SlopeType st = TileCollision::slopeTypeAt(map, layer, tx, ty);
            if (st == SlopeType::None) continue;

            const float tileLeft = tx * tw;
            const float tileBottom = ty * th;

            // Clamp sample X to tile bounds; use box center
            float sampleX = std::clamp(box.cx, tileLeft, tileLeft + tw);
            float localX = sampleX - tileLeft; // [0, tw]

            float height = 0.0f;
            if (st == SlopeType::LeftUp) {
                height = (localX / tw) * th;
            }
            else if (st == SlopeType::RightUp) {
                height = (1.0f - (localX / tw)) * th;
            }

            const float surfaceY = tileBottom + height;
            if (surfaceY > bestSurfaceY) {
                bestSurfaceY = surfaceY;
                found = true;
            }
        }

        if (!found) return;

        const float desiredBottom = bestSurfaceY;
        const float snap = desiredBottom - bottom;

        // Snap only upward (or tiny downward) within a reasonable range to prevent teleporting.
        const float maxSnap = th + 0.01f;
        if (snap > -0.05f && snap <= maxSnap) {
            box.cy += snap;
            velY = 0.0f;
            out.grounded = true;
            out.hitY = true;
        }
    }

    void TileCollision::moveAndCollide(
        const TileMap& map,
        const TileMapLayer& layer,
        AABB& box,
        float& velX,
        float& velY,
        float dt
    ) {
        // Compatibility: behaves like the old implementation (solid-only).
        MoveResult2D dummy{};
        const float prevBottom = (box.cy - box.h * 0.5f);
        (void)TileCollision::moveAndCollideEx(map, layer, box, velX, velY, dt, 0.0f, false, false, prevBottom);
    }

    MoveResult2D TileCollision::moveAndCollideEx(
        const TileMap& map,
        const TileMapLayer& layer,
        AABB& box,
        float& velX,
        float& velY,
        float dt,
        float maxStepUp,
        bool enableOneWay,
        bool enableSlopes,
        float oneWayPrevBottom
    ) {
        MoveResult2D res{};

        // X then Y (classic platformer)
        box.cx += velX * dt;
        resolveX(map, layer, box, velX, res, maxStepUp);

        box.cy += velY * dt;
        resolveY(map, layer, box, velY, res, enableOneWay, oneWayPrevBottom);

        if (enableSlopes) {
            applySlopeSnap(map, layer, box, velY, res);
        }

        return res;
    }

} // namespace HBE::Renderer