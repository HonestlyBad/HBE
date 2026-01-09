#pragma once
#include <string>
#include <unordered_map>

#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/Color.h"
#include "HBE/Renderer/Font.h"

namespace HBE::Renderer {

    class Renderer2D;
    class ResourceCache;
    class Mesh;
    class GLShader;
    class Texture2D;

    class TextRenderer2D {
    public:

        enum class TextAlignH { Left, Center, Right };
        enum class TextAlignV {Baseline, Top, Middle, Bottom};
        
        struct TextLayout {
            float width = 0.0f;
            float height = 0.0f;
            int lineCount = 1;
        };

        // Measure without drawing.
        // if maxWidth > 0, it wraps.
        TextLayout measureText(const std::string& text, float scale = 1.0f, float maxWidth = 0.0f) const;

        // drawText with alignment and optional wrapping.
        // x,y refer to an anchor point based on alignH/alignV
        void drawTextAligned(Renderer2D& r2d,
            float x, float y,
            const std::string& text,
            float scale,
            Color4 tint,
            TextAlignH alignH,
            TextAlignV alignV,
            float maxWidth = 0.0f,
            float lineSpacingMult = 1.25f);

        bool initialize(ResourceCache& cache, GLShader* spriteShader, Mesh* quadMesh);

        // Load a custom TTF/OTF and store it by name
        bool loadFont(ResourceCache& cache,
            const std::string& fontName,
            const std::string& ttfPath,
            float pixelHeight,
            int atlasW = 512,
            int atlasH = 512);

        void setActiveFont(const std::string& fontName);

        // x,y are screen-space pixels (origin bottom-left).
        // y is treated as the BASELINE for proportional fonts.
        void drawText(Renderer2D& r2d,
            float x, float y,
            const std::string& text,
            float scale = 1.0f,
            Color4 tint = { 1,1,1,1 });

    private:
        // Fallback debug atlas font (your original)
        Texture2D* m_debugFontTex = nullptr;
        int m_dbgTexW = 0, m_dbgTexH = 0;
        int m_dbgGlyphW = 8, m_dbgGlyphH = 8;
        int m_dbgCols = 16;

        bool buildDebugAtlas(ResourceCache& cache);
        void dbgUvForChar(unsigned char c, float outUVRect[4]) const;

        // low-level draw (NO alignment/wrap, NO recursion)
        void drawTextRaw(Renderer2D& r2d,
            float x, float y,
            const std::string& text,
            float scale,
            Color4 tint);

        // Real fonts
        std::unordered_map<std::string, Font> m_fonts;
        Font* m_activeFont = nullptr;

        Material m_mat{};
        Mesh* m_quad = nullptr;
    };

} // namespace HBE::Renderer
