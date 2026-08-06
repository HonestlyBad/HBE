# HBMapMaker — ImGui-based tile map editor
if(NOT TARGET hbe::imgui)
    message(WARNING "HBMapMaker requires Dear ImGui in external/imgui — skipping.")
    return()
endif()

add_executable(HBMapMaker
    src/main.cpp
    src/Editor/Editor.cpp
    src/IO/SceneIO.cpp
    src/Platform/FileDialog.cpp
)

target_include_directories(HBMapMaker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(HBMapMaker PRIVATE
    HBE.Core
    HBE.Platform.SDL
    HBE.Renderer.GL
    hbe::imgui
    hbe::sdl3
    hbe::opengl
    hbe::nlohmann_json
)

if(TARGET hbe::sdl3_mixer)
    target_link_libraries(HBMapMaker PRIVATE hbe::sdl3_mixer)
endif()

hbe_copy_runtime_deps(TARGET HBMapMaker)

set_target_properties(HBMapMaker PROPERTIES FOLDER "Apps")

if(WIN32)
    set_target_properties(HBMapMaker PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:HBMapMaker>"
    )
endif()
