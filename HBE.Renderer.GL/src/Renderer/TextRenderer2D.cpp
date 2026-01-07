#include "HBE/Renderer/TextRenderer2D.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/Texture2D.h"
#include "HBE/Renderer/Mesh.h"
#include "HBE/Renderer/RenderItem.h"

#include <vector>
#include <unordered_map>
#include <array>
#include <cstdint>


namespace HBE::Renderer {

    static void putPixel(std::vector<unsigned char>& rgba, int w, int x, int y, unsigned char a) {
        int i = (y * w + x) * 4;
        rgba[i + 0] = 255;
        rgba[i + 1] = 255;
        rgba[i + 2] = 255;
        rgba[i + 3] = a;
    }

    // Minimal 8x8 bitmaps for the debug charset we need now.
    // Each row is 8 bits (1=on). Top row first.
    using Glyph8 = std::array<std::uint8_t, 8>;

    static const std::unordered_map<char, Glyph8> kDebugGlyphs = {
        {' ', Glyph8{0,0,0,0,0,0,0,0}},
        {':', Glyph8{0,0,24,24,0,24,24,0}},
        {'F', Glyph8{124,64,64,120,64,64,64,0}},
        {'P', Glyph8{120,68,68,120,64,64,64,0}},
        {'S', Glyph8{60,64,64,56,4,4,120,0}},
        {'U', Glyph8{68,68,68,68,68,68,56,0}},
        {'0', Glyph8{56,68,76,84,100,68,56,0}},
        {'1', Glyph8{16,48,16,16,16,16,56,0}},
        {'2', Glyph8{56,68,4,8,16,32,124,0}},
        {'3', Glyph8{56,68,4,24,4,68,56,0}},
        {'4', Glyph8{8,24,40,72,124,8,8,0}},
        {'5', Glyph8{124,64,120,4,4,68,56,0}},
        {'6', Glyph8{28,32,64,120,68,68,56,0}},
        {'7', Glyph8{124,4,8,16,32,32,32,0}},
        {'8', Glyph8{56,68,68,56,68,68,56,0}},
        {'9', Glyph8{56,68,68,60,4,8,48,0}},
    };


    bool TextRenderer2D::initialize(ResourceCache& cache, GLShader* spriteShader, Mesh* quadMesh) {
        if (!spriteShader || !quadMesh) return false;
        m_quad = quadMesh;

        if (!buildDebugAtlas(cache)) return false;

        m_mat.shader = spriteShader;
        m_mat.texture = m_fontTex;
        return true;
    }

    bool TextRenderer2D::buildDebugAtlas(ResourceCache& cache) {
        // 16 cols x 8 rows => supports ASCII-ish mapping, but we only fill what we need.
        m_glyphW = 8; m_glyphH = 8;
        m_cols = 16;
        const int rows = 8;
        m_texW = m_cols * m_glyphW;
        m_texH = rows * m_glyphH;

        std::vector<unsigned char> rgba(m_texW * m_texH * 4, 0);

        auto drawGlyphAt = [&](unsigned char c, const Glyph8& glyph) {

            int col = (c % m_cols);
            int row = (c / m_cols);
            int ox = col * m_glyphW;
            int oy = row * m_glyphH;

            // our glyph rows are top->bottom. texture is bottom-left UV friendly.
            for (int gy = 0; gy < 8; ++gy) {
                std::uint8_t bits = glyph[gy];
                for (int gx = 0; gx < 8; ++gx) {
                    bool on = (bits & (1 << (7 - gx))) != 0;
                    if (on) {
                        int x = ox + gx;
                        int y = (oy + (7 - gy)); // flip so glyph top ends up visually top
                        putPixel(rgba, m_texW, x, y, 255);
                    }
                }
            }
            };

        // Fill only our debug glyphs
        for (auto& kv : kDebugGlyphs) {
            char c = kv.first;
            drawGlyphAt((unsigned char)c, kv.second);
        }

        // Create texture through cache (or create directly)
        // If you don't have "create texture from rgba" in cache, just create a new Texture2D here.
        m_fontTex = cache.getOrCreateTextureFromRGBA("debug_font_atlas", m_texW, m_texH, rgba.data());
        return (m_fontTex != nullptr);
    }

    void TextRenderer2D::uvForChar(unsigned char c, float outUV[4]) const {
        int col = (c % m_cols);
        int row = (c / m_cols);

        float u0 = (float)(col * m_glyphW) / (float)m_texW;
        float v0 = (float)(row * m_glyphH) / (float)m_texH;

        float uS = (float)m_glyphW / (float)m_texW;
        float vS = (float)m_glyphH / (float)m_texH;

        outUV[0] = u0;
        outUV[1] = v0;
        outUV[2] = uS;
        outUV[3] = vS;
    }

    void TextRenderer2D::drawText(Renderer2D& r2d, float x, float y, const std::string& text, float pixelScale, Color4 /*tint*/) {
        if (!m_fontTex || !m_quad) return;

        RenderItem item{};
        item.mesh = m_quad;
        item.material = &m_mat;
        item.transform.rotation = 0.0f;

        const float gw = (float)m_glyphW * pixelScale;
        const float gh = (float)m_glyphH * pixelScale;

        item.transform.scaleX = gw;
        item.transform.scaleY = gh;

        float penX = x;
        for (char ch : text) {
            unsigned char c = (unsigned char)ch;

            // only draw chars we have; unknown -> space
            if (kDebugGlyphs.find((char)c) == kDebugGlyphs.end())
                c = (unsigned char)' ';

            uvForChar(c, item.uvRect);

            item.transform.posX = penX + gw * 0.5f;
            item.transform.posY = y + gh * 0.5f;

            r2d.draw(item);
            penX += gw;
        }
    }

} // namespace HBE::Renderer
