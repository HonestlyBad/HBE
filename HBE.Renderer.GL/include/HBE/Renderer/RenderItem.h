#pragma once

#include "HBE/Renderer/Transform2D.h"

namespace HBE::Renderer {

    class Mesh;
    class Material;
    

    // One drawable thing in the scene
    struct RenderItem {
        Mesh* mesh = nullptr;
        Material* material = nullptr;
        Transform2D transform;

        // u0, v0, u1, v1  (normalized 0..1)
        float uvRect[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    };

} // namespace HBE::Renderer
