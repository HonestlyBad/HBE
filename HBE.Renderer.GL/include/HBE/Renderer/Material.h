#pragma once

namespace HBE::Renderer {

    class GLShader;
    class Texture2D;

    struct Color4 {
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float a = 1.0f;
    };

    // Holds "how to draw" info: shader, texture, tint color, etc.
    class Material {
    public:
        GLShader* shader = nullptr;
        Texture2D* texture = nullptr;

        // Basic tint color
        Color4 color{};

        // Apply this material to the GPU, given an MVP matrix
        void apply(const float* mvp) const;
    };

} // namespace HBE::Renderer
