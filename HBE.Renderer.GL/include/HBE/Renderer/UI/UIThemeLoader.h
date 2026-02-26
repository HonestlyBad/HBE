#pragma once
#pragma once

#include <string>
#include "HBE/Renderer/UI/UIStyle.h"

namespace HBE::Renderer::UI {

	// Loads a UIStyle from JSON. Intended for quick iteration (hot reload).
	class UIThemeLoader {
	public:
		static bool loadStyleFromJsonFile(const std::string& path, UIStyle& outStyle, std::string* outError = nullptr);
	};

} // namespace HBE::Renderer::UI