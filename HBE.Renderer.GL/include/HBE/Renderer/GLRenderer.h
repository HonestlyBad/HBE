#pragma once

#include <array>

namespace HBE::Platform {
    class SDLPlatform;
}

namespace HBE::Renderer {

    class GLShader;
    struct Transform2D;
    struct RenderItem;
    struct Camera2D;

    class GLRenderer {
    public:
        GLRenderer() = default;
        ~GLRenderer() = default;

        GLRenderer(const GLRenderer&) = delete;
        GLRenderer& operator=(const GLRenderer&) = delete;

        GLShader* getBasicShader() const { return nullptr; } // or remove if unused

        // Must be called after SDLPlatform has created a GL context.
        bool initialize(HBE::Platform::SDLPlatform& platform);

        void beginFrame();
        void endFrame(HBE::Platform::SDLPlatform& platform);

        void setClearColor(float r, float g, float b, float a = 1.0f);

        // Set the active 2D camera (used when drawing RenderItems).
        void setCamera(const Camera2D& cam);

        // Generic draw for 2D items (quad, sprites, etc.).
        void draw(const RenderItem& item);

        // NEW: update GL viewport when the window size changes
        void resizeViewport(int width, int height);

    private:
        bool m_initialized = false;
        std::array<float, 4> m_clearColor{ 0.1f, 0.2f, 0.35f, 1.0f };

        // Non-owning pointer to the current camera (owned by sandbox / game).
        Camera2D* m_camera = nullptr;

        void buildTransformMatrix(const Transform2D& t, float out[16]);
        void buildViewMatrix(float out[16]) const;
        void buildOrthoProjection(float out[16]) const;
        void multiplyMat4(const float* a, const float* b, float* out) const;
    };

} // namespace HBE::Renderer
