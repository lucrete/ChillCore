# ChillCore Android native build
#
# Phase D step 2: full engine source closure compiles for Android.
# Sources are picked up via GLOB_RECURSE and filtered:
#   - Platform/Desktop/**          excluded (GLFW backend)
#   - Rendering/Gfx/OpenGl/**      excluded (desktop GL backend, glad)
#   - Ui/DevUi/**                  excluded — DevUiStub.cpp added back
#                                  in its place to provide no-op DevUi
#                                  for CC_DISABLE_DEVUI builds.

cmake_minimum_required(VERSION 3.22.1)

project("chillcore")

# Repo paths. CMakeLists.txt sits at:
#   Code/Targets/Android/app/src/main/cpp/CMakeLists.txt
# CMAKE_SOURCE_DIR resolves there; six levels up to Code/.
set(CC_CODE_ROOT "${CMAKE_SOURCE_DIR}/../../../../../..")
set(CC_CORE     "${CC_CODE_ROOT}/Core")
set(CC_APP      "${CC_CODE_ROOT}/App")
set(CC_PACKAGES "${CC_CODE_ROOT}/Packages")

# ============================================================================
# Engine source list
# ============================================================================

file(GLOB_RECURSE CC_CORE_SOURCES CONFIGURE_DEPENDS
    "${CC_CORE}/*.cpp"
)
file(GLOB_RECURSE CC_CORE_C_SOURCES CONFIGURE_DEPENDS
    "${CC_CORE}/*.c"
)
file(GLOB_RECURSE CC_APP_SOURCES CONFIGURE_DEPENDS
    "${CC_APP}/*.cpp"
)

# Exclude desktop-only and DevUi sources.
list(FILTER CC_CORE_SOURCES   EXCLUDE REGEX ".*/Platform/Desktop/.*")
list(FILTER CC_CORE_SOURCES   EXCLUDE REGEX ".*/Rendering/Gfx/OpenGl/.*")
list(FILTER CC_CORE_SOURCES   EXCLUDE REGEX ".*/Ui/DevUi/.*")
list(FILTER CC_CORE_C_SOURCES EXCLUDE REGEX ".*/Rendering/Gfx/OpenGl/.*")

set(CHILLCORE_SOURCES
    MainAndroid.cpp

    # Engine
    ${CC_CORE_SOURCES}
    ${CC_CORE_C_SOURCES}
    ${CC_APP_SOURCES}

    # DevUi no-op stub replaces the real DevUi*.cpp set on Android.
    ${CC_CORE}/Ui/DevUi/DevUiStub.cpp

    # Third-party — only what's actually consumed on Android.
    ${CC_PACKAGES}/pugixml-1.15/src/pugixml.cpp
)

add_library(chillcore SHARED ${CHILLCORE_SOURCES})

# ============================================================================
# Compile defines
# ============================================================================

target_compile_definitions(chillcore PRIVATE
    CC_GFX_BACKEND_GLES
    CC_DISABLE_DEVUI
    CC_PRINT_ENABLED
)

# ============================================================================
# Include directories — superset of the desktop vcxproj include set,
# minus desktop-only paths (glfw, glad, imgui).
# ============================================================================

target_include_directories(chillcore PRIVATE
    ${CC_APP}
    ${CC_APP}/AppStates
    ${CC_APP}/ProceduralArt
    ${CC_APP}/UiScreens

    ${CC_CORE}
    ${CC_CORE}/CCUtils
    ${CC_CORE}/Components
    ${CC_CORE}/Input
    ${CC_CORE}/Platform
    ${CC_CORE}/Platform/Android
    ${CC_CORE}/Rendering
    ${CC_CORE}/Rendering/Cameras
    ${CC_CORE}/Rendering/Gfx
    ${CC_CORE}/Rendering/Gfx/Gles
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
    ${CC_PACKAGES}/nlohmann
    ${CC_PACKAGES}/pugixml-1.15/src
    ${CC_PACKAGES}/rapidyaml
    ${CC_PACKAGES}/stb_image
    ${CC_PACKAGES}/zlib
)

target_compile_features(chillcore PRIVATE cxx_std_17)

# ============================================================================
# Linkage
# ============================================================================

find_package(game-activity REQUIRED CONFIG)

# GameActivity ships its JNI entry point as a library symbol; the linker
# strips it without explicit retention because nothing in C++ references it.
set(CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} -u Java_com_google_androidgamesdk_GameActivity_initializeNativeCode")

target_link_libraries(chillcore PRIVATE
    game-activity::game-activity_static
    EGL
    GLESv3
    android
    log
    z              # NDK-provided zlib, consumed by Core/CCUtils/PngLoader.cpp
)
