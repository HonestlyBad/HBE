#pragma once
#include <string>
#include "HBE/Renderer/TileMap.h"

namespace HBE::Renderer {

	class TileMapLoader {
	public:
		static bool loadFromJsonFile(const std::string& path, TileMap& outMap, std::string* outError = nullptr);
	};
}