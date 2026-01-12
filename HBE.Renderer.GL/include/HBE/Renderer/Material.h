#pragma once

#include "HBE/Renderer/Color.h"

namespace HBE::Renderer {

    class GLShader;
    class Texture2D;

    // Holds "how to draw" info: shader, texture, tint color, etc.
    class Material {
    public:
        GLShader* shader = nullptr;
        Texture2D* texture = nullptr;

        // Basic tint color
        Color4 color{ 1.0f, 1.0f, 1.0f, 1.0f };

        // Optional: SDF font rendering controls.
        // For normal sprites/bitmaps, leave useSDF=false.
        bool useSDF = false;
        float sdfSoftness = 1.0f; // higher = softer edge; 1.0 is a good default

        // Apply this material to the GPU, given an MVP matrix
        void apply(const float* mvp) const;
    };

} // namespace HBE::Renderer
