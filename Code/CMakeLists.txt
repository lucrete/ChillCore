# ============================================================================
# ChillCore — root build description
#
# Describes the engine as a library. Knows nothing about entry points or
# platform shells; each target under Targets/ adds its own entry point and
# platform links. See AGD-0040 and Planning/BuildPipelinePlan.md.
# ============================================================================

cmake_minimum_required(VERSION 3.22.1)

project(ChillCore LANGUAGES CXX C)

set(CC_CODE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(CC_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(CC_CORE      "${CC_CODE_ROOT}/Core")
set(CC_APP       "${CC_CODE_ROOT}/App")
set(CC_PACKAGES  "${CC_CODE_ROOT}/Packages")

option(CC_GFX_BACKEND_GLES "Compile the GLES backend instead of desktop GL" OFF)
option(CC_DISABLE_DEVUI    "Compile the no-op DevUi stub instead of the ImGui DevUi" OFF)

if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
endif()

# Static CRT. Every vendored dependency is built /MT; a mismatch surfaces as
# link errors rather than a clear diagnostic.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ============================================================================
# Source closure
#
# Globbed rather than listed. With one source tree shared by every target, an
# explicit list becomes a second place to remember, and the failure mode — a
# file silently absent from one target — is worse than the reproducibility
# risk. Exclusions are the mirror image of each other: a target compiles the
# platform sources it needs and none of the others.
# ============================================================================

file(GLOB_RECURSE CC_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CC_CORE}/*.cpp"
    "${CC_CORE}/*.c"
    "${CC_APP}/*.cpp"
    "${CC_APP}/*.c"
)
file(GLOB_RECURSE CC_ENGINE_HEADERS CONFIGURE_DEPENDS
    "${CC_CORE}/*.h"
    "${CC_APP}/*.h"
)

if(CC_GFX_BACKEND_GLES)
    list(FILTER CC_ENGINE_SOURCES EXCLUDE REGEX ".*/Platform/Desktop/.*")
    list(FILTER CC_ENGINE_SOURCES EXCLUDE REGEX ".*/Rendering/Gfx/OpenGl/.*")
else()
    list(FILTER CC_ENGINE_SOURCES EXCLUDE REGEX ".*/Platform/Android/.*")
    list(FILTER CC_ENGINE_SOURCES EXCLUDE REGEX ".*/Rendering/Gfx/Gles/.*")
endif()

# DevUi is all-or-nothing: either the ImGui views compile, or the stub does.
if(CC_DISABLE_DEVUI)
    list(FILTER CC_ENGINE_SOURCES EXCLUDE REGEX ".*/Ui/DevUi/.*")
    list(APPEND CC_ENGINE_SOURCES "${CC_CORE}/Ui/DevUi/DevUiStub.cpp")
else()
    list(FILTER CC_ENGINE_SOURCES EXCLUDE REGEX ".*/Ui/DevUi/DevUiStub\\.cpp$")
endif()

# ============================================================================
# Vendored third-party sources compiled into the engine
# ============================================================================

set(CC_THIRD_PARTY_SOURCES
    "${CC_PACKAGES}/pugixml-1.15/src/pugixml.cpp"
)

if(NOT CC_DISABLE_DEVUI)
    list(APPEND CC_THIRD_PARTY_SOURCES
        "${CC_PACKAGES}/imgui/imgui.cpp"
        "${CC_PACKAGES}/imgui/imgui_demo.cpp"
        "${CC_PACKAGES}/imgui/imgui_draw.cpp"
        "${CC_PACKAGES}/imgui/imgui_tables.cpp"
        "${CC_PACKAGES}/imgui/imgui_widgets.cpp"
        "${CC_PACKAGES}/imgui/backends/imgui_impl_glfw.cpp"
        "${CC_PACKAGES}/imgui/backends/imgui_impl_opengl3.cpp"
    )
endif()

if(NOT CC_GFX_BACKEND_GLES)
    # Vorbis decoding for miniaudio. Android's audio path does not use it.
    list(APPEND CC_THIRD_PARTY_SOURCES "${CC_PACKAGES}/miniaudio/stb_vorbis.c")
endif()

# ============================================================================
# Engine library
# ============================================================================

add_library(ChillCoreEngine STATIC
    ${CC_ENGINE_SOURCES}
    ${CC_ENGINE_HEADERS}
    ${CC_THIRD_PARTY_SOURCES}
)

target_compile_features(ChillCoreEngine PUBLIC cxx_std_17)

# The Solution Explorer tree is derived from the on-disk hierarchy rather than
# hand-maintained.
source_group(TREE "${CC_CODE_ROOT}" FILES
    ${CC_ENGINE_SOURCES}
    ${CC_ENGINE_HEADERS}
    ${CC_THIRD_PARTY_SOURCES}
)

target_include_directories(ChillCoreEngine PUBLIC
    ${CC_APP}
    ${CC_APP}/AppStates
    ${CC_APP}/AudioTracker
    ${CC_APP}/AudioTracker/UiScreens
    ${CC_APP}/ProceduralArt
    ${CC_APP}/UiScreens

    ${CC_CORE}
    ${CC_CORE}/Audio
    ${CC_CORE}/AudioTracker
    ${CC_CORE}/AudioTracker/Commands
    ${CC_CORE}/AudioTracker/Synthesis
    ${CC_CORE}/AudioTracker/Synthesis/Generators
    ${CC_CORE}/CCUtils
    ${CC_CORE}/Components
    ${CC_CORE}/Input
    ${CC_CORE}/Platform
    ${CC_CORE}/Rendering
    ${CC_CORE}/Rendering/Cameras
    ${CC_CORE}/Rendering/Gfx
    ${CC_CORE}/Rendering/Gfx/GlCommon
    ${CC_CORE}/Rendering/Lighting
    ${CC_CORE}/Rendering/Materials
    ${CC_CORE}/Rendering/Renderables
    ${CC_CORE}/Rendering/ShaderManagement
    ${CC_CORE}/Rendering/TextureManagement
    ${CC_CORE}/Rendering/Uniforms
    ${CC_CORE}/SceneManagement
    ${CC_CORE}/StateMachine
    ${CC_CORE}/Ui
    ${CC_CORE}/Ui/DevUi
    ${CC_CORE}/Ui/Elements
    ${CC_CORE}/Ui/Font
    ${CC_CORE}/Ui/Interaction
    ${CC_CORE}/Ui/Layout
    ${CC_CORE}/Ui/Rendering

    ${CC_PACKAGES}
    ${CC_PACKAGES}/cgltf-1.15
    ${CC_PACKAGES}/miniaudio
    ${CC_PACKAGES}/nlohmann
    ${CC_PACKAGES}/pugixml-1.15/src
    ${CC_PACKAGES}/rapidyaml
    ${CC_PACKAGES}/stb_image
    ${CC_PACKAGES}/zlib
)

if(CC_GFX_BACKEND_GLES)
    target_include_directories(ChillCoreEngine PUBLIC
        ${CC_CORE}/Platform/Android
        ${CC_CORE}/Rendering/Gfx/Gles
    )
else()
    target_include_directories(ChillCoreEngine PUBLIC
        ${CC_CORE}/Platform/Desktop
        ${CC_CORE}/Rendering/Gfx/OpenGl
        ${CC_PACKAGES}/glfw-3.4.bin.WIN64/include
        ${CC_PACKAGES}/imgui
        ${CC_PACKAGES}/imgui/backends
        ${CC_PACKAGES}/include
        ${CC_PACKAGES}/include/glad
        ${CC_PACKAGES}/include/KHR
    )
endif()

# ============================================================================
# Compile definitions
#
# The graphics backend is selected by definition, not by directory. Desktop GL
# is the absence of a definition.
# ============================================================================

target_compile_definitions(ChillCoreEngine PUBLIC
    $<$<CONFIG:Debug>:CC_PRINT_ENABLED>
    $<$<CONFIG:Debug>:CC_DEBUG>
)

if(CC_GFX_BACKEND_GLES)
    target_compile_definitions(ChillCoreEngine PUBLIC CC_GFX_BACKEND_GLES)
endif()

if(CC_DISABLE_DEVUI)
    target_compile_definitions(ChillCoreEngine PUBLIC CC_DISABLE_DEVUI)
endif()

if(MSVC)
    target_compile_definitions(ChillCoreEngine PUBLIC _CONSOLE GLFW_INCLUDE_NONE)
    target_compile_options(ChillCoreEngine PUBLIC /W3 /permissive- /sdl)
endif()

# ============================================================================
# Targets
# ============================================================================

# Android is configured by Gradle, which enters at its own target description.
if(NOT CC_GFX_BACKEND_GLES)
    add_subdirectory(Targets/Desktop)
endif()
