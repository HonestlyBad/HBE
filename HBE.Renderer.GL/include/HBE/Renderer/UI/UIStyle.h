#pragma once
#include "HBE/Renderer/Color.h"

namespace HBE::Renderer::UI {

	struct UIStyle {
		// Panel
		HBE::Renderer::Color4 panelBg{ 0.10f, 0.10f, 0.12f, 0.90f };
		HBE::Renderer::Color4 panelBorder{ 1.00f, 1.00f, 1.00f, 0.15f };

		// button
		HBE::Renderer::Color4 btnIdle{ 0.18f, 0.18f, 0.22f, 0.95f };
		HBE::Renderer::Color4 btnHover{ 0.25f, 0.25f, 0.32f, 0.95f };
		HBE::Renderer::Color4 btnDown{ 0.12f, 0.12f, 0.16f, 0.95f };
		HBE::Renderer::Color4 btnBorder{ 1.00f, 1.00f, 1.00f, 0.18f };

		// Text
		HBE::Renderer::Color4 text{ 1.00f, 1.00f, 1.00f, 1.00f };
		HBE::Renderer::Color4 textMuted{ 1.00f, 1.00f, 1.00f, 0.65f };

		// Layout
		float padding = 10.0f;
		float itemH = 32.0f;
		float spacing = 8.0f;

		// Text scale
		float textScale = 1.0f;
	};
}