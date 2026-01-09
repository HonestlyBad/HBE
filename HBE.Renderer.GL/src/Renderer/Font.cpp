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

        m_packed = other.m_packed;
        other.m_packed = nullptr;

        m_tex = other.m_tex;
        other.m_tex = nullptr;

        m_atlasW = other.m_atlasW;
        m_atlasH = other.m_atlasH;
        m_pixelHeight = other.m_pixelHeight;

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
        m_tex = nullptr;
        m_atlasW = atlasW;
        m_atlasH = atlasH;
        m_pixelHeight = pixelHeight;

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

        LogInfo(std::string("Font: Loaded '") + ttfPath + "' => '" + cacheTextureName + "'");
        return true;
    }

    bool Font::buildQuad(char c,
        float& penX, float& penY,
        float& outX0, float& outY0, float& outX1, float& outY1,
        float& outU0, float& outV0, float& outU1, float& outV1) const
    {
        const int code = (unsigned char)c;
        if (code < FirstChar || code > LastChar) return false;
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

        // UV quad: stb's t is top->bottom, OpenGL expects bottom-left, so flip V
        outU0 = q.s0;
        outU1 = q.s1;
        outV0 = q.t0;
        outV1 = q.t1;

        return true;
    }

} // namespace HBE::Renderer
