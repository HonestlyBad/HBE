#pragma once

#include <string>
#include <cstdint>

namespace HBE::Renderer {

    class ResourceCache;
    class Texture2D;

    class Font {
    public:
        static constexpr int FirstChar = 32;   // space
        static constexpr int LastChar = 126;  // '~'
        static constexpr int CharCount = (LastChar - FirstChar + 1);

        Font();
        ~Font();

        Font(const Font&) = delete;
        Font& operator=(const Font&) = delete;

        Font(Font&& other) noexcept;
        Font& operator=(Font&& other) noexcept;

        bool loadFromTTF(
            ResourceCache& cache,
            const std::string& cacheTextureName,
            const std::string& ttfPath,
            float pixelHeight,
            int atlasW = 512,
            int atlasH = 512,
            int padding = 2
        );

        Texture2D* texture() const { return m_tex; }
        int atlasWidth()  const { return m_atlasW; }
        int atlasHeight() const { return m_atlasH; }
        float pixelHeight() const { return m_pixelHeight; }

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

        // Opaque packed glyph blob (actually stbtt_packedchar[CharCount]).
        // Kept opaque so Font.h doesn’t require stb_truetype includes.
        void* m_packed = nullptr;

        Texture2D* m_tex = nullptr;
        int m_atlasW = 0;
        int m_atlasH = 0;
        float m_pixelHeight = 0.0f;
    };

} // namespace HBE::Renderer
