# =============================================================================
# HBE — third-party dependency wiring
#
# This file defines the following imported / interface targets used across the
# engine and its apps:
#
#     hbe::glad          — glad OpenGL loader (STATIC)
#     hbe::glm           — glm math (INTERFACE, header-only)
#     hbe::nlohmann_json — nlohmann::json (INTERFACE, header-only)
#     hbe::stb           — stb single-header libs (INTERFACE)
#     hbe::imgui         — Dear ImGui + SDL3/OpenGL3 backends (STATIC)
#     hbe::sdl3          — SDL3 (bundled on Windows, find_package elsewhere)
#     hbe::sdl3_mixer    — SDL3_mixer
#     hbe::sdl3_ttf      — SDL3_ttf (optional; not currently linked but wired
#                                    up because the assets/fonts pipeline uses it)
#     hbe::opengl        — OpenGL (system)
# =============================================================================

include_guard(GLOBAL)

# -----------------------------------------------------------------------------
# hbe_alias_target(<alias> <target>)
#
# Like add_library(<alias> ALIAS <target>), but safe to use when <target> may
# itself already be an ALIAS target (CMake forbids aliasing an alias). Some
# distro-provided SDL3 CMake configs (e.g. Arch/CachyOS) export SDL3::SDL3 as
# an ALIAS rather than a real target, which breaks a direct ALIAS-of-ALIAS.
# This resolves the alias chain down to the real target before aliasing.
# -----------------------------------------------------------------------------
function(hbe_alias_target alias_name target_name)
    set(_resolved ${target_name})
    get_target_property(_aliased ${_resolved} ALIASED_TARGET)
    while(_aliased)
        set(_resolved ${_aliased})
        get_target_property(_aliased ${_resolved} ALIASED_TARGET)
    endwhile()
    add_library(${alias_name} ALIAS ${_resolved})
endfunction()

set(HBE_EXTERNAL_DIR ${CMAKE_SOURCE_DIR}/external)

# -----------------------------------------------------------------------------
# glad (vendored inside HBE.Renderer.GL/external/glad)
# -----------------------------------------------------------------------------
add_library(hbe_glad STATIC
    ${CMAKE_SOURCE_DIR}/HBE.Renderer.GL/external/glad/src/glad.c
)
target_include_directories(hbe_glad PUBLIC
    ${CMAKE_SOURCE_DIR}/HBE.Renderer.GL/external/glad/include
)
add_library(hbe::glad ALIAS hbe_glad)

# -----------------------------------------------------------------------------
# glm — header-only
# -----------------------------------------------------------------------------
add_library(hbe_glm INTERFACE)
target_include_directories(hbe_glm INTERFACE
    ${HBE_EXTERNAL_DIR}/glm
)
add_library(hbe::glm ALIAS hbe_glm)

# -----------------------------------------------------------------------------
# nlohmann_json — header-only
# -----------------------------------------------------------------------------
add_library(hbe_nlohmann_json INTERFACE)
target_include_directories(hbe_nlohmann_json INTERFACE
    ${HBE_EXTERNAL_DIR}/nlohmann
)
add_library(hbe::nlohmann_json ALIAS hbe_nlohmann_json)

# -----------------------------------------------------------------------------
# stb — header-only single-file libs
# -----------------------------------------------------------------------------
add_library(hbe_stb INTERFACE)
target_include_directories(hbe_stb INTERFACE
    ${HBE_EXTERNAL_DIR}/stb
)
add_library(hbe::stb ALIAS hbe_stb)

# -----------------------------------------------------------------------------
# Dear ImGui (+ SDL3 / OpenGL3 backends) — only linked by HBMapMaker today
# but exposed as a proper target so any future tool can reuse it.
# -----------------------------------------------------------------------------
if(EXISTS ${HBE_EXTERNAL_DIR}/imgui/imgui.cpp)
    add_library(hbe_imgui STATIC
        ${HBE_EXTERNAL_DIR}/imgui/imgui.cpp
        ${HBE_EXTERNAL_DIR}/imgui/imgui_draw.cpp
        ${HBE_EXTERNAL_DIR}/imgui/imgui_tables.cpp
        ${HBE_EXTERNAL_DIR}/imgui/imgui_widgets.cpp
        ${HBE_EXTERNAL_DIR}/imgui/imgui_demo.cpp
        ${HBE_EXTERNAL_DIR}/imgui/backends/imgui_impl_sdl3.cpp
        ${HBE_EXTERNAL_DIR}/imgui/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(hbe_imgui PUBLIC
        ${HBE_EXTERNAL_DIR}/imgui
        ${HBE_EXTERNAL_DIR}/imgui/backends
    )
    target_compile_definitions(hbe_imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
    # The backend translation units include <SDL3/SDL.h> and OpenGL loader
    # headers. Deferred to after the SDL3 target block so hbe::sdl3 exists.
    add_library(hbe::imgui ALIAS hbe_imgui)
endif()

# -----------------------------------------------------------------------------
# SDL3 / SDL3_mixer / SDL3_ttf
#
#   - HBE_USE_SYSTEM_SDL3=ON  → find_package (recommended on Linux/macOS)
#   - HBE_USE_SYSTEM_SDL3=OFF → use pre-built binaries in external/SDL3*
#                              (Windows dev workflow — matches the .vcxproj)
# -----------------------------------------------------------------------------
if(HBE_USE_SYSTEM_SDL3)
    find_package(SDL3 CONFIG REQUIRED)
    hbe_alias_target(hbe::sdl3 SDL3::SDL3)

    find_package(SDL3_mixer CONFIG QUIET)
    if(TARGET SDL3_mixer::SDL3_mixer)
        hbe_alias_target(hbe::sdl3_mixer SDL3_mixer::SDL3_mixer)
    else()
        message(WARNING
            "SDL3_mixer not found via find_package. "
            "On Arch/CachyOS, install it from the AUR (e.g. `yay -S sdl3_mixer`) "
            "or disable audio in HBE.Platform.SDL."
        )
        add_library(hbe_sdl3_mixer_stub INTERFACE)
        add_library(hbe::sdl3_mixer ALIAS hbe_sdl3_mixer_stub)
    endif()

    find_package(SDL3_ttf CONFIG QUIET)
    if(TARGET SDL3_ttf::SDL3_ttf)
        hbe_alias_target(hbe::sdl3_ttf SDL3_ttf::SDL3_ttf)
    endif()
else()
    # Bundled Windows binaries — mirrors what the .vcxproj files reference.
    set(_sdl3_root       ${HBE_EXTERNAL_DIR}/SDL3)
    set(_sdl3_mixer_root ${HBE_EXTERNAL_DIR}/SDL3_mixer)
    set(_sdl3_ttf_root   ${HBE_EXTERNAL_DIR}/SDL3_ttf)

    add_library(hbe_sdl3 SHARED IMPORTED GLOBAL)
    set_target_properties(hbe_sdl3 PROPERTIES
        IMPORTED_LOCATION             "${_sdl3_root}/lib/x64/SDL3.dll"
        IMPORTED_IMPLIB               "${_sdl3_root}/lib/x64/SDL3.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${_sdl3_root}/include"
    )
    add_library(hbe::sdl3 ALIAS hbe_sdl3)

    if(EXISTS "${_sdl3_mixer_root}/lib/x64/SDL3_mixer.lib")
        add_library(hbe_sdl3_mixer SHARED IMPORTED GLOBAL)
        set_target_properties(hbe_sdl3_mixer PROPERTIES
            IMPORTED_LOCATION             "${_sdl3_mixer_root}/lib/x64/SDL3_mixer.dll"
            IMPORTED_IMPLIB               "${_sdl3_mixer_root}/lib/x64/SDL3_mixer.lib"
            INTERFACE_INCLUDE_DIRECTORIES "${_sdl3_mixer_root}/include"
        )
        add_library(hbe::sdl3_mixer ALIAS hbe_sdl3_mixer)
    endif()

    if(EXISTS "${_sdl3_ttf_root}/lib/x64/SDL3_ttf.lib")
        add_library(hbe_sdl3_ttf SHARED IMPORTED GLOBAL)
        set_target_properties(hbe_sdl3_ttf PROPERTIES
            IMPORTED_LOCATION             "${_sdl3_ttf_root}/lib/x64/SDL3_ttf.dll"
            IMPORTED_IMPLIB               "${_sdl3_ttf_root}/lib/x64/SDL3_ttf.lib"
            INTERFACE_INCLUDE_DIRECTORIES "${_sdl3_ttf_root}/include"
        )
        add_library(hbe::sdl3_ttf ALIAS hbe_sdl3_ttf)
    endif()
endif()

# -----------------------------------------------------------------------------
# OpenGL
# -----------------------------------------------------------------------------
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)
add_library(hbe::opengl ALIAS OpenGL::GL)

# -----------------------------------------------------------------------------
# Wire ImGui backend translation units to their runtime deps (SDL3 + GL loader).
# Done after SDL3 / OpenGL are resolved above.
# -----------------------------------------------------------------------------
if(TARGET hbe_imgui)
    target_link_libraries(hbe_imgui PUBLIC hbe::sdl3 hbe::opengl)
endif()

# =============================================================================
# Helper: copy bundled SDL DLLs (and optional assets/) next to an executable
# after build. Mirrors the HBECopyRuntime target from the vcxproj files.
#
#   hbe_copy_runtime_deps(TARGET <exe> [ASSETS_DIR <dir>])
# =============================================================================
function(hbe_copy_runtime_deps)
    set(options)
    set(oneValueArgs TARGET ASSETS_DIR)
    set(multiValueArgs)
    cmake_parse_arguments(HCD "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT HCD_TARGET)
        message(FATAL_ERROR "hbe_copy_runtime_deps: TARGET is required")
    endif()

    # Only copy DLLs on Windows with the bundled build.
    if(WIN32 AND NOT HBE_USE_SYSTEM_SDL3)
        if(TARGET hbe_sdl3)
            add_custom_command(TARGET ${HCD_TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:hbe_sdl3>"
                    "$<TARGET_FILE_DIR:${HCD_TARGET}>"
                VERBATIM)
        endif()
        if(TARGET hbe_sdl3_mixer)
            add_custom_command(TARGET ${HCD_TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:hbe_sdl3_mixer>"
                    "$<TARGET_FILE_DIR:${HCD_TARGET}>"
                VERBATIM)
        endif()
        if(TARGET hbe_sdl3_ttf)
            add_custom_command(TARGET ${HCD_TARGET} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:hbe_sdl3_ttf>"
                    "$<TARGET_FILE_DIR:${HCD_TARGET}>"
                VERBATIM)
        endif()
    endif()

    if(HCD_ASSETS_DIR AND EXISTS ${HCD_ASSETS_DIR})
        add_custom_command(TARGET ${HCD_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${HCD_ASSETS_DIR}"
                "$<TARGET_FILE_DIR:${HCD_TARGET}>/assets"
            VERBATIM)
    endif()
endfunction()
