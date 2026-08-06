# HBE.Sandbox — engine showcase / demo game
add_executable(HBE.Sandbox
    src/DevConsole.cpp
    src/GameLayer.cpp
    src/main.cpp
    src/ParticleEffects.cpp
)

target_include_directories(HBE.Sandbox PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(HBE.Sandbox PRIVATE
    HBE.Core
    HBE.Platform.SDL
    HBE.Renderer.GL
    hbe::sdl3
    hbe::opengl
)

if(TARGET hbe::sdl3_mixer)
    target_link_libraries(HBE.Sandbox PRIVATE hbe::sdl3_mixer)
endif()

# Copy SDL DLLs + assets next to the exe (parity with the vcxproj post-build).
hbe_copy_runtime_deps(
    TARGET     HBE.Sandbox
    ASSETS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets
)

set_target_properties(HBE.Sandbox PROPERTIES FOLDER "Apps")

if(WIN32)
    set_target_properties(HBE.Sandbox PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:HBE.Sandbox>"
    )
endif()
