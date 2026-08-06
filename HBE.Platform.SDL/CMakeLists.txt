# HBE.Platform.SDL — windowing, input, audio (SDL3 backend)
add_library(HBE.Platform.SDL STATIC
    src/Platform/Audio.cpp
    src/Platform/GraphicsSettings.cpp
    src/Platform/Input.cpp
    src/Platform/SDLPlatform.cpp
)

target_include_directories(HBE.Platform.SDL
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        # Sibling engine headers.
        ${CMAKE_SOURCE_DIR}/HBE.Core/include
)

target_link_libraries(HBE.Platform.SDL
    PUBLIC
        hbe::sdl3
    PRIVATE
        hbe::nlohmann_json
        hbe::glm
)

if(TARGET hbe::sdl3_mixer)
    target_link_libraries(HBE.Platform.SDL PUBLIC hbe::sdl3_mixer)
endif()

set_target_properties(HBE.Platform.SDL PROPERTIES FOLDER "Engine")
