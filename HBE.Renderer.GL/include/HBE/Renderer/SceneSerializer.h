#pragma once

#include <string>
#include <functional>

#include "HBE/Renderer/Scene2D.h"

namespace HBE::Renderer {

    class Mesh;
    class Material;

    struct SceneSaveCallbacks {
        // Required for saving pointer-backed components:
        std::function<std::string(const Mesh*)> meshKey;
        std::function<std::string(const Material*)> materialKey;

        // Optional:
        std::function<std::string(const SpriteRenderer2D::SpriteSheetHandle*)> sheetKey;
    };

    struct SceneLoadCallbacks {
        // Required for loading pointer-backed components:
        std::function<Mesh* (const std::string&)> mesh;
        std::function<Material* (const std::string&)> material;

        // Optional:
        std::function<const SpriteRenderer2D::SpriteSheetHandle* (const std::string&)> sheet;

        // Script binding by name (you set the onUpdate lambdas here)
        std::function<void(HBE::ECS::Entity e, const std::string& scriptName, Scene2D& scene)> bindScript;

        // Animator preset builder by name (rebuild clips/states/transitions)
        std::function<void(HBE::ECS::Entity e, const std::string& presetName, SpriteAnimationStateMachine& sm, Scene2D& scene)> buildAnimatorPreset;
    };

    class SceneSerializer {
    public:
        // Saves current scene state. tilemapPath is stored at scene root for your loader/editor.
        static bool saveToFile(Scene2D& scene,
            const std::string& path,
            const SceneSaveCallbacks& cb,
            const std::string& tilemapPath,
            std::string* outError = nullptr);

        // Loads and rebuilds the scene. If outTilemapPath != nullptr, you can rebuild tilemap renderer after load.
        static bool loadFromFile(Scene2D& scene,
            const std::string& path,
            const SceneLoadCallbacks& cb,
            std::string* outTilemapPath = nullptr,
            std::string* outError = nullptr);
    };

}