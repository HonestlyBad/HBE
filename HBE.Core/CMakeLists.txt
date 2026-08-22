# HBE.Core — engine framework / layer stack / ECS / event bus
add_library(HBE.Core STATIC
    src/Core/Application.cpp
    src/Core/AssetPaths.cpp
    src/Core/EventBus.cpp
    src/Core/FileWatcher.cpp
    src/Core/LayerStack.cpp
    src/Core/Log.cpp
    src/Core/Profiler.cpp
    src/Core/Time.cpp
    src/Core/Timestep.cpp
    src/Core/UUID.cpp
    src/ECS/CombatEvents.cpp
    src/ECS/CombatSystem.cpp
    src/Input/InputMap.cpp
)

target_include_directories(HBE.Core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        # Sibling engine headers — needed to compile Core sources.
        ${CMAKE_SOURCE_DIR}/HBE.Platform.SDL/include
        ${CMAKE_SOURCE_DIR}/HBE.Renderer.GL/include
)

target_link_libraries(HBE.Core
    PUBLIC
        hbe::glm
        hbe::nlohmann_json
    PRIVATE
        hbe::sdl3
)

set_target_properties(HBE.Core PROPERTIES FOLDER "Engine")
