#include "AppStateXrDemo.h"

#include <stdio.h>

#include "CameraManager.h"
#include "CoreMain.h"
#include "FrameTimer.h"
#include "GfxRenderApi.h"
#include "InputManager.h"
#include "Material.h"
#include "MaterialManager.h"
#include "PrintManager.h"
#include "PostProcess.h"
#include "RenderManager.h"
#include "RenderableSphere.h"
#include "SceneHierarchy.h"
#include "StateMachine.h"
#include "UiManager.h"
#include "UiSurface.h"
#include "UiWorldPanel.h"
#include "UiElement.h"
#include "OptionsController.h"
#include "ShowcaseHudController.h"
#include "PauseMenuController.h"
#include "XrModeController.h"
#include "XrGrabbable.h"
#include "XrHands.h"
#include "XrManager.h"
#include "XrPausePanel.h"

namespace
{
    // Where the panel stands: ahead of the play area origin, at eye height,
    // facing back towards it.
    const CC::Vector3 PANEL_WORLD_POSITION(0.0f, 1.4f, -1.5f);

    const char* const GRABBABLE_NAME[] = { "XrGrab01", "XrGrab02", "XrGrab03", "XrGrab04" };

    const CC::Vector3 GRABBABLE_HOME[] =
    {
        CC::Vector3(-0.4f, 0.9f, -0.6f),
        CC::Vector3( 0.0f, 0.9f, -0.7f),
        CC::Vector3( 0.4f, 0.9f, -0.6f),
        CC::Vector3( 0.0f, 1.2f, -0.5f),
    };
}

XrPanelController::XrPanelController()
    : readoutElement(nullptr)
{
}

void XrPanelController::Init()
{
    readoutElement = GetSurface()->GetElementById("readout");

    if (readoutElement != nullptr && !pendingReadout.empty())
    {
        readoutElement->SetTextContent(pendingReadout);
    }
}

void XrPanelController::SetReadout(const std::string& text)
{
    // Init may not have run yet when the state sets its first line, so the
    // text is held until there is an element to put it in.
    pendingReadout = text;

    if (readoutElement != nullptr)
    {
        readoutElement->SetTextContent(text);
        GetSurface()->InvalidateLayout();
    }
}

AppStateXrDemo::AppStateXrDemo()
    : handsObject(nullptr)
    , hands(nullptr)
    , panelObject(nullptr)
    , panelComponent(nullptr)
    , panelPosition(PANEL_WORLD_POSITION)
    , pausePanelObject(nullptr)
    , pausePanel(nullptr)
    , isXrActive(false)
    , pendingTogglePause(false)
    , panelController(nullptr)
    , lastReadoutTime(0.0f)
    , gradePresetIndex(0)
{
    isXrSupported = true;
    isPausable = true;

    for (int i = 0; i < GRABBABLE_COUNT; i++)
    {
        grabbableObject[i] = nullptr;
    }
}

AppStateXrDemo::~AppStateXrDemo()
{
}

// ========================
// Lifecycle
// ========================
void AppStateXrDemo::Init()
{
    // Absent a session the state still runs, flat, so the scene and the panel
    // can be worked on at a desk. Only the hands go away.
    isXrActive = CC::XrManager::Get() != nullptr && CC::XrManager::Get()->IsSessionRunning();

    CC::CameraManager::Get()->SetActiveCamera("CameraFree");

    SceneInit();
    InitControls();

    // The window keeps the application HUD and the pause menu. The panel in
    // the world has its own surface, so the two no longer take turns.
    CC::UiSurface* windowSurface = CC::UiManager::Get()->GetWindowSurface();
    windowSurface->Screens().RegisterScreen("PauseMenu", "Data/Ui/PauseMenu.html", "Data/Ui/PauseMenu.css",
        new PauseMenuController(
            [this]() { pendingTogglePause = true; },
            []() { CC::StateMachine::Get()->GotoState("Boot"); },
            []() { CC::CoreMain::Get()->RequestQuit(); }));
    windowSurface->Screens().RegisterScreen("Options", "Data/Ui/Options.html", "Data/Ui/Options.css",
        new OptionsController());
    windowSurface->Screens().RegisterScreen("Hud", "Data/Ui/ShowcaseHud.html", "Data/Ui/ShowcaseHud.css",
        new ShowcaseHudController([this]() { pendingTogglePause = true; }));
    windowSurface->Screens().RegisterScreen("XrMode", "Data/Ui/XrMode.html", "Data/Ui/XrMode.css",
        new XrModeController());

    ShowSceneScreen();

    CC::UiSurface* panelSurface = panelComponent->GetSurface();
    panelController = new XrPanelController();
    panelSurface->Screens().RegisterScreen("XrPanel", "Data/Ui/XrPanel.html", "Data/Ui/XrPanel.css",
                                           panelController);
    panelSurface->Screens().SetScreen("XrPanel");

    panelSurface->RegisterButtonAction("xrRecentre", [this]() { RecentreGrabbables(); });
    panelSurface->RegisterButtonAction("xrExit", []() { CC::StateMachine::Get()->GotoState("Boot"); });
    panelSurface->RegisterButtonAction("xrCyclePreset", [this]() { ApplyNextGradePreset(); });

    // The headset's pause menu is its own screen on its own surface, not a
    // copy of the window's: the two are operated independently.
    CC::UiSurface* pauseSurface = pausePanel->GetSurface();
    pauseSurface->Screens().RegisterScreen("XrPauseMenu", "Data/Ui/XrPauseMenu.html", "Data/Ui/XrPauseMenu.css",
        new PauseMenuController(
            [this]() { pendingTogglePause = true; },
            []() { CC::StateMachine::Get()->GotoState("Boot"); },
            []() { CC::CoreMain::Get()->RequestQuit(); }));
    pauseSurface->Screens().SetScreen("XrPauseMenu");

    CC::InputManager::Get()->SetInputTargetScene(CC::InputDomain::Window);
    CC::InputManager::Get()->SetInputTargetScene(CC::InputDomain::Headset);
    pendingTogglePause = false;

    CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "AppStateXrDemo::Init() (xr %s)",
            isXrActive ? "active" : "inactive");
}

void AppStateXrDemo::InitControls()
{
    // Escape on the keyboard, Start on a gamepad, and A on the right
    // controller. The XR menu button stays unbound and reserved for the
    // runtime's own dashboard. Pointing and grabbing are the engine's and
    // bound in every context.
    //
    // Paused binds only what resumes; menu navigation comes with every
    // context. Created first so the play context is the one left active.
    CC::InputActionMap& actionMap = CC::InputManager::Get()->GetActionMap();
    actionMap.CreateContext("AppStateXrDemoPaused");
    actionMap.RegisterAction(CC::ActionDef(Pause,   "Pause",   CC::InputTrigger::GamepadStart));
    actionMap.RegisterAction(CC::ActionDef(PauseXr, "PauseXr", CC::InputTrigger::XrPrimaryRight));

    actionMap.CreateContext("AppStateXrDemo");
    actionMap.RegisterAction(CC::ActionDef(Pause,   "Pause",   CC::InputTrigger::GamepadStart));
    actionMap.RegisterAction(CC::ActionDef(PauseXr, "PauseXr", CC::InputTrigger::XrPrimaryRight));
}

void AppStateXrDemo::SceneInit()
{
    CC::SceneHierarchy::Get()->LoadFromFile("Data/Scenes/ExampleScene.yaml");

    CC::MaterialManager* materials = CC::MaterialManager::Get();
    if (!materials->HasMaterial("XrGrabbable"))
    {
        materials->CreateMaterial("XrGrabbable", "LitColour", "", CC::Vector3(0.9f, 0.5f, 0.2f));
    }

    handsObject = new CC::SceneObject("XrHands");
    hands = new CC::XrHands();
    handsObject->AddComponent(hands);
    CC::SceneHierarchy::Get()->AddRootObject(handsObject);

    for (int i = 0; i < GRABBABLE_COUNT; i++)
    {
        grabbableHomePosition[i] = GRABBABLE_HOME[i];

        grabbableObject[i] = new CC::SceneObject(GRABBABLE_NAME[i]);
        grabbableObject[i]->AddComponent(new CC::RenderableSphere(materials->GetMaterial("XrGrabbable")));
        grabbableObject[i]->AddComponent(new CC::XrGrabbable());
        grabbableObject[i]->GetTransform().SetScale(CC::Vector3(0.08f, 0.08f, 0.08f));
        grabbableObject[i]->GetTransform().SetPosition(grabbableHomePosition[i]);
        CC::SceneHierarchy::Get()->AddRootObject(grabbableObject[i]);
    }

    // The procedural material is built in MaterialManager and referenced by
    // name from the scene, so its parameters are set here rather than loaded.
    // Left unset they are all zero and the surface renders black.
    CC::Material* proceduralVeins = materials->GetMaterial("ProceduralVeins");
    proceduralVeins->SetUniform("aspectRatio", 1.0f);
    proceduralVeins->SetUniform("patternScale", 6.0f);
    proceduralVeins->SetUniform("pulseSpeed", 0.8f);
    proceduralVeins->SetUniform("veinSharpness", 7.0f);
    proceduralVeins->SetUniform("glowColour", CC::Vector3(4.0f, 1.4f, 0.5f));

    // The panel's size in metres is its transform; the surface behind it
    // knows only pixels.
    panelObject = new CC::SceneObject("XrPanel");
    panelComponent = new CC::UiWorldPanel("XrDemoPanel", PANEL_SURFACE_WIDTH, PANEL_SURFACE_HEIGHT);
    panelObject->AddComponent(panelComponent);
    panelObject->GetTransform().SetPosition(panelPosition);
    panelObject->GetTransform().SetScale(CC::Vector3(PANEL_WIDTH, PANEL_HEIGHT, 1.0f));
    CC::SceneHierarchy::Get()->AddRootObject(panelObject);

    // Hidden until pause shows it in front of the head.
    pausePanelObject = new CC::SceneObject("XrPausePanel");
    pausePanelObject->AddComponent(new CC::UiWorldPanel("XrPausePanel", PANEL_SURFACE_WIDTH, PANEL_SURFACE_HEIGHT));
    pausePanel = new CC::XrPausePanel();
    pausePanelObject->AddComponent(pausePanel);
    pausePanelObject->GetTransform().SetScale(CC::Vector3(PAUSE_PANEL_WIDTH, PAUSE_PANEL_HEIGHT, 1.0f));
    CC::SceneHierarchy::Get()->AddRootObject(pausePanelObject);

    CC::SceneHierarchy::Get()->Init();
    CC::SceneHierarchy::Get()->SetEnabled(true);
}

void AppStateXrDemo::SceneShutdown()
{
    CC::SceneHierarchy::Get()->Shutdown();

    handsObject = nullptr;
    hands = nullptr;
    for (int i = 0; i < GRABBABLE_COUNT; i++)
    {
        grabbableObject[i] = nullptr;
    }
    panelObject = nullptr;
    pausePanelObject = nullptr;

    // The panels' Shutdown ran with the hierarchy and took their surfaces,
    // and the controllers registered on those surfaces went with them.
    panelComponent = nullptr;
    panelController = nullptr;
    pausePanel = nullptr;
}

void AppStateXrDemo::Shutdown()
{
    SceneShutdown();

    // The screens this state registered on the window go with it. The
    // app-lifetime ones stay.
    CC::UiManager::Get()->GetWindowSurface()->Screens().ClearAllScreens();
}

// ========================
// Frame
// ========================
void AppStateXrDemo::Update()
{
    // The runtime reports the session ready some frames after the state has
    // initialised, so this is watched rather than sampled once at Init.
    const bool wasXrActive = isXrActive;
    isXrActive = CC::XrManager::Get() != nullptr && CC::XrManager::Get()->IsSessionRunning();

    if (isXrActive != wasXrActive)
    {
        if (!IsPaused())
        {
            ShowSceneScreen();
        }
        CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "AppStateXrDemo: xr session %s",
                isXrActive ? "started" : "ended");
    }

    UpdatePauseInput();

    if (isXrActive && !IsPaused())
    {
        UpdateReadout();
    }
}

void AppStateXrDemo::UpdatePauseInput()
{
    CC::InputManager* input = CC::InputManager::Get();

    bool isPausePressed = input->EdgePositive(Pause) || input->EdgePositive(PauseXr);

    if (isPausePressed || pendingTogglePause)
    {
        pendingTogglePause = false;
        TogglePause();
    }
}

void AppStateXrDemo::OnPaused()
{
    CC::InputManager* input = CC::InputManager::Get();
    CC::UiSurface* windowSurface = CC::UiManager::Get()->GetWindowSurface();

    input->GetActionMap().SetContext("AppStateXrDemoPaused");

    // A finger on a stick when the menu opens never lifts off that stick,
    // so its last deflection would keep steering behind the menu.
    input->ClearAllJoystickOverrides();

    // The window and the headset each get their own menu. The world panel
    // keeps showing what it was showing, and stops taking rays because the
    // Headset target is now the pause panel alone.
    input->SetInputTargetSurface(CC::InputDomain::Window, windowSurface);
    windowSurface->Screens().SetScreen("PauseMenu");

    if (isXrActive)
    {
        pausePanel->Show();
        input->SetInputTargetSurface(CC::InputDomain::Headset, pausePanel->GetSurface());
    }
}

void AppStateXrDemo::OnResumed()
{
    CC::InputManager* input = CC::InputManager::Get();

    input->GetActionMap().SetContext("AppStateXrDemo");
    input->SetInputTargetScene(CC::InputDomain::Window);
    input->SetInputTargetScene(CC::InputDomain::Headset);

    pausePanel->Hide();
    ShowSceneScreen();
}

void AppStateXrDemo::ShowSceneScreen()
{
    // The HUD is the window's screen either way. It carries the tracking
    // label, which shows itself once a session owns input.
    CC::UiManager::Get()->GetWindowSurface()->Screens().SetScreen("Hud");
}

void AppStateXrDemo::UpdateReadout()
{
    if (!IsReadoutDue())
    {
        return;
    }

    std::string readout;

    if (!isXrActive)
    {
        readout = "No XR session - running flat";
    }
    else
    {
        // Raw action values and the reach to the nearest sphere. In a headset
        // this line is the only instrument available: a squeeze reading zero
        // says the binding never arrived, while a squeeze that moves with a
        // reach that never closes says the hands are somewhere else.
        CC::XrManager* xr = CC::XrManager::Get();

        const CC::TrackedPose& leftGrip = xr->GetHandPose(CC::XrHand::Left, CC::XrPoseKind::Grip);
        const CC::TrackedPose& rightGrip = xr->GetHandPose(CC::XrHand::Right, CC::XrPoseKind::Grip);

        float nearestReach = NearestGrabbableDistance();

        char buffer[256];
        snprintf(buffer, sizeof(buffer),
                 "%s  sq %.2f/%.2f  tr %.2f/%.2f  track %d%d  reach %.2f  held %d%d",
                 xr->IsSessionFocused() ? "FOCUS" : "NO-FOCUS",
                 xr->GetSqueezeValue(CC::XrHand::Left),
                 xr->GetSqueezeValue(CC::XrHand::Right),
                 xr->GetTriggerValue(CC::XrHand::Left),
                 xr->GetTriggerValue(CC::XrHand::Right),
                 leftGrip.isTracked ? 1 : 0,
                 rightGrip.isTracked ? 1 : 0,
                 nearestReach,
                 hands->IsHolding(CC::XrHand::Left) ? 1 : 0,
                 hands->IsHolding(CC::XrHand::Right) ? 1 : 0);
        readout = buffer;
    }

    if (readout != lastReadout)
    {
        lastReadout = readout;
        panelController->SetReadout(readout);
        CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "XrDemo: %s", readout.c_str());
    }
}

bool AppStateXrDemo::IsReadoutDue()
{
    // Setting the text invalidates layout, so a readout rewritten every frame
    // relays the panel out every frame and the buttons move under the ray.
    bool isDue = false;

    float now = CC::FrameTimer::Get()->TimeSinceStartup();
    if (now - lastReadoutTime >= READOUT_INTERVAL_SECONDS)
    {
        lastReadoutTime = now;
        isDue = true;
    }

    return isDue;
}

float AppStateXrDemo::NearestGrabbableDistance() const
{
    float nearest = 99.0f;

    for (int hand = 0; hand < HAND_COUNT; hand++)
    {
        const CC::TrackedPose& gripPose =
            CC::XrManager::Get()->GetHandPose((CC::XrHand)hand, CC::XrPoseKind::Grip);

        if (gripPose.isTracked)
        {
            CC::Vector3 gripPosition = gripPose.position;

            for (int i = 0; i < GRABBABLE_COUNT; i++)
            {
                if (grabbableObject[i] != nullptr)
                {
                    CC::Vector3 offset = grabbableObject[i]->GetWorldPosition() - gripPosition;
                    float distance = offset.Magnitude();
                    if (distance < nearest)
                    {
                        nearest = distance;
                    }
                }
            }
        }
    }

    return nearest;
}

void AppStateXrDemo::ApplyNextGradePreset()
{
    CC::PostProcess* postProcess = CC::RenderManager::Get()->GetPostProcess();
    int presetCount = postProcess->GetPresetCount();

    if (presetCount <= 0)
    {
        CCPrint(CC::PrintManager::CHANNEL_WARN, "AppStateXrDemo: no grade presets to cycle");
    }
    else
    {
        gradePresetIndex = (gradePresetIndex + 1) % presetCount;

        const char* presetName = postProcess->GetPresetName(gradePresetIndex);
        postProcess->ApplyPreset(presetName);

        CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "AppStateXrDemo: grade preset '%s'", presetName);
    }
}

void AppStateXrDemo::RecentreGrabbables()
{
    hands->ReleaseAll();

    for (int i = 0; i < GRABBABLE_COUNT; i++)
    {
        if (grabbableObject[i] != nullptr)
        {
            grabbableObject[i]->SetParent(nullptr);
            grabbableObject[i]->GetTransform().SetPosition(grabbableHomePosition[i]);
        }
    }

    for (int hand = 0; hand < HAND_COUNT; hand++)
    {
        if (isXrActive)
        {
            CC::XrManager::Get()->TriggerHaptic((CC::XrHand)hand, HAPTIC_AMPLITUDE, HAPTIC_DURATION_SECONDS);
        }
    }

    CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "AppStateXrDemo: grabbables recentred");
}
