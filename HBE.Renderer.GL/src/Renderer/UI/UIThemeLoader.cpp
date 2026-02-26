#include "HBE/Renderer/UI/UIThemeLoader.h"
#include "HBE/Core/Log.h"

#include <fstream>
#include <sstream>
#include <json.hpp>

using json = nlohmann::json;

namespace HBE::Renderer::UI {

	static bool readAllText(const std::string& path, std::string& out) {
		std::ifstream f(path);
		if (!f.is_open()) return false;
		std::stringstream ss;
		ss << f.rdbuf();
		out = ss.str();
		return true;
	}

	static bool tryReadColor(const json& j, const char* key, HBE::Renderer::Color4& c) {
		if (!j.contains(key)) return false;
		const auto& v = j.at(key);

		if (v.is_array() && v.size() >= 4) {
			c.r = v[0].get<float>();
			c.g = v[1].get<float>();
			c.b = v[2].get<float>();
			c.a = v[3].get<float>();
			return true;
		}
		if (v.is_object()) {
			c.r = v.value("r", c.r);
			c.g = v.value("g", c.g);
			c.b = v.value("b", c.b);
			c.a = v.value("a", c.a);
			return true;
		}
		return false;
	}

	bool UIThemeLoader::loadStyleFromJsonFile(const std::string& path, UIStyle& outStyle, std::string* outError) {
		std::string text;
		if (!readAllText(path, text)) {
			if (outError) *outError = "UIThemeLoader: could not open: " + path;
			return false;
		}

		json j;
		try { j = json::parse(text); }
		catch (...) {
			if (outError) *outError = "UIThemeLoader: invalid JSON";
			return false;
		}

		UIStyle s = outStyle; // start from current/default and override

		tryReadColor(j, "panelBg", s.panelBg);
		tryReadColor(j, "panelBorder", s.panelBorder);
		tryReadColor(j, "btnIdle", s.btnIdle);
		tryReadColor(j, "btnHover", s.btnHover);
		tryReadColor(j, "btnDown", s.btnDown);
		tryReadColor(j, "btnBorder", s.btnBorder);
		tryReadColor(j, "text", s.text);
		tryReadColor(j, "textMuted", s.textMuted);
		tryReadColor(j, "sliderTrack", s.sliderTrack);
		tryReadColor(j, "sliderFill", s.sliderFill);
		tryReadColor(j, "sliderKnob", s.sliderKnob);
		tryReadColor(j, "sliderKnobHover", s.sliderKnobHover);

		s.padding = j.value("padding", s.padding);
		s.itemH = j.value("itemH", s.itemH);
		s.spacing = j.value("spacing", s.spacing);
		s.baselineFudgeMul = j.value("baselineFudgeMul", s.baselineFudgeMul);
		s.textScale = j.value("textScale", s.textScale);

		outStyle = s;
		return true;
	}

} // namespace HBE::Renderer::UI