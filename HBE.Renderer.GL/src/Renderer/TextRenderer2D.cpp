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
#include <cmath>

namespace HBE::Renderer {

    static void putPixel(std::vector<unsigned char>& rgba, int w, int x, int y, unsigned char a) {
        int i = (y * w + x) * 4;
        rgba[i + 0] = 255;
        rgba[i + 1] = 255;
        rgba[i + 2] = 255;
        rgba[i + 3] = a;
    }

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
        m_mat.texture = m_debugFontTex;
        m_mat.color = { 1,1,1,1 };
        return true;
    }

    bool TextRenderer2D::loadFont(ResourceCache& cache,
        const std::string& fontName,
        const std::string& ttfPath,
        float pixelHeight,
        int atlasW,
        int atlasH)
    {
        Font f;
        const std::string texName = "font_" + fontName + "_" + std::to_string((int)pixelHeight);

        if (!f.loadFromTTF(cache, texName, ttfPath, pixelHeight, atlasW, atlasH)) {
            return false;
        }

        auto ins = m_fonts.emplace(fontName, std::move(f));
        // set active to the newly loaded font
        m_activeFont = &ins.first->second;

        // use its texture in our material for drawing
        m_mat.texture = m_activeFont->texture();
        return true;
    }

    void TextRenderer2D::setActiveFont(const std::string& fontName) {
        auto it = m_fonts.find(fontName);
        if (it == m_fonts.end()) {
            // fallback to debug font
            m_activeFont = nullptr;
            m_mat.texture = m_debugFontTex;
            return;
        }
        m_activeFont = &it->second;
        m_mat.texture = m_activeFont->texture();
    }

    bool TextRenderer2D::buildDebugAtlas(ResourceCache& cache) {
        m_dbgGlyphW = 8; m_dbgGlyphH = 8;
        m_dbgCols = 16;
        const int rows = 8;
        m_dbgTexW = m_dbgCols * m_dbgGlyphW;
        m_dbgTexH = rows * m_dbgGlyphH;

        std::vector<unsigned char> rgba(m_dbgTexW * m_dbgTexH * 4, 0);

        auto drawGlyphAt = [&](unsigned char c, const Glyph8& glyph) {
            int col = (c % m_dbgCols);
            int row = (c / m_dbgCols);
            int ox = col * m_dbgGlyphW;
            int oy = row * m_dbgGlyphH;

            for (int gy = 0; gy < 8; ++gy) {
                std::uint8_t bits = glyph[gy];
                for (int gx = 0; gx < 8; ++gx) {
                    bool on = (bits & (1 << (7 - gx))) != 0;
                    if (on) {
                        int x = ox + gx;
                        int y = (oy + (7 - gy));
                        putPixel(rgba, m_dbgTexW, x, y, 255);
                    }
                }
            }
            };

        for (auto& kv : kDebugGlyphs) {
            drawGlyphAt((unsigned char)kv.first, kv.second);
        }

        m_debugFontTex = cache.getOrCreateTextureFromRGBA("debug_font_atlas", m_dbgTexW, m_dbgTexH, rgba.data());
        if (!m_debugFontTex) return false;

        // pixel font should stay crisp
        m_debugFontTex->setFiltering(false);
        return true;
    }

    void TextRenderer2D::dbgUvForChar(unsigned char c, float outUVRect[4]) const {
        int col = (c % m_dbgCols);
        int row = (c / m_dbgCols);

        float u0 = (float)(col * m_dbgGlyphW) / (float)m_dbgTexW;
        float v0 = (float)(row * m_dbgGlyphH) / (float)m_dbgTexH;

        float uS = (float)m_dbgGlyphW / (float)m_dbgTexW;
        float vS = (float)m_dbgGlyphH / (float)m_dbgTexH;

        outUVRect[0] = u0;
        outUVRect[1] = v0;
        outUVRect[2] = uS;
        outUVRect[3] = vS;
    }

    void TextRenderer2D::drawText(Renderer2D& r2d,
        float x, float y,
        const std::string& text,
        float scale,
        Color4 tint)
    {
        if (!m_quad) return;

        RenderItem item{};
        item.mesh = m_quad;
        item.material = &m_mat;
        item.transform.rotation = 0.0f;

        // Tint works only if the shader supports uColor (we’ll update that next)
        m_mat.color = tint;

        float penX = x;
        float penY = y; // baseline

        if (m_activeFont && m_activeFont->texture()) {
            for (char ch : text) {
                float x0, y0, x1, y1;
                float u0, v0, u1, v1;

                float oldPenX = penX;
                float oldPenY = penY;

                float px = penX;
                float py = penY;

                if (!m_activeFont->buildQuad(ch, px, py, x0, y0, x1, y1, u0, v0, u1, v1)) {
                    penX += (m_activeFont->pixelHeight() * 0.5f) * scale;
                    continue;
                }

                penX = oldPenX + (px - oldPenX) * scale;
                penY = oldPenY + (py - oldPenY) * scale;

                float sx0 = x + (x0 - x) * scale;
                float sx1 = x + (x1 - x) * scale;

                // Flip Y around baseline y (because stb gives y-down coords)
                float sy0 = y - (y0 - y) * scale;
                float sy1 = y - (y1 - y) * scale;

                item.uvRect[0] = u0;
                item.uvRect[1] = v0;
                item.uvRect[2] = (u1 - u0);
                item.uvRect[3] = (v1 - v0);

                const float gw = (sx1 - sx0);
                const float gh = (sy1 - sy0);

                item.transform.scaleX = gw;
                item.transform.scaleY = gh;

                item.transform.posX = std::round(sx0 + gw * 0.5f);
                item.transform.posY = std::round(sy0 + gh * 0.5f);

                r2d.draw(item);
            }
            return;
        }

        

        // Fallback: debug bitmap font path (fixed 8x8)
        const float gw = (float)m_dbgGlyphW * scale;
        const float gh = (float)m_dbgGlyphH * scale;

        item.transform.scaleX = gw;
        item.transform.scaleY = gh;

        for (char ch : text) {
            unsigned char c = (unsigned char)ch;

            if (kDebugGlyphs.find((char)c) == kDebugGlyphs.end())
                c = (unsigned char)' ';

            dbgUvForChar(c, item.uvRect);

            item.transform.posX = penX + gw * 0.5f;
            item.transform.posY = y + gh * 0.5f;


            r2d.draw(item);
            penX += gw;
        }
    }

} // namespace HBE::Renderer
