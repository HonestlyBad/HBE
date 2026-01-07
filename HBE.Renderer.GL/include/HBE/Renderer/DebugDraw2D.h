#pragma once

#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/Color.h"

namespace HBE::Renderer {

    class Renderer2D;
    class ResourceCache;
    class Mesh;
    class GLShader;


    class DebugDraw2D {
    public:
        bool initialize(ResourceCache& cache, Mesh* quadMesh);

        // Draw a rectangle centered at (cx, cy)
        void rect(Renderer2D& r2d,
            float cx, float cy,
            float w, float h,
            float r, float g, float b, float a,
            bool filled);


    private:
        Mesh* m_quadMesh = nullptr;
        GLShader* m_shader = nullptr;
        Material m_mat{};
    };

} // namespace HBE::Renderer
