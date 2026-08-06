cmake_minimum_required(VERSION 3.24)

project(HonestlyBadEngine
        LANGUAGES C CXX
        DESCRIPTION "HBE — Honestly Bad Engine + tools + games"
)

# ----------------------------------------------------------------------------
# Global settings
# ----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)

# Multi-config generators (VS) ignore CMAKE_BUILD_TYPE, single-config need a default.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# Put all binaries in a single per-config output tree so runtime deps + assets
# can live next to the executables (same behavior as the vcxproj post-build).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ----------------------------------------------------------------------------
# Options
# ----------------------------------------------------------------------------
option(HBE_BUILD_SANDBOX  "Build HBE.Sandbox demo game" ON)
option(HBE_BUILD_MEGAX    "Build MegaX game"            ON)
option(HBE_BUILD_MAPMAKER "Build HBMapMaker editor"     ON)

# Default: use bundled SDL3 on Windows (matches the .vcxproj workflow),
# system SDL3 (find_package) on Linux / macOS.
if(WIN32)
    set(_hbe_default_system_sdl3 OFF)
else()
    set(_hbe_default_system_sdl3 ON)
endif()
option(HBE_USE_SYSTEM_SDL3
    "find_package(SDL3) instead of using the bundled external/SDL3 binaries"
    ${_hbe_default_system_sdl3}
)

# ----------------------------------------------------------------------------
# Compiler flags
# ----------------------------------------------------------------------------
if(MSVC)
    add_compile_options(/W3 /permissive- /Zc:__cplusplus /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS UNICODE _UNICODE)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
endif()

# ----------------------------------------------------------------------------
# Third-party dependencies
# ----------------------------------------------------------------------------
include(${CMAKE_CURRENT_LIST_DIR}/cmake/HBEDependencies.cmake)

# ----------------------------------------------------------------------------
# Engine libraries
# ----------------------------------------------------------------------------
add_subdirectory(HBE.Platform.SDL)
add_subdirectory(HBE.Renderer.GL)
add_subdirectory(HBE.Core)

# ----------------------------------------------------------------------------
# Games / tools
# ----------------------------------------------------------------------------
if(HBE_BUILD_SANDBOX)
    add_subdirectory(HBE.Sandbox)
endif()
if(HBE_BUILD_MEGAX)
    add_subdirectory(MegaX)
endif()
if(HBE_BUILD_MAPMAKER)
    add_subdirectory(HBMapMaker)
endif()

message(STATUS "")
message(STATUS "==== HBE build config ====")
message(STATUS "  Generator:        ${CMAKE_GENERATOR}")
message(STATUS "  Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler:         ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  System SDL3:      ${HBE_USE_SYSTEM_SDL3}")
message(STATUS "  Sandbox:          ${HBE_BUILD_SANDBOX}")
message(STATUS "  MegaX:            ${HBE_BUILD_MEGAX}")
message(STATUS "  HBMapMaker:       ${HBE_BUILD_MAPMAKER}")
message(STATUS "==========================")
message(STATUS "")
