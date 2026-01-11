#include "HBE/Renderer/UI/UIContext.h"

#include <cstring>

namespace HBE::Renderer::UI{

	// FNV-1a 32-bit
	std::uint32_t UIContext::hashId(const char* s) {
		std::uint32_t h = 2166136261u;
		while (*s) {
			h ^= (unsigned char)(*s++);
			h *= 16777619u;
		}
		return h;
	}

	void UIContext::bind(HBE::Renderer::Renderer2D* r2d, HBE::Renderer::DebugDraw2D* debug, HBE::Renderer::TextRenderer2D* text) {
		m_r2d = r2d;
		m_debug = debug;
		m_text = text;
	}

	void UIContext::beginFrame(float /*dt*/) {
		m_hot = 0;

		// Consume queued edge events into per-frame flags
		m_mousePressedL = m_mousePressedL_queued;
		m_mouseReleasedL = m_mouseReleasedL_queued;

		// Clear queue for next frame
		m_mousePressedL_queued = false;
		m_mouseReleasedL_queued = false;

		m_wheelY = 0.0f;
		m_panels.clear();
	}

	void UIContext::endFrame() {
		// if primary mouse is up, release active
		if (!m_mouseDownL) {
			m_active = 0;
		}
	}

	void UIContext::onEvent(HBE::Core::Event& e) {
		using namespace HBE::Core;

		if (e.type() == EventType::MouseMoved) {
			auto& ev = static_cast<MouseMovedEvent&>(e);
			m_mouseX = ev.logicalX;
			m_mouseY = ev.logicalY;
			m_mouseInViewport = ev.inViewport;
		}
		else if (e.type() == EventType::MouseButtonPressed) {
			auto& ev = static_cast<MouseButtonPressedEvent&>(e);
			m_mouseX = ev.logicalX;
			m_mouseY = ev.logicalY;
			m_mouseInViewport = ev.inViewport;

			if (!m_mouseInViewport) return;

			if (ev.button == 1) { // Left
				m_mouseDownL = true;
				m_mousePressedL_queued = true;
			}
			else if (ev.button == 3) { m_mouseDownR = true; }
			else if (ev.button == 2) { m_mouseDownM = true; }
		}
		else if (e.type() == EventType::MouseButtonReleased) {
			auto& ev = static_cast<MouseButtonReleasedEvent&>(e);
			m_mouseX = ev.logicalX;
			m_mouseY = ev.logicalY;
			m_mouseInViewport = ev.inViewport;

			if (ev.button == 1) {
				m_mouseDownL = false;
				m_mouseReleasedL_queued = true;
			}
			else if (ev.button == 3) { m_mouseDownR = false; }
			else if (ev.button == 2) { m_mouseDownM = false; }
		}
		else if (e.type() == EventType::MouseScrolled) {
			auto& ev = static_cast<MouseScrolledEvent&>(e);
			m_mouseX = ev.logicalX;
			m_mouseY = ev.logicalY;
			m_mouseInViewport = ev.inViewport;

			if (!m_mouseInViewport) return;
			m_wheelY += ev.wheelY;
		}
	}

	void UIContext::drawFilledRect(const UIRect& r, const HBE::Renderer::Color4& c) {
		if (!m_r2d || !m_debug) return;
		const float cx = r.x + r.w * 0.5f;
		const float cy = r.y + r.h * 0.5f;
		m_debug->rect(*m_r2d, cx, cy, r.w, r.h, c.r, c.g, c.b, c.a, true);
	}

	void UIContext::drawBorderRect(const UIRect& r, const HBE::Renderer::Color4& c) {
		if (!m_r2d || !m_debug) return;

		const float t = 2.0f; // border thickness

		// top
		m_debug->rect(*m_r2d,
			r.x + r.w * 0.5f, r.y + r.h - t * 0.5f,
			r.w, t,
			c.r, c.g, c.b, c.a,
			true);

		// bottom
		m_debug->rect(*m_r2d,
			r.x + r.w * 0.5f, r.y + t * 0.5f,
			r.w, t,
			c.r, c.g, c.b, c.a,
			true);

		// left
		m_debug->rect(*m_r2d,
			r.x + t * 0.5f, r.y + r.h * 0.5f,
			t, r.h,
			c.r, c.g, c.b, c.a,
			true);

		// right
		m_debug->rect(*m_r2d,
			r.x + r.w - t * 0.5f, r.y + r.h * 0.5f,
			t, r.h,
			c.r, c.g, c.b, c.a,
			true);
	}


	bool UIContext::beginPanel(const char* id, const UIRect& rect, const char* title) {
		(void)id; // reserved (later: dragging, collapse, etc..)

		// Draw panel
		drawFilledRect(rect, m_style.panelBg);
		drawBorderRect(rect, m_style.panelBorder);

		PanelState p;
		p.outer = rect;
		p.content = rect;
		p.content.x += m_style.padding;
		p.content.y += m_style.padding;
		p.content.w -= m_style.padding * 2.0f;
		p.content.h -= m_style.padding * 2.0f;

		// top-down cursor inside content
		p.cursorX = p.content.x;
		p.cursorY = p.content.y + p.content.h; // top

		m_panels.push_back(p);

		if (title && title[0] && m_text && m_r2d) {
			// title near top-left inside panel
			float tx = p.content.x;
			float ty = (p.content.y + p.content.h) - 2.0f; // baseline ish
			m_text->drawTextAligned(*m_r2d, tx, ty, title,
				m_style.textScale, m_style.text,
				HBE::Renderer::TextRenderer2D::TextAlignH::Left, HBE::Renderer::TextRenderer2D::TextAlignV::Baseline);

			// reserve space
			p.cursorY -= (m_style.itemH * 0.8f);
			m_panels.back() = p;
		}
		return true;
	}

	void UIContext::endPanel() {
		if (!m_panels.empty()) m_panels.pop_back();
	}

	void UIContext::label(const char* text, bool muted) {
		if (m_panels.empty() || !m_text || !m_r2d) return;

		auto& p = m_panels.back();
		const float x = p.cursorX;
		const float yTop = p.cursorY;

		// draw label
		m_text->drawTextAligned(*m_r2d, x, yTop, text,
			m_style.textScale, muted ? m_style.textMuted : m_style.text,
			HBE::Renderer::TextRenderer2D::TextAlignH::Left,
			HBE::Renderer::TextRenderer2D::TextAlignV::Baseline);

		// Advance cursor
		p.cursorY -= (m_style.itemH * 0.7f);
	}

	bool UIContext::button(const char* id, const char* text) {
		if (m_panels.empty()) return false;
		auto& p = m_panels.back();

		// build rect using top-down layout
		UIRect r;
		r.x = p.cursorX;
		r.w = p.content.w;
		r.h = m_style.itemH;
		r.y = p.cursorY - r.h; // bc cursrY is top

		// advance cursor
		p.cursorY -= (r.h + m_style.spacing);

		return buttonRect(id, r, text);
	}

	bool UIContext::buttonRect(const char* id, const UIRect& rect, const char* text) {
		const std::uint32_t hid = hashId(id);
		return buttonInternal(hid, rect, text);
	}

	bool UIContext::buttonInternal(std::uint32_t id, const UIRect& rect, const char* text) {
		if (!m_r2d || !m_debug || !m_text) return false;

		// only interact if mouse is inside viewport
		const bool hovered = m_mouseInViewport && rect.contains(m_mouseX, m_mouseY);

		if (hovered) m_hot = id;

		// Primary press begins active
		if (hovered && m_mousePressedL) {
			m_active = id;
		}

		bool clicked = false;

		// release triggers click if still hovered and was active
		if (m_mouseReleasedL) {
			if (hovered && m_active == id) {
				clicked = true;
			}
		}

		// pick color by state
		HBE::Renderer::Color4 bg = m_style.btnIdle;
		if (hovered) bg = m_style.btnHover;
		if (m_active == id && isMouseDownPrimary()) bg = m_style.btnDown;

		drawFilledRect(rect, bg);
		drawBorderRect(rect, m_style.btnBorder);

		// Measure text block
		auto layout = m_text->measureText(text, m_style.textScale, 0.0f);

		// Center X (we can still use drawTextAligned for horizontal centering)
		const float cx = rect.x + rect.w * 0.5f;

		// Compute baseline Y for vertical centering.
		// drawTextAligned expects y to be a BASELINE when alignV == Baseline.
		float approxLineH = (layout.lineCount > 0) ? (layout.height / (float)layout.lineCount) : layout.height;
		float baselineFudge = approxLineH * 0.55f;  // tweak: 0.20f..0.35f depending on font

		const float baselineY = rect.y + (rect.h + layout.height) * 0.5f - baselineFudge;

		m_text->drawTextAligned(*m_r2d,
			cx, baselineY,
			text,
			m_style.textScale,
			m_style.text,
			HBE::Renderer::TextRenderer2D::TextAlignH::Center,
			HBE::Renderer::TextRenderer2D::TextAlignV::Baseline,
			0.0f,
			1.25f);



		return clicked;
	}

	void UIContext::spacing(float h) {
		if (m_panels.empty()) return;
		m_panels.back().cursorY -= h;
	}

}