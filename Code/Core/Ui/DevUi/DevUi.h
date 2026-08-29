#ifndef DEVUI_H
#define DEVUI_H

#include <string>

namespace CC
{
    class CommandConsole;
    class DevUiViewFrameProfile;
    class DevUiViewSceneHierarchy;
    class DevUiViewCommandConsole;
    class DevUiViewAbout;

    class DevUi
    {
    public:
        DevUi();
        ~DevUi();
        static DevUi* Get();

        void Update();
        // Native window handle is pulled from PlatformWindow internally —
        // callers don't need to know which windowing backend is in use.
        void Init();
        void Shutdown();
        void StartFrame();
        void EndFrame();
        bool IsHovered();
        bool IsCapturingKeyboard();
        void SetContextText(const std::string& text);

    private:
        static DevUi* instance;

        CommandConsole* commandConsole = nullptr;
        DevUiViewFrameProfile* frameProfileView = nullptr;
        DevUiViewSceneHierarchy* sceneHierarchyView = nullptr;
        DevUiViewCommandConsole* commandConsoleView = nullptr;
        DevUiViewAbout* aboutView = nullptr;

        void DrawMainMenuBar();
        void DrawDevOverlay();
        void DrawViewToggles();

        std::string contextText;
        float mainMenuBarHeight = 0.0f;
        bool showDemoWindow = false;
        bool wasConsoleVisible = false;
        bool savedMouseLockState = false;
        bool wasCapturing = false;
    };
}

#endif // DEVUI_H
