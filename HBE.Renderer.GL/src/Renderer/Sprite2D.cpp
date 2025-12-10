#include "HBE/Renderer/Sprite2D.h"

#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/Texture2D.h"
#include "HBE/Core/Log.h"

namespace HBE::Renderer {
	
	using HBE::Core::LogError;

	// helper: compute uv rect for a frame at (col,row) on a sheet.
	// Remember: flip images vertically on Load (stbi_set_flip_vertically_on_Load(1)),
	// so GPU's origin is at bottom-left
	static void ComputeFrameUV(
		const SpriteSheet& sheet,
		int col,
		int row,
		float outUV[4]
	) {
		if (!sheet.isValid()) {
			outUV[0] = 0.0f; outUV[1] = 0.0f;
			outUV[2] = 1.0f; outUV[3] = 1.0f;
			return;
		}

		int fw = sheet.frameWidth;
		int fh = sheet.frameHeight;

		int x = sheet.marginX + col * (fw + sheet.spacingX);
		int yTop = sheet.marginY + row * (fh + sheet.spacingY);

		// convert top-left origin (in original image) to bottom-left-origin (GPU)
		int gpuY = sheet.texHeight - (yTop + fh);

		if (x < 0) x = 0;
		if (gpuY < 0) gpuY = 0;

		float u0 = static_cast<float>(x) / static_cast<float>(sheet.texWidth);
		float v0 = static_cast<float>(gpuY) / static_cast<float>(sheet.texHeight);

		float uScale = static_cast<float>(fw) / static_cast<float>(sheet.texWidth);
		float vScale = static_cast<float>(fh) / static_cast<float>(sheet.texHeight);

		// uvRect = (offsetX, offSetY, scaleX, scaleY)
		outUV[0] = u0;
		outUV[1] = v0;
		outUV[2] = uScale;
		outUV[3] = vScale;
	}

	// -------------------------------------------------------------------------
	// SpriteRenderer2D
	// -------------------------------------------------------------------------

	SpriteSheet SpriteRenderer2D::DeclareSpriteSheet(
		ResourceCache& cache,
		const std::string& name,
		const std::string& path,
		int frameWidth,
		int frameHeight,
		int marginX,
		int marginY,
		int spacingX,
		int spacingY
	) {
		SpriteSheet s;

		Texture2D* tex = cache.getOrCreateTextureFromFile(name, path);
		if (!tex) {
			LogError("SpriteRenderer2D::DeclareSpriteSheet: failed to load texture '" + path + "'");
			return s;
		}

		s.texture = tex;
		s.texWidth = tex->getWidth();
		s.texHeight = tex->getHeight();
		s.frameWidth = frameWidth;
		s.frameHeight = frameHeight;
		s.marginX = marginX;
		s.marginY = marginY;
		s.spacingX = spacingX;
		s.spacingY = spacingY;

		return s;
	}

	void SpriteRenderer2D::SetStaticSpriteFrame(
		RenderItem& item,
		const SpriteSheet& sheet,
		int col,
		int row
	) {
		float uv[4];
		ComputeFrameUV(sheet, col, row, uv);

		item.uvRect[0] = uv[0];
		item.uvRect[1] = uv[1];
		item.uvRect[2] = uv[2];
		item.uvRect[3] = uv[3];
	}

	// -------------------------------------------------------------------------
	// SpriteAnimation
	// -------------------------------------------------------------------------

	SpriteAnimation::SpriteAnimation(
		const SpriteSheet* sheet,
		int colStart,
		int colEnd,
		int row,
		float framesPerSecond,
		bool loop
	)
		: m_sheet(sheet)
		, m_row(row)
		, m_colStart(colStart)
		, m_colEnd(colEnd)
		, m_fps(framesPerSecond)
		, m_loop(loop)
		, m_time(0.0f)
		, m_currentCol(colStart)
		, m_playing(false) {
		if (m_colEnd < m_colStart) {
			std::swap(m_colStart, m_colEnd);
		}
	}

	void SpriteAnimation::play(bool restartIfPlaying) {
		if (!m_sheet || !m_sheet->isValid()) {
			m_playing = false;
			return;
		}

		// already playing, do nothing
		if (m_playing && !restartIfPlaying) return;

		m_playing = true;
		m_time = 0.0f;
		m_currentCol = m_colStart;
	}

	void SpriteAnimation::stop() {
		m_playing = false;
	}

	void SpriteAnimation::update(float dt) {
		if (!m_playing || !m_sheet || !m_sheet->isValid()) return;

		// no speed set, dont advance
		if (m_fps <= 0.0f) return;

		float frameDuration = 1.0f / m_fps;
		m_time += dt;

		while (m_time >= frameDuration) {
			m_time -= frameDuration;
			++m_currentCol;

			if (m_currentCol > m_colEnd) {
				if (m_loop) {
					m_currentCol = m_colStart;
				}
				else {
					m_currentCol = m_colEnd;
					m_playing = false;
					break;
				}
			}
		}
	}

	void SpriteAnimation::apply(RenderItem& item) const {
		if (!m_sheet || !m_sheet->isValid()) {
			return;
		}

		float uv[4];
		ComputeFrameUV(*m_sheet, m_currentCol, m_row, uv);

		item.uvRect[0] = uv[0];
		item.uvRect[1] = uv[1];
		item.uvRect[2] = uv[2];
		item.uvRect[3] = uv[3];
	}
}