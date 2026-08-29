# ============================================================================
# Android target — GameActivity, EGL, GLES 3.1
#
# Gradle enters here. This file adds the entry point and the platform links;
# the engine itself comes from Code/CMakeLists.txt.
#
# This file sits at Code/Targets/Android/app/src/main/cpp/ — six levels below
# Code/.
# ============================================================================

cmake_minimum_required(VERSION 3.22.1)

project("chillcore")

set(CC_CODE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../..")

# Gradle passes these as cache entries; set them here as well so the build is
# correct when CMake is invoked directly.
set(CC_GFX_BACKEND_GLES ON)
set(CC_DISABLE_DEVUI    ON)

add_subdirectory("${CC_CODE_ROOT_DIR}" "${CMAKE_BINARY_DIR}/ChillCoreEngine")

add_library(chillcore SHARED MainAndroid.cpp)

target_link_libraries(chillcore PRIVATE ChillCoreEngine)

find_package(game-activity REQUIRED CONFIG)

# GameActivity ships its JNI entry point as a library symbol; the linker
# strips it without explicit retention because nothing in C++ references it.
target_link_options(chillcore PRIVATE
    "-u" "Java_com_google_androidgamesdk_GameActivity_initializeNativeCode"
)

target_link_libraries(chillcore PRIVATE
    game-activity::game-activity_static
    EGL
    GLESv3
    android
    log
    z              # NDK-provided zlib, consumed by Core/CCUtils/PngLoader.cpp
)
