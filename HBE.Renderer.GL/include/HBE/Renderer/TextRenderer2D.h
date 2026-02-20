#pragma once
#include <string>
#include <unordered_map>
#include <deque>
#include <cstdint>

#include "HBE/Renderer/Camera2D.h"
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
        enum class TextAlignV { Baseline, Top, Middle, Bottom };

        struct TextLayout {
            float width = 0.0f;
            float height = 0.0f;
            int lineCount = 1;
        };

        struct TextAnim {
            float t = 0.0f;
            float duration = 1.0f;

            bool typewriter = false;
            float charsPerSecond = 30.0f;
            int maxChars = -1;

            bool fadeIn = false;
            float fadeInTime = 0.25f;
            bool fadeOut = false;
            float fadeOutTime = 0.25f;

            float offsetX = 0.0f;
            float offsetY = 0.0f;
            float velX = 0.0f;
            float velY = 0.0f;

            float startScale = 1.0f;
            float endScale = 1.0f;

            bool autoExpire = false;
        };

        void drawTextAnimated(Renderer2D& r2d,
            float x, float y,
            const std::string& text,
            float baseScale,
            Color4 baseTint,
            TextAlignH alignH,
            TextAlignV alignV,
            float maxWidth,
            float lineSpacingMult,
            const TextAnim& anim);

        TextLayout measureText(const std::string& text, float scale = 1.0f, float maxWidth = 0.0f) const;

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

        bool loadFont(ResourceCache& cache,
            const std::string& fontName,
            const std::string& ttfPath,
            float pixelHeight,
            int atlasW = 512,
            int atlasH = 512);

        // SDF font bake + load
        bool loadSDFont(ResourceCache& cache,
            const std::string& fontName,
            const std::string& ttfPath,
            float pixelHeight,
            int atlasW = 1024,
            int atlasH = 1024,
            int padding = 8);

        void setActiveFont(const std::string& fontName);

        void drawText(Renderer2D& r2d,
            float x, float y,
            const std::string& text,
            float scale = 1.0f,
            Color4 tint = { 1,1,1,1 });

        // Text culling (world + UI pass based on active camera)
        void setCullingEnabled(bool enabled) { m_enableCulling = enabled; }

        // Positive inset shrinks the view rect (debug), negative expands it.
        // Example: inset=200 => text must be 200 units inside the screen edges.
        void setCullInset(float inset) { m_cullInset = inset; }

        // Call once per frame before issuing any drawText* calls.
        // This clears transient per-frame material storage so each text draw can have its own tint safely.
        void beginFrame(std::uint64_t frameIndex);


    private:
        Texture2D* m_debugFontTex = nullptr;
        int m_dbgTexW = 0, m_dbgTexH = 0;
        int m_dbgGlyphW = 8, m_dbgGlyphH = 8;
        int m_dbgCols = 16;

        bool buildDebugAtlas(ResourceCache& cache);
        bool m_enableCulling = true;
        float m_cullInset = 0.0f;
        void dbgUvForChar(unsigned char c, float outUVRect[4]) const;

        void drawTextRaw(Renderer2D& r2d,
            float x, float y,
            const std::string& text,
            float scale,
            Color4 tint);

        std::unordered_map<std::string, Font> m_fonts;
        Font* m_activeFont = nullptr;

        Material m_mat{};
        Mesh* m_quad = nullptr;

        std::deque<Material> m_frameMaterials;
        std::uint64_t m_frameIndex = 0;
        bool m_frameIndexValid = false;
    };

} // namespace HBE::Renderer
