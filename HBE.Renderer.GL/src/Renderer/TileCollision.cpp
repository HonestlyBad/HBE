#include "HBE/Renderer/TileCollision.h"
#include <cmath>

namespace HBE::Renderer {

    static void getExtents(const AABB& b, float& minX, float& minY, float& maxX, float& maxY) {
        float hw = b.w * 0.5f;
        float hh = b.h * 0.5f;
        minX = b.cx - hw;
        maxX = b.cx + hw;
        minY = b.cy - hh;
        maxY = b.cy + hh;
    }

    bool TileCollision::isSolidTile(const TileMap& map, const TileMapLayer& layer, int tx, int ty) {
        int tileId = layer.at(tx, ty);
        if (tileId == 0) return false;

        const auto& ts = map.tilesets[layer.tilesetIndex];
        return ts.solidTiles.count(tileId) > 0;
    }

    static void resolveX(const TileMap& map, const TileMapLayer& layer, AABB& box, float& velX) {
        float minX, minY, maxX, maxY;
        getExtents(box, minX, minY, maxX, maxY);

        const float tw = map.worldTileW();
        const float th = map.worldTileH();

        int minTX = (int)std::floor(minX / tw);
        int maxTX = (int)std::floor((maxX - 0.001f) / tw);
        int minTY = (int)std::floor(minY / th);
        int maxTY = (int)std::floor((maxY - 0.001f) / th);

        if (velX > 0) {
            for (int ty = minTY; ty <= maxTY; ++ty) {
                if (TileCollision::isSolidTile(map, layer, maxTX, ty)) {
                    box.cx = maxTX * tw - box.w * 0.5f;
                    velX = 0;
                    return;
                }
            }
        }
        else if (velX < 0) {
            for (int ty = minTY; ty <= maxTY; ++ty) {
                if (TileCollision::isSolidTile(map, layer, minTX, ty)) {
                    box.cx = (minTX + 1) * tw + box.w * 0.5f;
                    velX = 0;
                    return;
                }
            }
        }
    }

    static void resolveY(const TileMap& map, const TileMapLayer& layer, AABB& box, float& velY) {
        float minX, minY, maxX, maxY;
        getExtents(box, minX, minY, maxX, maxY);

        const float tw = map.worldTileW();
        const float th = map.worldTileH();

        int minTX = (int)std::floor(minX / tw);
        int maxTX = (int)std::floor((maxX - 0.001f) / tw);
        int minTY = (int)std::floor(minY / th);
        int maxTY = (int)std::floor((maxY - 0.001f) / th);

        if (velY > 0) {
            for (int tx = minTX; tx <= maxTX; ++tx) {
                if (TileCollision::isSolidTile(map, layer, tx, maxTY)) {
                    box.cy = maxTY * th - box.h * 0.5f;
                    velY = 0;
                    return;
                }
            }
        }
        else if (velY < 0) {
            for (int tx = minTX; tx <= maxTX; ++tx) {
                if (TileCollision::isSolidTile(map, layer, tx, minTY)) {
                    box.cy = (minTY + 1) * th + box.h * 0.5f;
                    velY = 0;
                    return;
                }
            }
        }
    }

    void TileCollision::moveAndCollide(const TileMap& map, const TileMapLayer& layer,
        AABB& box, float& velX, float& velY, float dt) {

        box.cx += velX * dt;
        resolveX(map, layer, box, velX);

        box.cy += velY * dt;
        resolveY(map, layer, box, velY);
    }

}
