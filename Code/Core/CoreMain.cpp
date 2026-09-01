#include <stdio.h>
#include "CoreMain.h"
#include "CCAssert.h"
#include "FontManager.h"
#include "TextRenderer.h"
#include "UiRenderer.h"
#include "UiManager.h"
#include "UiScreenSystem.h"
#include "StringDbManager.h"
#include "GfxRenderApi.h"
#include "AudioManager.h"

#ifdef __ANDROID__
    #include "PlatformFileSystemAndroid.h"
    #include "PlatformWindowAndroid.h"
#else
    #include "PlatformFileSystemDesktop.h"
    #include "PlatformWindowGlfw.h"
#endif

namespace CC
{
    CoreMain* CoreMain::instance = nullptr;

    CoreMain::CoreMain()
        : isQuitRequested(false)
    {
        CC_ASSERT(instance == nullptr, "CoreMain already created");
        instance = this;
    }

    CoreMain::~CoreMain()
    {
        instance = nullptr;
    }

    CoreMain* CoreMain::Get()
    {
        CC_ASSERT(instance != nullptr, "CoreMain not created yet");
        return instance;
    }

    bool CoreMain::IsQuitRequested() const
    {
        return isQuitRequested;
    }

    void CoreMain::Init()
    {
#ifdef __ANDROID__
        platformFileSystem = new PlatformFileSystemAndroid();
#else
        platformFileSystem = new PlatformFileSystemDesktop();
#endif

#ifdef __ANDROID__
        platformWindow = new PlatformWindowAndroid();
#else
        platformWindow = new PlatformWindowGlfw();
#endif
        PlatformWindow::Config windowConfig;
        platformWindow->Init(windowConfig);

        inputManager = new InputManager();
        printManager = new PrintManager();
        frameTimer = new FrameTimer();
        audioManager = new AudioManager();
        sceneHierarchy = new SceneHierarchy();
        devUi = new DevUi();
        renderManager = new RenderManager();
        fontManager = new FontManager();
        textRenderer = new TextRenderer();
        uiRenderer = new UiRenderer();
        stringDbManager = new StringDbManager();
        uiManager = new UiManager();
        uiScreenSystem = new UiScreenSystem();
    }

    void CoreMain::Shutdown()
    {
        delete(uiScreenSystem);
        delete(uiManager);
        delete(stringDbManager);
        delete(uiRenderer);
        delete(textRenderer);
        delete(fontManager);
        delete(renderManager);
        delete(devUi);
        delete(sceneHierarchy);
        delete(audioManager);
        delete(frameTimer);
        delete(printManager);
        delete(inputManager);

        platformWindow->Shutdown();
        delete(platformWindow);

        delete(platformFileSystem);
    }

    void CoreMain::TickFrame(IAppMain* appMain)
    {
        CC_ASSERT(appMain != NULL, "AppMain not set");

        frameTimer->FrameStart();

        // FrameStart phase
        frameTimer->AddTimestamp(StandardTimestamp::UpdateInput,      "InputManager");
        inputManager->PollPlatform();   // glfwPollEvents — fresh OS input into GLFW
        devUi->StartFrame();            // ImGui::NewFrame consumes fresh GLFW input
        inputManager->Update();         // queries IsHovered with this-frame ImGui state
        frameTimer->AddTimestamp(StandardTimestamp::UpdateStartFrame, "RenderStart");
        renderManager->StartFrame();

        // Update phase
        frameTimer->AddTimestamp(StandardTimestamp::AppMain,          "AppMain");
        appMain->Update();
        frameTimer->AddTimestamp(StandardTimestamp::SceneUpdate,      "SceneHierarchy");
        sceneHierarchy->Update();

        // UiUpdate phase
        frameTimer->AddTimestamp(StandardTimestamp::UiScreen,         "UiScreen");
        uiScreenSystem->Update();
        frameTimer->AddTimestamp(StandardTimestamp::UiUpdate,         "UiUpdate");
        uiManager->Update();
        frameTimer->AddTimestamp(StandardTimestamp::DevUiUpdate,      "DevUi");
        devUi->Update();                // toggles + content building (overlay, view draws)
        audioManager->Update();         // reap finished SFX after UI/DevUi may have triggered new ones

        // Render phase
        frameTimer->AddTimestamp(StandardTimestamp::Render,           "Render");
        renderManager->Render();

        // UiRender phase
        frameTimer->AddTimestamp(StandardTimestamp::UiRender,         "Ui Render");
        Gfx::RenderApi::Get()->AddGpuTimestamp("UI");
        uiManager->Render();
        textRenderer->EndFrame();
        devUi->EndFrame();
        Gfx::RenderApi::Get()->AddGpuTimestamp("Finalize");

        // SwapBuffers phase
        frameTimer->AddTimestamp(StandardTimestamp::SwapBuffers,      "SwapBuffers");
        renderManager->EndFrame();
    }

    // Desktop convenience: while-loop wrapper around TickFrame. Android
    // shells drive TickFrame directly from inside android_main's
    // ALooper-driven outer loop and do not call Run.
    void CoreMain::Run(IAppMain* appMain)
    {
        while (!IsQuitRequested())
        {
            TickFrame(appMain);
        }
    }

    // Surface-lost ordering: notify the graphics backend first so it
    // can drop any cached state that depends on the current binding,
    // then let PlatformWindow rebind the context to a 1×1 pbuffer and
    // destroy the on-screen surface.
    void CoreMain::OnSurfaceLost()
    {
        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();
        if (gfxApi != nullptr)
        {
            gfxApi->OnSurfaceLost();
        }
        platformWindow->OnSurfaceLost();
    }

    // Surface-restored ordering: PlatformWindow first so the context is
    // bound to the new on-screen surface before the backend's hook runs
    // and might issue GL calls.
    void CoreMain::OnSurfaceRestored()
    {
        platformWindow->OnSurfaceRestored();
        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();
        if (gfxApi != nullptr)
        {
            gfxApi->OnSurfaceRestored();
        }
    }

    // Context-lost ordering: backend first so it can drop / mark
    // cached state as invalid while the soon-to-be-released context is
    // still current; PlatformWindow then handles the EGL bookkeeping.
    void CoreMain::OnContextLost()
    {
        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();
        if (gfxApi != nullptr)
        {
            gfxApi->OnContextLost();
        }
        platformWindow->OnContextLost();
    }

    // The hook surface stays in place for future persistence-driven
    // recovery (a saved-state restore on relaunch). The current
    // policy on context loss is "log it, request quit, let the
    // process restart cold" rather than rebuild GL handles in place,
    // so this body is empty.
    void CoreMain::OnContextRestored()
    {
    }
}