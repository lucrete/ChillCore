# ============================================================================
# Desktop target — Windows, GLFW, desktop GL 4.3
#
# Adds the entry point and the platform links. Everything else comes from
# ChillCoreEngine.
# ============================================================================

add_executable(ChillCore MainDesktop.cpp)

source_group("Targets/Desktop" FILES MainDesktop.cpp)

target_link_libraries(ChillCore PRIVATE ChillCoreEngine)

target_link_directories(ChillCore PRIVATE
    "${CC_PACKAGES}/zlib"
    "${CC_PACKAGES}/glfw-3.4.bin.WIN64/lib-vc2022"
)

if(CC_ENABLE_XR)
    target_link_directories(ChillCore PRIVATE "${CC_OPENXR_PACKAGE}/lib-vc2022")

    # The loader pulls in the Windows registry and COM APIs it uses to
    # enumerate runtimes.
    target_link_libraries(ChillCore PRIVATE
        $<IF:$<CONFIG:Debug>,openxr_loaderd.lib,openxr_loader.lib>
        advapi32.lib
        ole32.lib
    )
endif()

target_link_libraries(ChillCore PRIVATE
    glfw3_mt.lib
    opengl32.lib
    $<IF:$<CONFIG:Debug>,libz-static-d.lib,libz-static.lib>
)

if(MSVC)
    # The debug zlib is built against the debug CRT; the release import of
    # LIBCMT arrives through it and conflicts.
    target_link_options(ChillCore PRIVATE $<$<CONFIG:Debug>:/NODEFAULTLIB:LIBCMT>)
endif()

# Assets are addressed relative to the application directory, not the
# executable, so the debugger must start there.
set_target_properties(ChillCore PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CC_APP}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CC_REPO_ROOT}/Build/x64/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CC_REPO_ROOT}/Build/x64/Release"
)

set_property(DIRECTORY "${CC_CODE_ROOT}" PROPERTY VS_STARTUP_PROJECT ChillCore)
