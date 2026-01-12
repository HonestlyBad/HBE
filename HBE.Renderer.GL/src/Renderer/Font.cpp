#include "HBE/Renderer/Font.h"

#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/Texture2D.h"
#include "HBE/Core/Log.h"

#include <fstream>
#include <vector>
#include <cstdint>
#include <string>
#include <utility>
#include <new>      // std::nothrow
#include <cstring>  // std::memset
#include <algorithm>

// stb implementation in exactly ONE .cpp
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

namespace HBE::Renderer {

    using HBE::Core::LogInfo;
    using HBE::Core::LogError;

    static bool readFileBytes(const std::string& path, std::vector<std::uint8_t>& out) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;

        f.seekg(0, std::ios::end);
        std::streamsize sz = f.tellg();
        f.seekg(0, std::ios::beg);
        if (sz <= 0) return false;

        out.resize((size_t)sz);
        if (!f.read((char*)out.data(), sz)) return false;
        return true;
    }

    Font::Font() = default;

    Font::~Font() {
        freePacked();
    }

    Font::Font(Font&& other) noexcept {
        *this = std::move(other);
    }

    Font& Font::operator=(Font&& other) noexcept {
        if (this == &other) return *this;

        freePacked();

        m_mode = other.m_mode;
        m_packed = other.m_packed;
        other.m_packed = nullptr;

        m_sdfGlyphs = other.m_sdfGlyphs;

        m_tex = other.m_tex;
        other.m_tex = nullptr;

        m_atlasW = other.m_atlasW;
        m_atlasH = other.m_atlasH;
        m_pixelHeight = other.m_pixelHeight;
        m_lineHeight = other.m_lineHeight;

        return *this;
    }

    void Font::freePacked() {
        if (m_packed) {
            delete[](stbtt_packedchar*)m_packed;
            m_packed = nullptr;
        }
    }

    bool Font::loadFromTTF(ResourceCache& cache,
        const std::string& cacheTextureName,
        const std::string& ttfPath,
        float pixelHeight,
        int atlasW,
        int atlasH,
        int padding)
    {
        m_mode = Mode::Bitmap;
        m_tex = nullptr;
        m_atlasW = atlasW;
        m_atlasH = atlasH;
        m_pixelHeight = pixelHeight;
        m_lineHeight = pixelHeight * 1.25f; // reasonable default for bitmap path

        std::vector<std::uint8_t> ttfBytes;
        if (!readFileBytes(ttfPath, ttfBytes)) {
            LogError(std::string("Font: Failed to read TTF: ") + ttfPath);
            return false;
        }

        // Allocate packed glyph array
        freePacked();
        stbtt_packedchar* packed = new (std::nothrow) stbtt_packedchar[CharCount];
        if (!packed) {
            LogError("Font: Out of memory allocating packed glyphs");
            return false;
        }
        std::memset(packed, 0, sizeof(stbtt_packedchar) * CharCount);
        m_packed = packed;

        // Alpha atlas
        std::vector<std::uint8_t> alpha((size_t)atlasW * (size_t)atlasH, 0);

        stbtt_pack_context pc{};
        if (!stbtt_PackBegin(&pc, alpha.data(), atlasW, atlasH, atlasW, padding, nullptr)) {
            LogError("Font: stbtt_PackBegin failed");
            freePacked();
            return false;
        }

        stbtt_PackSetOversampling(&pc, 2, 2);

        if (!stbtt_PackFontRange(&pc,
            ttfBytes.data(),
            0,
            pixelHeight,
            FirstChar,
            CharCount,
            packed))
        {
            stbtt_PackEnd(&pc);
            LogError("Font: stbtt_PackFontRange failed (atlas too small?)");
            freePacked();
            return false;
        }

        stbtt_PackEnd(&pc);

        // Convert alpha -> RGBA
        std::vector<std::uint8_t> rgba((size_t)atlasW * (size_t)atlasH * 4, 0);
        for (int y = 0; y < atlasH; ++y) {
            for (int x = 0; x < atlasW; ++x) {
                std::uint8_t a = alpha[(size_t)y * (size_t)atlasW + (size_t)x];
                size_t idx = ((size_t)y * (size_t)atlasW + (size_t)x) * 4;
                rgba[idx + 0] = 255;
                rgba[idx + 1] = 255;
                rgba[idx + 2] = 255;
                rgba[idx + 3] = a;
            }
        }

        m_tex = cache.getOrCreateTextureFromRGBA(cacheTextureName, atlasW, atlasH, rgba.data());
        if (!m_tex) {
            LogError("Font: Failed to create atlas texture");
            freePacked();
            return false;
        }

        // Pixel-fonts usually want nearest; keep default filtering.

        LogInfo(std::string("Font(Bitmap): Loaded '") + ttfPath + "' => '" + cacheTextureName + "'");
        return true;
    }

    bool Font::loadFromTTF_SDF(ResourceCache& cache,
        const std::string& cacheTextureName,
        const std::string& ttfPath,
        float pixelHeight,
        int atlasW,
        int atlasH,
        int padding,
        std::uint8_t onEdgeValue,
        float pixelDistScale)
    {
        m_mode = Mode::SDF;
        m_tex = nullptr;
        m_atlasW = atlasW;
        m_atlasH = atlasH;
        m_pixelHeight = pixelHeight;
        m_lineHeight = pixelHeight * 1.25f;

        // Clear SDF glyphs
        for (auto& g : m_sdfGlyphs) g = GlyphSDF{};

        // We don't need packed glyphs in SDF mode
        freePacked();

        std::vector<std::uint8_t> ttfBytes;
        if (!readFileBytes(ttfPath, ttfBytes)) {
            LogError(std::string("Font(SDF): Failed to read TTF: ") + ttfPath);
            return false;
        }

        stbtt_fontinfo font{};
        if (!stbtt_InitFont(&font, ttfBytes.data(), stbtt_GetFontOffsetForIndex(ttfBytes.data(), 0))) {
            LogError("Font(SDF): stbtt_InitFont failed");
            return false;
        }

        const float scale = stbtt_ScaleForPixelHeight(&font, pixelHeight);

        // Better line height based on font metrics
        int ascent = 0, descent = 0, lineGap = 0;
        stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
        m_lineHeight = (float)(ascent - descent + lineGap) * scale;

        // We'll build a single alpha atlas (SDF values) then expand to RGBA.
        std::vector<std::uint8_t> alpha((size_t)atlasW * (size_t)atlasH, 0);

        int penX = padding;
        int penY = padding;
        int rowH = 0;

        auto placeGlyph = [&](int gw, int gh) -> bool {
            if (penX + gw + padding > atlasW) {
                // new row
                penX = padding;
                penY += rowH + padding;
                rowH = 0;
            }
            if (penY + gh + padding > atlasH) {
                return false;
            }
            rowH = std::max(rowH, gh);
            return true;
            };

        for (int c = FirstChar; c <= LastChar; ++c) {
            int gw = 0, gh = 0;
            int xoff = 0, yoff = 0;

            unsigned char* sdf = stbtt_GetCodepointSDF(
                &font,
                scale,
                c,
                padding,
                onEdgeValue,
                pixelDistScale,
                &gw, &gh,
                &xoff, &yoff
            );

            // Some fonts may not have certain glyphs; skip gracefully.
            if (!sdf || gw <= 0 || gh <= 0) {
                if (sdf) stbtt_FreeSDF(sdf, nullptr);
                continue;
            }

            if (!placeGlyph(gw, gh)) {
                stbtt_FreeSDF(sdf, nullptr);
                LogError("Font(SDF): Atlas too small - increase atlasW/atlasH.");
                return false;
            }

            const int dstX = penX;
            const int dstY = penY;

            // Copy bitmap into atlas
            for (int y = 0; y < gh; ++y) {
                std::uint8_t* dstRow = &alpha[(size_t)(dstY + y) * (size_t)atlasW + (size_t)dstX];
                const unsigned char* srcRow = &sdf[(size_t)y * (size_t)gw];
                std::memcpy(dstRow, srcRow, (size_t)gw);
            }

            // Metrics
            int advW = 0, lsb = 0;
            stbtt_GetCodepointHMetrics(&font, c, &advW, &lsb);
            const float advancePx = (float)advW * scale;

            GlyphSDF& g = m_sdfGlyphs[(size_t)(c - FirstChar)];
            g.valid = true;
            g.w = gw; g.h = gh;
            g.xoff = xoff;
            g.yoff = yoff;
            g.xadvance = advancePx;

            // UVs
            const float u0 = (float)dstX / (float)atlasW;
            const float v0 = (float)dstY / (float)atlasH;
            const float u1 = (float)(dstX + gw) / (float)atlasW;
            const float v1 = (float)(dstY + gh) / (float)atlasH;

            g.u0 = u0; g.v0 = v0;
            g.u1 = u1; g.v1 = v1;

            // advance atlas cursor
            penX += gw + padding;

            stbtt_FreeSDF(sdf, nullptr);
        }

        // Expand alpha to RGBA (SDF in A)
        std::vector<std::uint8_t> rgba((size_t)atlasW * (size_t)atlasH * 4, 0);
        for (int y = 0; y < atlasH; ++y) {
            for (int x = 0; x < atlasW; ++x) {
                std::uint8_t a = alpha[(size_t)y * (size_t)atlasW + (size_t)x];
                size_t idx = ((size_t)y * (size_t)atlasW + (size_t)x) * 4;
                rgba[idx + 0] = 255;
                rgba[idx + 1] = 255;
                rgba[idx + 2] = 255;
                rgba[idx + 3] = a;
            }
        }

        m_tex = cache.getOrCreateTextureFromRGBA(cacheTextureName, atlasW, atlasH, rgba.data());
        if (!m_tex) {
            LogError("Font(SDF): Failed to create atlas texture");
            return false;
        }

        // SDF needs linear filtering
        m_tex->setFiltering(true);

        LogInfo(std::string("Font(SDF): Loaded '") + ttfPath + "' => '" + cacheTextureName + "'");
        return true;
    }

    bool Font::buildQuad(char c,
        float& penX, float& penY,
        float& outX0, float& outY0, float& outX1, float& outY1,
        float& outU0, float& outV0, float& outU1, float& outV1) const
    {
        const int code = (unsigned char)c;
        if (code < FirstChar || code > LastChar) return false;

        if (m_mode == Mode::Bitmap) {
            if (!m_packed) return false;

            auto* packed = (stbtt_packedchar*)m_packed;

            stbtt_aligned_quad q{};
            stbtt_GetPackedQuad(
                packed,
                m_atlasW, m_atlasH,
                code - FirstChar,
                &penX, &penY,
                &q,
                1 // align to integer pixels
            );

            // Pixel quad
            outX0 = q.x0; outY0 = q.y0;
            outX1 = q.x1; outY1 = q.y1;

            // UV quad
            outU0 = q.s0;
            outU1 = q.s1;
            outV0 = q.t0;
            outV1 = q.t1;

            return true;
        }

        // SDF mode
        const GlyphSDF& g = m_sdfGlyphs[(size_t)(code - FirstChar)];
        if (!g.valid) return false;

        const float x0 = penX + (float)g.xoff;
        const float y0 = penY + (float)g.yoff;
        const float x1 = x0 + (float)g.w;
        const float y1 = y0 + (float)g.h;

        outX0 = x0; outY0 = y0;
        outX1 = x1; outY1 = y1;

        outU0 = g.u0;
        outU1 = g.u1;
        outV0 = g.v0;
        outV1 = g.v1;

        // advance for next glyph
        penX += g.xadvance;
        return true;
    }

} // namespace HBE::Renderer
