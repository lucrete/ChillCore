#ifndef COREMAIN_H
#define COREMAIN_H
#include "InputManager.h"
#include "PrintManager.h"
#include "FrameTimer.h"
#include "SceneHierarchy.h"
#include "RenderManager.h"
#include "AppMainInterface.h"
#include "DevUi.h"
#include "PlatformFileSystem.h"
#include "PlatformWindow.h"

namespace CC
{
    class FontManager;
    class TextRenderer;
    class UiRenderer;
    class UiManager;
    class UiScreenSystem;
    class StringDbManager;
    class AudioManager;

    class CoreMain
    {
    public:
        CoreMain();
        virtual ~CoreMain();

        static CoreMain* Get();

        void Init();
        void TickFrame(IAppMain* appMain);
        void Run(IAppMain* appMain);
        void Shutdown();

        // Surface lifecycle. Called by the platform shell when the
        // on-screen window goes / comes back. Forwards to PlatformWindow
        // and the graphics backend so the graphics context (and every
        // GL handle) survives the cycle.
        void OnSurfaceLost();
        void OnSurfaceRestored();

        // Context lifecycle. Called when the underlying graphics
        // context is destroyed and recreated, invalidating every GL
        // handle the engine holds. Subsystems rebuild their GPU
        // resources from CPU-authoritative sources between these two
        // calls.
        void OnContextLost();
        void OnContextRestored();

        void RequestQuit() { isQuitRequested = true; }
        bool IsQuitRequested() const;

    private:
        static CoreMain* instance;
        bool isQuitRequested = false;
        PlatformFileSystem* platformFileSystem;
        PlatformWindow* platformWindow;
        InputManager* inputManager;
        PrintManager* printManager;
        FrameTimer* frameTimer;
        SceneHierarchy* sceneHierarchy;
        RenderManager* renderManager;
        DevUi* devUi;
        FontManager* fontManager;
        TextRenderer* textRenderer;
        UiRenderer* uiRenderer;
        StringDbManager* stringDbManager;
        UiManager* uiManager;
        UiScreenSystem* uiScreenSystem;
        AudioManager* audioManager;
    };
}
#endif // COREMAIN_H
