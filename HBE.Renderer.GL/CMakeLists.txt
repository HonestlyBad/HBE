# HBE.Renderer.GL — OpenGL 3.3 2D renderer, ECS render components, UI
add_library(HBE.Renderer.GL STATIC
    src/Renderer/AnimationPresetRegistry.cpp
    src/Renderer/CameraController.cpp
    src/Renderer/DebugDraw2D.cpp
    src/Renderer/Font.cpp
    src/Renderer/Framebuffer.cpp
    src/Renderer/GLRenderer.cpp
    src/Renderer/GLShader.cpp
    src/Renderer/Material.cpp
    src/Renderer/Mesh.cpp
    src/Renderer/ParticleEmitter.cpp
    src/Renderer/ParticleSystem.cpp
    src/Renderer/PostProcessStack.cpp
    src/Renderer/PrefabLibrary.cpp
    src/Renderer/Renderer2D.cpp
    src/Renderer/ResourceCache.cpp
    src/Renderer/Scene2D.cpp
    src/Renderer/SceneSerializer.cpp
    src/Renderer/ScriptRegistry.cpp
    src/Renderer/Sprite2D.cpp
    src/Renderer/SpriteAnimationStateMachine.cpp
    src/Renderer/SpriteBatch2D.cpp
    src/Renderer/SpriteRenderer2D.cpp
    src/Renderer/TextRenderer2D.cpp
    src/Renderer/Texture2D.cpp
    src/Renderer/TileCollision.cpp
    src/Renderer/TileMapLoader.cpp
    src/Renderer/TileMapRenderer.cpp
    src/Renderer/UI/UIContext.cpp
    src/Renderer/UI/UIThemeLoader.cpp
)

target_include_directories(HBE.Renderer.GL
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_SOURCE_DIR}/HBE.Core/include
        ${CMAKE_SOURCE_DIR}/HBE.Platform.SDL/include
)

target_link_libraries(HBE.Renderer.GL
    PUBLIC
        hbe::glad
        hbe::glm
        hbe::opengl
    PRIVATE
        HBE.Core
        hbe::nlohmann_json
        hbe::stb
        hbe::sdl3
)

set_target_properties(HBE.Renderer.GL PROPERTIES FOLDER "Engine")
