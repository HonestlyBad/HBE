#pragma once

#include <string>
#include <cstdint>
#include <array>

namespace HBE::Renderer {

    class ResourceCache;
    class Texture2D;

    // Font supports two atlas types:
    //  - Bitmap alpha atlas (stb_truetype packed)
    //  - Single-channel SDF atlas (baked at load time)
    // Both paths share buildQuad() so TextRenderer2D doesn't need to branch.
    class Font {
    public:
        static constexpr int FirstChar = 32;   // space
        static constexpr int LastChar = 126;  // '~'
        static constexpr int CharCount = (LastChar - FirstChar + 1);

        enum class Mode {
            Bitmap,
            SDF
        };

        Font();
        ~Font();

        Font(const Font&) = delete;
        Font& operator=(const Font&) = delete;

        Font(Font&& other) noexcept;
        Font& operator=(Font&& other) noexcept;

        // Classic bitmap alpha atlas (good for pixel fonts).
        bool loadFromTTF(
            ResourceCache& cache,
            const std::string& cacheTextureName,
            const std::string& ttfPath,
            float pixelHeight,
            int atlasW = 512,
            int atlasH = 512,
            int padding = 2
        );

        // SDF atlas (scales cleanly). The generated atlas is stored as RGBA with
        // the SDF stored in the alpha channel (0..1).
        //
        // Recommended:
        //  pixelHeight: 48-64
        //  padding: 8-12 (more padding = better at huge scales, but larger atlas)
        bool loadFromTTF_SDF(
            ResourceCache& cache,
            const std::string& cacheTextureName,
            const std::string& ttfPath,
            float pixelHeight,
            int atlasW = 1024,
            int atlasH = 1024,
            int padding = 8,
            std::uint8_t onEdgeValue = 128,
            float pixelDistScale = 64.0f
        );

        Texture2D* texture() const { return m_tex; }
        int atlasWidth()  const { return m_atlasW; }
        int atlasHeight() const { return m_atlasH; }
        float pixelHeight() const { return m_pixelHeight; }
        float lineHeight() const { return m_lineHeight; }

        Mode mode() const { return m_mode; }
        bool isSDF() const { return m_mode == Mode::SDF; }

        // penY is baseline in screen pixel space.
        // Outputs quad in pixel space + UVs (BOTTOM-LEFT origin).
        bool buildQuad(
            char c,
            float& penX, float& penY,
            float& outX0, float& outY0, float& outX1, float& outY1,
            float& outU0, float& outV0, float& outU1, float& outV1
        ) const;

    private:
        void freePacked();

        struct GlyphSDF {
            bool valid = false;
            int w = 0, h = 0;
            int xoff = 0, yoff = 0;       // offset from baseline to top-left (y down)
            float xadvance = 0.0f;        // advance in pixels
            float u0 = 0.0f, v0 = 0.0f;   // bottom-left UV
            float u1 = 0.0f, v1 = 0.0f;   // top-right UV
        };

        Mode m_mode = Mode::Bitmap;

        // Opaque packed glyph blob (actually stbtt_packedchar[CharCount]).
        // Kept opaque so Font.h doesn't require stb_truetype includes.
        void* m_packed = nullptr;

        // SDF glyph table
        std::array<GlyphSDF, CharCount> m_sdfGlyphs{};

        Texture2D* m_tex = nullptr;
        int m_atlasW = 0;
        int m_atlasH = 0;
        float m_pixelHeight = 0.0f;
        float m_lineHeight = 0.0f;
    };

} // namespace HBE::Renderer
