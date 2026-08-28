#include "DevUi.h"
#include <GLFW/glfw3.h>
#include "CCAssert.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "InputManager.h"
#include "FrameTimer.h"
#include "PrintManager.h"
#include "CommandConsole.h"
#include "DevUiViewFrameProfile.h"
#include "DevUiViewSceneHierarchy.h"
#include "DevUiViewCommandConsole.h"

namespace CC
{
    DevUi* DevUi::instance = NULL;

    DevUi::DevUi()
    {
        CC_ASSERT(instance == NULL, "DevUi already created");
        instance = this;
        commandConsole = new CommandConsole();
        frameProfileView = new DevUiViewFrameProfile();
        sceneHierarchyView = new DevUiViewSceneHierarchy();
        commandConsoleView = new DevUiViewCommandConsole();

        commandConsole->RegisterCommand("profile",
            "Frame profiling. 'profile' for snapshot, 'profile long' for 3s CSV capture",
            [this](const std::vector<std::string>& args)
            {
                bool isLongCapture = !args.empty() && args[0] == "long";

                if (isLongCapture && FrameTimer::Get()->IsCapturing())
                {
                    CCPrint(PrintManager::CHANNEL_ALWAYS, "Capture already in progress");
                }
                else if (isLongCapture)
                {
                    FrameTimer::Get()->StartCapture(3.0f);
                    CCPrint(PrintManager::CHANNEL_ALWAYS, "Capturing 3 seconds of frame data...");
                }
                else
                {
                    std::string snapshot = frameProfileView->GetFrameTimingSnapshot();
                    CCPrint(PrintManager::CHANNEL_ALWAYS, "%s", snapshot.c_str());
                    ImGui::SetClipboardText(snapshot.c_str());
                    CCPrint(PrintManager::CHANNEL_ALWAYS, "(Copied to clipboard)");
                }
            });
    }

    DevUi::~DevUi()
    {
        delete commandConsoleView;
        delete sceneHierarchyView;
        delete frameProfileView;
        delete commandConsole;
        instance = nullptr;
    }

    DevUi* DevUi::Get()
    {
        CC_ASSERT(instance != NULL, "DevUi not created yet");
        return instance;
    }

    void DevUi::Update()
    {
        if (InputManager::Get()->EdgePositive(InputAction::DevToggleFrameProfile))
        {
            frameProfileView->ToggleVisible();
        }
        if (InputManager::Get()->EdgePositive(InputAction::DevToggleSceneHierarchy))
        {
            sceneHierarchyView->ToggleVisible();
        }
        if (InputManager::Get()->EdgePositive(InputAction::DevToggleCommandConsole))
        {
            commandConsoleView->ToggleVisible();
        }

        bool isConsoleVisible = commandConsoleView->IsVisible();
        if (isConsoleVisible && !wasConsoleVisible)
        {
            savedMouseLockState = InputManager::Get()->IsMouseCursorLocked();
            InputManager::Get()->LockMouseCursor(false);
            commandConsoleView->RequestFocus();
        }
        else if (!isConsoleVisible && wasConsoleVisible)
        {
            InputManager::Get()->LockMouseCursor(savedMouseLockState);
        }
        wasConsoleVisible = isConsoleVisible;

        // ImGui content building. Cost lands in the UiUpdate CPU phase.
        if (showDemoWindow)
        {
            ImGui::ShowDemoWindow(&showDemoWindow);
        }

        DrawDevOverlay();

        FrameTimer::Get()->RecordProfileData();

        if (wasCapturing && !FrameTimer::Get()->IsCapturing())
        {
            std::string csv = FrameTimer::Get()->WriteCaptureToFile("profile_capture.csv");
            ImGui::SetClipboardText(csv.c_str());
            CCPrint(PrintManager::CHANNEL_ALWAYS, "(Copied to clipboard)");
        }
        wasCapturing = FrameTimer::Get()->IsCapturing();

        if (frameProfileView->IsVisible())
        {
            frameProfileView->Draw();
        }

        if (sceneHierarchyView->IsVisible())
        {
            sceneHierarchyView->Draw();
        }

        if (commandConsoleView->IsVisible())
        {
            commandConsoleView->Draw();
        }
    }

    void DevUi::Init()
    {
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        //io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch

        // Setup Platform/Renderer backends. ImGui ships per-windowing-system
        // platform backends (imgui_impl_glfw / imgui_impl_android / ...)
        // and per-graphics-API renderer backends (imgui_impl_opengl3 /
        // imgui_impl_vulkan / ...). This file is the desktop-GLFW + GL3
        // pair; the future Android build will be a sibling file using
        // imgui_impl_android + imgui_impl_opengl3 (IMGUI_IMPL_OPENGL_ES3).
        // glfwGetCurrentContext returns the window the GL context belongs
        // to — set current by PlatformWindow::Init.
        ImGui_ImplGlfw_InitForOpenGL(glfwGetCurrentContext(), true);
        ImGui_ImplOpenGL3_Init();
    }

    void DevUi::Shutdown()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void DevUi::StartFrame()
    {
        // ImGui frame opens here so InputManager::Update can query
        // IsHovered / IsCapturingKeyboard against this-frame state. All
        // ImGui content building (overlays, view draws, profile recording)
        // lives in Update so its cost is attributed to the DevUi update
        // CPU phase rather than this minimal new-frame call.
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void DevUi::EndFrame()
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    bool DevUi::IsHovered()
    {
        return ImGui::GetIO().WantCaptureMouse;
    }

    bool DevUi::IsCapturingKeyboard()
    {
        return ImGui::GetIO().WantCaptureKeyboard;
    }

    void DevUi::SetContextText(const std::string& text)
    {
        contextText = text;
    }

    void DevUi::DrawDevOverlay()
    {
        const float xyPosition = 10.0f;
        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoMove;

        ImGui::SetNextWindowPos(ImVec2(xyPosition, xyPosition), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.35f);

        if (ImGui::Begin("FPS", nullptr, flags))
        {
            ImGui::Text("%.1f FPS (%.1f)", FrameTimer::Get()->GetFramesPerSecond(), ImGui::GetIO().Framerate);
            ImGui::Text("Mouse: %.0f, %.0f", ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);

            if (!contextText.empty())
            {
                ImGui::Text("%s", contextText.c_str());
            }
        }

        ImGuiTreeNodeFlags baseFlags = ImGuiTreeNodeFlags_DrawLinesToNodes;

        if (ImGui::TreeNodeEx("Extras", baseFlags))
        {
            if (ImGui::TreeNodeEx("Input Mappings", baseFlags))
            {
                ActiveInputType activeInput = InputManager::Get()->GetActiveInputType();
                const char* inputTypeName = (activeInput == ActiveInputType::KeyboardMouse) ? "Keyboard/Mouse" : "Gamepad";
                ImGui::Text("Active Input: %s", inputTypeName);
                ImGui::Separator();

                InputActionMap& actionMap = InputManager::Get()->GetActionMap();
                int maxAction = actionMap.GetMaxActionIndex();

                for (int i = 0; i <= maxAction; i++)
                {
                    if (actionMap.IsBound(i))
                    {
                        ImGui::Text("%s: %s [%s]",
                            actionMap.GetActionName(i).c_str(),
                            actionMap.GetTriggerName(i).c_str(),
                            actionMap.GetKeyComboString(i).c_str());
                    }
                }
                ImGui::TreePop();
            }

            DrawViewToggles();

            if (ImGui::TreeNodeEx("Debug", baseFlags))
            {
                ImGui::Checkbox("Show Demo Window", &showDemoWindow);
                ImGui::TreePop();
            }

            ImGui::TreePop();
        }

        ImGui::End();
    }

    void DevUi::DrawViewToggles()
    {
        ImGuiTreeNodeFlags baseFlags = ImGuiTreeNodeFlags_DrawLinesToNodes;

        if (ImGui::TreeNodeEx("Views", baseFlags))
        {
            bool isFrameProfileVisible = frameProfileView->IsVisible();
            if (ImGui::Checkbox("Frame Profile", &isFrameProfileVisible))
            {
                frameProfileView->SetVisible(isFrameProfileVisible);
            }

            bool isSceneHierarchyVisible = sceneHierarchyView->IsVisible();
            if (ImGui::Checkbox("Scene Hierarchy", &isSceneHierarchyVisible))
            {
                sceneHierarchyView->SetVisible(isSceneHierarchyVisible);
            }

            bool isCommandConsoleVisible = commandConsoleView->IsVisible();
            if (ImGui::Checkbox("Command Console", &isCommandConsoleVisible))
            {
                commandConsoleView->SetVisible(isCommandConsoleVisible);
            }
            ImGui::TreePop();
        }
    }
}