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

		// Slider
		HBE::Renderer::Color4 sliderTrack{ 0.12f, 0.12f, 0.16f, 0.95f };
		HBE::Renderer::Color4 sliderFill{ 0.25f, 0.55f, 0.95f, 0.95f };
		HBE::Renderer::Color4 sliderKnob{ 0.90f, 0.90f, 0.90f, 1.00f };
		HBE::Renderer::Color4 sliderKnobHover{ 1.00f, 1.00f, 1.00f, 1.00f };

		// Layout
		float padding = 10.0f;
		float itemH = 32.0f;
		float spacing = 8.0f;
		float baselineFudgeMul = 0.55f;

		// Text scale
		float textScale = 1.0f;
	};
}