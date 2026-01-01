#pragma once

#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Material.h"

namespace HBE::Renderer {

    class Renderer2D;
    class ResourceCache;
    class Mesh;
    class GLShader;

    struct Color4;

    class DebugDraw2D {
    public:
        bool initialize(ResourceCache& cache, Mesh* quadMesh);

        // Draw a rectangle centered at (cx, cy)
        void rect(Renderer2D& r2d, float cx, float cy, float w, float h, const Color4& color, bool filled = false);

    private:
        Mesh* m_quadMesh = nullptr;
        GLShader* m_shader = nullptr;
        Material m_mat{};
    };

} // namespace HBE::Renderer
