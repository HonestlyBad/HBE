# MegaX — shoot-em-up built on HBE
add_executable(MegaX
    src/main.cpp
    src/Game/Bullet.cpp
    src/Game/Effects.cpp
    src/Game/Enemy.cpp
    src/Game/EnemyBullet.cpp
    src/Game/EnemyManager.cpp
    src/Game/GameLayer.cpp
    src/Game/PerfCapture.cpp
    src/Game/Player.cpp
    src/World/World.cpp
)

target_include_directories(MegaX PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

option(MEGAX_PROFILE_IN_RELEASE
    "Keep MegaX's HBE_PROFILE_SCOPE timers alive in Release builds" OFF)

if(MEGAX_PROFILE_IN_RELEASE)
    target_compile_definitions(MegaX PRIVATE HBE_PROFILE_ENABLED=1)
endif()

target_link_libraries(MegaX PRIVATE
    HBE.Core
    HBE.Platform.SDL
    HBE.Renderer.GL
    hbe::sdl3
    hbe::opengl
)

if(TARGET hbe::sdl3_mixer)
    target_link_libraries(MegaX PRIVATE hbe::sdl3_mixer)
endif()

hbe_copy_runtime_deps(
    TARGET     MegaX
    ASSETS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets
)

set_target_properties(MegaX PROPERTIES FOLDER "Apps")

if(WIN32)
    set_target_properties(MegaX PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:MegaX>"
    )
endif()
