#pragma once

#include <string>

#include "HBE/Renderer/RenderItem.h"

namespace HBE::Renderer {

	class Texture2D;
	class ResourceCache;

	// Describes how frames are laid out on a sprite sheet texture.
	struct SpriteSheet {
		Texture2D* texture = nullptr;

		int texWidth = 0;
		int texHeight = 0;

		int frameWidth = 0;
		int frameHeight = 0;

		// optional margins/spacing in pixels for more complex sheets
		int marginX = 0;
		int marginY = 0;
		int spacingX = 0;
		int spacingY = 0;

		bool isValid() const { return texture != nullptr; }
	};

	// static ish helper; doesn't own anything, ujust cooperates with ResourceCache.
	class SpriteRenderer2D {
	public:
		// Load a sprite sheet texture via ResourceCache and describe its layout.
		//
		// name = cache key (e.g. "hero_idle")
		// path = file path to PNG
		// farame Width/ frameHeight = size of each frame in pixels
		// imageW / imageH are optional: if 0, they're taken from the actual texture
		static SpriteSheet DeclareSpriteSheet(
			ResourceCache& cache,
			const std::string& name,
			const std::string& path,
			int frameWidth,
			int frameHeight,
			int marginX = 0,
			int marginY = 0,
			int spacingX = 0,
			int spacingY = 0
		);

		// set a static frame (no animation) on a RenderItem
		// col / row are zero-based from indices within the sprite sheet grid.
		static void SetStaticSpriteFrame(
			RenderItem& item,
			const SpriteSheet& sheet,
			int col,
			int row
		);
	};

	// very small animation helper
	// - works over a single row in a spritesheet
	// - plays frames from colStart.. colEnd at given FPS
	class SpriteAnimation {
	public:
		SpriteAnimation() = default;

		SpriteAnimation(
			const SpriteSheet* sheet,
			int colStart, // includsive
			int colEnd, // inclusive
			int row,
			float framesPerSecond,
			bool loop = true
		);

		// start playing: if restart is true, rewind to first frame.
		void play(bool restartIfPlaying = false);
		void stop();

		void update(float dt);

		// writes UVs for the current frame into the RenderItem's uvRect.
		void apply(RenderItem& item) const;
		
		bool isPlaying() const { return m_playing; }

	private:
		const SpriteSheet* m_sheet = nullptr;

		int m_row = 0;
		int m_colStart = 0;
		int m_colEnd = 0;
		float m_fps = 0.0f;
		bool m_loop = true;

		float m_time = 0.0f;
		int m_currentCol = 0;
		bool m_playing = false;
	};
}