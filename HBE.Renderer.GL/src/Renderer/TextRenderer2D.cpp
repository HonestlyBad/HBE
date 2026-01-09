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

    static bool isWhitespace(char c) {
        return c == ' ' || c == '\t';
    }

    static float clamp01(float v) {
        if (v < 0.0f) return 0.0f;
        if (v > 1.0f) return 1.0f;
        return v;
    }

    // Count “glyphs” in a string (ASCII-safe; ignores '\n' by default)
    static int countGlyphsASCII(const std::string& s, bool includeNewlines) {
        int n = 0;
        for (char c : s) {
            if (!includeNewlines && c == '\n') continue;
            n++;
        }
        return n;
    }

    // Return first N glyphs of string (ASCII-safe), preserving newlines if they appear
    static std::string takeFirstGlyphsASCII(const std::string& s, int glyphCount) {
        if (glyphCount < 0) return s;
        std::string out;
        out.reserve((size_t)glyphCount);

        int n = 0;
        for (char c : s) {
            if (n >= glyphCount) break;
            out.push_back(c);
            // count everything except maybe you want to ignore '\n' for reveal;
            // for now, count it so lines reveal naturally.
            n++;
        }
        return out;
    }


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

    TextRenderer2D::TextLayout TextRenderer2D::measureText(
        const std::string& text,
        float scale,
        float maxWidth) const
    {
        TextLayout out{};
        out.lineCount = 1;

        // Use active font if available; otherwise debug font metrics.
        const bool useTTF = (m_activeFont && m_activeFont->texture());

        float lineHeight = 0.0f;
        if (useTTF) {
            lineHeight = m_activeFont->pixelHeight() * scale;
        }
        else {
            lineHeight = (float)m_dbgGlyphH * scale;
        }

        float lineSpacing = lineHeight * 1.25f;
        float curLineW = 0.0f;
        float maxLineW = 0.0f;

        auto commitLine = [&]() {
            if (curLineW > maxLineW) maxLineW = curLineW;
            curLineW = 0.0f;
            out.lineCount++;
            };

        if (!useTTF) {
            // Debug font: fixed width characters
            const float gw = (float)m_dbgGlyphW * scale;

            for (size_t i = 0; i < text.size(); ++i) {
                char ch = text[i];

                if (ch == '\n') {
                    commitLine();
                    continue;
                }

                // wrap
                if (maxWidth > 0.0f && curLineW + gw > maxWidth && curLineW > 0.0f) {
                    commitLine();
                }

                curLineW += gw;
            }

            if (curLineW > maxLineW) maxLineW = curLineW;

            out.width = maxLineW;
            out.height = (out.lineCount * lineSpacing);
            return out;
        }

        // TTF font: use buildQuad to advance pen
        float penX = 0.0f;
        float penY = 0.0f;

        float lineStartPenX = penX;

        // Wrapping strategy: word-wrap by measuring word widths.
        size_t i = 0;
        while (i < text.size()) {
            char ch = text[i];

            if (ch == '\n') {
                // commit current line
                curLineW = (penX - lineStartPenX) * scale;
                if (curLineW > maxLineW) maxLineW = curLineW;

                // new line
                out.lineCount++;
                penX = 0.0f;
                penY += m_activeFont->pixelHeight(); // advance baseline down in stb-space
                lineStartPenX = penX;
                i++;
                continue;
            }

            // If wrapping enabled, measure next "word" width.
            if (maxWidth > 0.0f && isWhitespace(ch) == false) {
                // Measure word width without committing
                float testPenX = penX;
                float testPenY = penY;

                size_t j = i;
                while (j < text.size() && text[j] != '\n' && !isWhitespace(text[j])) {
                    float x0, y0, x1, y1, u0, v0, u1, v1;
                    float px = testPenX;
                    float py = testPenY;
                    if (m_activeFont->buildQuad(text[j], px, py, x0, y0, x1, y1, u0, v0, u1, v1)) {
                        testPenX = px;
                        testPenY = py;
                    }
                    else {
                        testPenX += (m_activeFont->pixelHeight() * 0.5f);
                    }
                    j++;
                }

                float wordW = (testPenX - penX) * scale;
                float currentW = (penX - lineStartPenX) * scale;

                if (currentW > 0.0f && currentW + wordW > maxWidth) {
                    // commit line and wrap before the word
                    if (currentW > maxLineW) maxLineW = currentW;

                    out.lineCount++;
                    penX = 0.0f;
                    penY += m_activeFont->pixelHeight();
                    lineStartPenX = penX;
                    continue; // retry this word on new line (don’t advance i)
                }
            }

            // Advance by this character
            float x0, y0, x1, y1, u0, v0, u1, v1;
            float px = penX;
            float py = penY;

            if (m_activeFont->buildQuad(ch, px, py, x0, y0, x1, y1, u0, v0, u1, v1)) {
                penX = px;
                penY = py;
            }
            else {
                penX += (m_activeFont->pixelHeight() * 0.5f);
            }

            i++;
        }

        // last line
        curLineW = (penX - lineStartPenX) * scale;
        if (curLineW > maxLineW) maxLineW = curLineW;

        out.width = maxLineW;
        out.height = (out.lineCount * lineSpacing);
        return out;
    }

    void TextRenderer2D::drawTextAnimated(Renderer2D& r2d,
        float x, float y,
        const std::string& text,
        float baseScale,
        Color4 baseTint,
        TextAlignH alignH,
        TextAlignV alignV,
        float maxWidth,
        float lineSpacingMult,
        const TextAnim& anim)
    {
        // 1) Compute animated alpha
        float alpha = 1.0f;

        if (anim.fadeIn && anim.fadeInTime > 0.0f) {
            alpha *= clamp01(anim.t / anim.fadeInTime);
        }

        if (anim.fadeOut && anim.autoExpire && anim.duration > 0.0f && anim.fadeOutTime > 0.0f) {
            float outStart = anim.duration - anim.fadeOutTime;
            if (anim.t >= outStart) {
                float u = (anim.t - outStart) / anim.fadeOutTime;
                alpha *= (1.0f - clamp01(u));
            }
        }

        Color4 tint = baseTint;
        tint.a *= alpha;

        // 2) Compute animated scale (linear; can add easing later)
        float scale = baseScale;
        if (anim.startScale != anim.endScale && anim.duration > 0.0f) {
            float u = clamp01(anim.t / anim.duration);
            scale = baseScale * (anim.startScale + (anim.endScale - anim.startScale) * u);
        }

        // 3) Compute animated position offset
        float ax = x + anim.offsetX + anim.velX * anim.t;
        float ay = y + anim.offsetY + anim.velY * anim.t;

        // 4) Typewriter reveal
        std::string drawStr = text;
        if (anim.typewriter) {
            int maxGlyphs = (int)std::floor(anim.t * anim.charsPerSecond);
            if (anim.maxChars >= 0 && maxGlyphs > anim.maxChars) maxGlyphs = anim.maxChars;
            drawStr = takeFirstGlyphsASCII(text, maxGlyphs);
        }

        // 5) Draw using your alignment+wrap pipeline
        drawTextAligned(r2d,
            ax, ay,
            drawStr,
            scale,
            tint,
            alignH,
            alignV,
            maxWidth,
            lineSpacingMult);
    }

    void TextRenderer2D::drawText(Renderer2D& r2d,
        float x, float y,
        const std::string& text,
        float scale,
        Color4 tint)
    {
        drawTextAligned(r2d, x, y, text, scale, tint,
            TextAlignH::Left, TextAlignV::Baseline,
            0.0f, 1.25f);
    }


    void TextRenderer2D::drawTextAligned(Renderer2D& r2d,
        float x, float y,
        const std::string& text,
        float scale,
        Color4 tint,
        TextAlignH alignH,
        TextAlignV alignV,
        float maxWidth,
        float lineSpacingMult)
    {
        if (!m_quad) return;

        // Measure block first (for alignment)
        TextLayout layout = measureText(text, scale, maxWidth);

        float startX = x;
        float startY = y;

        // Horizontal alignment shifts X by width
        if (alignH == TextAlignH::Center) startX -= layout.width * 0.5f;
        else if (alignH == TextAlignH::Right) startX -= layout.width;

        // Vertical alignment shifts Y by height
        if (alignV == TextAlignV::Top) startY += layout.height;
        else if (alignV == TextAlignV::Middle) startY += layout.height * 0.5f;
        else if (alignV == TextAlignV::Bottom) startY += 0.0f;
        // Baseline: leave as-is

        // Now draw line by line using your existing drawText logic:
        // We’ll split into lines with wrapping if needed, and call drawText for each line.
        const bool useTTF = (m_activeFont && m_activeFont->texture());

        float lineHeight = useTTF ? (m_activeFont->pixelHeight() * scale) : ((float)m_dbgGlyphH * scale);
        float lineSpacing = lineHeight * lineSpacingMult;

        // Build wrapped lines
        std::vector<std::string> lines;
        lines.reserve(8);

        auto pushLine = [&](const std::string& s) { lines.push_back(s); };

        if (maxWidth <= 0.0f) {
            // Just split on '\n'
            std::string cur;
            for (char ch : text) {
                if (ch == '\n') { pushLine(cur); cur.clear(); }
                else cur.push_back(ch);
            }
            pushLine(cur);
        }
        else {
            // Word wrap
            std::string cur;
            std::string word;

            auto flushWord = [&]() {
                if (word.empty()) return;

                std::string test = cur;
                if (!test.empty()) test.push_back(' ');
                test += word;

                float testW = measureText(test, scale, 0.0f).width;

                if (!cur.empty() && testW > maxWidth) {
                    pushLine(cur);
                    cur = word;
                }
                else {
                    if (!cur.empty()) cur.push_back(' ');
                    cur += word;
                }
                word.clear();
                };

            for (size_t i = 0; i < text.size(); ++i) {
                char ch = text[i];

                if (ch == '\n') {
                    flushWord();
                    pushLine(cur);
                    cur.clear();
                    continue;
                }

                if (isWhitespace(ch)) {
                    flushWord();
                }
                else {
                    word.push_back(ch);
                }
            }

            flushWord();
            pushLine(cur);
        }

        // Draw each line with per-line horizontal alignment inside the block
        float lineY = startY;
        for (size_t i = 0; i < lines.size(); ++i) {
            const std::string& line = lines[i];

            // measure line width for per-line alignment inside maxWidth
            float lineW = measureText(line, scale, 0.0f).width;

            float lineX = startX;
            if (alignH == TextAlignH::Center) lineX = startX + (layout.width - lineW) * 0.5f;
            else if (alignH == TextAlignH::Right) lineX = startX + (layout.width - lineW);

            // Use your existing drawText (baseline at lineY)
            drawTextRaw(r2d, lineX, lineY, line, scale, tint);

            lineY -= lineSpacing; // next line down
        }
    }

    void TextRenderer2D::drawTextRaw(Renderer2D& r2d,
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

        m_mat.color = tint;

        float penX = x;
        float penY = y; // baseline

        // ----- TTF path -----
        if (m_activeFont && m_activeFont->texture()) {
            for (char ch : text) {
                if (ch == '\n') {
                    // newline: move down one line (you can tune this later)
                    penX = x;
                    penY -= (m_activeFont->pixelHeight() * 1.25f) * scale;
                    continue;
                }

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

                // advance pen for NEXT glyph
                penX = oldPenX + (px - oldPenX) * scale;
                penY = oldPenY + (py - oldPenY) * scale;

                // flip Y around baseline (your engine is Y-up)
                float sx0 = x + (x0 - x) * scale;
                float sx1 = x + (x1 - x) * scale;
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

        // ----- Debug bitmap fallback (fixed 8x8) -----
        const float gw = (float)m_dbgGlyphW * scale;
        const float gh = (float)m_dbgGlyphH * scale;

        item.transform.scaleX = gw;
        item.transform.scaleY = gh;

        for (char ch : text) {
            if (ch == '\n') {
                penX = x;
                penY -= gh * 1.25f;
                continue;
            }

            unsigned char c = (unsigned char)ch;
            if (kDebugGlyphs.find((char)c) == kDebugGlyphs.end())
                c = (unsigned char)' ';

            dbgUvForChar(c, item.uvRect);

            item.transform.posX = penX + gw * 0.5f;
            item.transform.posY = penY + gh * 0.5f;

            r2d.draw(item);
            penX += gw;
        }
    }



} // namespace HBE::Renderer
