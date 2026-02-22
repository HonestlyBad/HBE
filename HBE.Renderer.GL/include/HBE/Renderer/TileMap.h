#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace HBE::Renderer {

    enum class SlopeType : uint8_t {
        None = 0,
        // rises left -> right ( / )
        LeftUp = 1,
        // rises right -> left ( \ )
        RightUp = 2,
    };

    struct TileMapTileset {
        std::string name;
        std::string texturePath;

        int tileW = 16;
        int tileH = 16;
        int margin = 0;
        int spacing = 0;

        // Tile IDs that are solid (1-based IDs)
        std::unordered_set<int> solidTiles;
        std::unordered_set<int> oneWayTiles;
        std::unordered_map<int, SlopeType> slopes;

        bool isSolid(int tileId) const { return solidTiles.count(tileId) > 0; }
        bool isOneWay(int tileId) const { return oneWayTiles.count(tileId) > 0; }

        SlopeType slopeType(int tileId) const {
            auto it = slopes.find(tileId);
            return (it == slopes.end()) ? SlopeType::None : it->second;
        }
    };

    struct TileMapLayer {
        std::string name;
        int w = 0;
        int h = 0;
        int tilesetIndex = 0;

        // row-major, bottom-left origin
        // 0 = empty, 1+ = tiles
        std::vector<int> data;

        int at(int x, int y) const {
            if (x < 0 || y < 0 || x >= w || y >= h) return 0;
            return data[y * w + x];
        }
    };

    struct TileMap {
        int version = 1;

        int tileSizeW = 16;
        int tileSizeH = 16;

        // Scale applied to tiles in world space
        float tilePixelScale = 1.0f;

        // World-space tile size
        float worldTileW() const { return tileSizeW * tilePixelScale; }
        float worldTileH() const { return tileSizeH * tilePixelScale; }

        std::vector<TileMapTileset> tilesets;
        std::vector<TileMapLayer> layers;

        const TileMapLayer* findLayer(const std::string& layerName) const {
            for (auto& l : layers)
                if (l.name == layerName) return &l;
            return nullptr;
        }
    };

}