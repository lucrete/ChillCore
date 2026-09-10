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
#include "RenderableCube.h"
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
#include "XrManager.h"

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
    : panelObject(nullptr)
    , panelComponent(nullptr)
    , panelController(nullptr)
    , lastReadoutTime(0.0f)
    , gradePresetIndex(0)
    , panelPosition(PANEL_WORLD_POSITION)
    , isXrActive(false)
    , isPaused(false)
    , pendingTogglePause(false)
{
    for (int hand = 0; hand < HAND_COUNT; hand++)
    {
        handObject[hand] = nullptr;
        rayObject[hand] = nullptr;
        heldObject[hand] = nullptr;
        wasHoveringPanel[hand] = false;
    }

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

    ShowSceneScreen();

    CC::UiSurface* panelSurface = panelComponent->GetSurface();
    panelController = new XrPanelController();
    panelSurface->Screens().RegisterScreen("XrPanel", "Data/Ui/XrPanel.html", "Data/Ui/XrPanel.css",
                                           panelController);
    panelSurface->Screens().SetScreen("XrPanel");

    panelSurface->RegisterButtonAction("xrRecentre", [this]() { RecentreGrabbables(); });
    panelSurface->RegisterButtonAction("xrExit", []() { CC::StateMachine::Get()->GotoState("Boot"); });
    panelSurface->RegisterButtonAction("xrCyclePreset", [this]() { ApplyNextGradePreset(); });

    CC::InputManager::Get()->SetInteractionMode(CC::InteractionMode::World);
    CC::InputManager::Get()->SetPaused(false);

    CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "AppStateXrDemo::Init() (xr %s)",
            isXrActive ? "active" : "inactive");
}

void AppStateXrDemo::InitControls()
{
    CC::InputActionMap& actionMap = CC::InputManager::Get()->GetActionMap();
    actionMap.CreateContext("AppStateXrDemo");

    // Squeeze grabs, trigger selects. Both hands bind the same pair of
    // actions to their own trigger, so either hand drives either.
    actionMap.RegisterAction(CC::ActionDef(Grab,            "Grab",            CC::InputTrigger::XrSqueezeLeft));
    actionMap.RegisterAction(CC::ActionDef(GrabSecondary,   "GrabSecondary",   CC::InputTrigger::XrSqueezeRight));
    actionMap.RegisterAction(CC::ActionDef(Select,          "Select",          CC::InputTrigger::XrTriggerLeft));
    actionMap.RegisterAction(CC::ActionDef(SelectSecondary, "SelectSecondary", CC::InputTrigger::XrTriggerRight));
    // Escape on the keyboard, Start on a gamepad, and the secondary face
    // button on either controller. The XR menu button stays unbound and
    // reserved for the runtime's own dashboard.
    actionMap.RegisterAction(CC::ActionDef(Pause,        "Pause",        CC::InputTrigger::GamepadStart));
    actionMap.RegisterAction(CC::ActionDef(PauseXrLeft,  "PauseXrLeft",  CC::InputTrigger::XrSecondaryLeft));
    actionMap.RegisterAction(CC::ActionDef(PauseXrRight, "PauseXrRight", CC::InputTrigger::XrSecondaryRight));

    actionMap.SetContext("AppStateXrDemo");
}

void AppStateXrDemo::SceneInit()
{
    CC::SceneHierarchy::Get()->LoadFromFile("Data/Scenes/ExampleScene.yaml");

    CC::MaterialManager* materials = CC::MaterialManager::Get();
    materials->CreateMaterial("XrHand", "LitColour", "", CC::Vector3(0.7f, 0.75f, 0.9f));
    materials->CreateMaterial("XrRay",  "LitColour", "", CC::Vector3(0.3f, 0.9f, 1.0f));
    materials->CreateMaterial("XrGrabbable", "LitColour", "", CC::Vector3(0.9f, 0.5f, 0.2f));

    for (int hand = 0; hand < HAND_COUNT; hand++)
    {
        handObject[hand] = new CC::SceneObject(hand == 0 ? "XrHandLeft" : "XrHandRight");
        handObject[hand]->AddComponent(new CC::RenderableCube(materials->GetMaterial("XrHand")));
        handObject[hand]->GetTransform().SetScale(CC::Vector3(0.05f, 0.05f, 0.09f));
        CC::SceneHierarchy::Get()->AddRootObject(handObject[hand]);

        // A stretched cube rather than a line: there is no line renderable,
        // and at this thickness the difference is not visible.
        rayObject[hand] = new CC::SceneObject(hand == 0 ? "XrRayLeft" : "XrRayRight");
        rayObject[hand]->AddComponent(new CC::RenderableCube(materials->GetMaterial("XrRay")));
        CC::SceneHierarchy::Get()->AddRootObject(rayObject[hand]);
    }

    for (int i = 0; i < GRABBABLE_COUNT; i++)
    {
        grabbableHomePosition[i] = GRABBABLE_HOME[i];

        grabbableObject[i] = new CC::SceneObject(GRABBABLE_NAME[i]);
        grabbableObject[i]->AddComponent(new CC::RenderableSphere(materials->GetMaterial("XrGrabbable")));
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

    CC::SceneHierarchy::Get()->Init();
    CC::SceneHierarchy::Get()->SetEnabled(true);
}

void AppStateXrDemo::SceneShutdown()
{
    CC::SceneHierarchy::Get()->Shutdown();

    for (int hand = 0; hand < HAND_COUNT; hand++)
    {
        handObject[hand] = nullptr;
        rayObject[hand] = nullptr;
        heldObject[hand] = nullptr;
    }
    for (int i = 0; i < GRABBABLE_COUNT; i++)
    {
        grabbableObject[i] = nullptr;
    }
    panelObject = nullptr;

    // The component's Shutdown ran with the hierarchy and took its surface,
    // and the controller registered on that surface went with it.
    panelComponent = nullptr;
    panelController = nullptr;
}

void AppStateXrDemo::Shutdown()
{
    SceneShutdown();
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
        if (!isPaused)
        {
            ShowSceneScreen();
        }
        CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "AppStateXrDemo: xr session %s",
                isXrActive ? "started" : "ended");
    }

    UpdatePauseInput();

    // Hands and the world-space pointer stop with the world. The eye views
    // do not: they are published outside this call, so the headset keeps
    // tracking while the scene stands still.
    if (isXrActive && !isPaused)
    {
        UpdateHands();
        UpdateReadout();
    }
}

void AppStateXrDemo::UpdateHands()
{
    CC::XrManager* xr = CC::XrManager::Get();

    for (int hand = 0; hand < HAND_COUNT; hand++)
    {
        const CC::XrHand handEnum = (CC::XrHand)hand;
        const CC::TrackedPose& gripPose = xr->GetHandPose(handEnum, CC::XrPoseKind::Grip);
        const CC::TrackedPose& aimPose  = xr->GetHandPose(handEnum, CC::XrPoseKind::Aim);

        handObject[hand]->SetEnabled(gripPose.isTracked);
        rayObject[hand]->SetEnabled(aimPose.isTracked);

        if (gripPose.isTracked)
        {
            handObject[hand]->GetTransform().SetPosition(gripPose.position);
            handObject[hand]->GetTransform().SetRotationQuaternion(gripPose.orientation);
            UpdateGrab(handEnum, gripPose);
        }

        if (aimPose.isTracked)
        {
            UpdateRay(handEnum, aimPose);
            SubmitPanelPointer(handEnum, aimPose);
        }
    }
}

void AppStateXrDemo::UpdateRay(CC::XrHand hand, const CC::TrackedPose& aimPose)
{
    const int handIndex = (int)hand;

    // Forward is -Z in the pose's own frame, the same convention the eye
    // views use.
    CC::Vector3 forward = aimPose.orientation.RotateVector(CC::Vector3(0.0f, 0.0f, -1.0f));

    // The cube is centred on its origin, so the bar sits half its length
    // along the ray to start at the hand.
    CC::Vector3 aimPosition = aimPose.position;
    CC::Vector3 centre = aimPosition + forward * (RAY_LENGTH * 0.5f);

    rayObject[handIndex]->GetTransform().SetPosition(centre);
    rayObject[handIndex]->GetTransform().SetRotationQuaternion(aimPose.orientation);
    rayObject[handIndex]->GetTransform().SetScale(CC::Vector3(RAY_THICKNESS, RAY_THICKNESS, RAY_LENGTH));
}

void AppStateXrDemo::UpdateGrab(CC::XrHand hand, const CC::TrackedPose& gripPose)
{
    const int handIndex = (int)hand;
    const int grabAction = (hand == CC::XrHand::Left) ? Grab : GrabSecondary;

    CC::InputManager* input = CC::InputManager::Get();
    CC::XrManager* xr = CC::XrManager::Get();

    // Vector3's operators are not const-qualified, so the pose's vectors are
    // copied before use rather than read through the const reference.
    CC::Vector3 gripPosition = gripPose.position;

    if (input->EdgePositive(grabAction) && heldObject[handIndex] == nullptr)
    {
        // Nearest wins. Two objects within reach of one hand is common once
        // they have been piled up, and taking the first found would pick by
        // spawn order rather than by what the hand is closest to.
        CC::SceneObject* nearest = nullptr;
        float nearestDistanceSquared = GRAB_RADIUS * GRAB_RADIUS;

        for (int i = 0; i < GRABBABLE_COUNT; i++)
        {
            if (grabbableObject[i]->GetParent() == nullptr)
            {
                CC::Vector3 offset = grabbableObject[i]->GetWorldPosition() - gripPosition;
                float distanceSquared = offset.Dot(offset);
                if (distanceSquared < nearestDistanceSquared)
                {
                    nearestDistanceSquared = distanceSquared;
                    nearest = grabbableObject[i];
                }
            }
        }

        if (nearest != nullptr)
        {
            // Held in the hand's frame, so the object keeps the offset and
            // angle it was picked up at instead of snapping to the palm.
            CC::Quaternion inverseGrip = gripPose.orientation.Inverse();
            CC::Vector3 worldOffset = nearest->GetWorldPosition() - gripPosition;

            nearest->SetParent(handObject[handIndex]);
            nearest->GetTransform().SetPosition(inverseGrip.RotateVector(worldOffset));
            nearest->GetTransform().SetRotationQuaternion(
                inverseGrip * nearest->GetTransform().GetRotationQuaternion());

            heldObject[handIndex] = nearest;
            xr->TriggerHaptic(hand, HAPTIC_AMPLITUDE, HAPTIC_DURATION_SECONDS);
        }
    }

    if (input->EdgeNegative(grabAction) && heldObject[handIndex] != nullptr)
    {
        CC::SceneObject* released = heldObject[handIndex];

        // Put back where it visibly is, not where its local offset would put
        // it once the parent is gone.
        CC::Vector3 worldPosition = released->GetWorldPosition();
        CC::Quaternion worldRotation = gripPose.orientation
                                     * released->GetTransform().GetRotationQuaternion();

        released->SetParent(nullptr);
        released->GetTransform().SetPosition(worldPosition);
        released->GetTransform().SetRotationQuaternion(worldRotation);

        heldObject[handIndex] = nullptr;
        xr->TriggerHaptic(hand, HAPTIC_AMPLITUDE, HAPTIC_DURATION_SECONDS);
    }
}

// ========================
// Panel pointer
// ========================

void AppStateXrDemo::SubmitPanelPointer(CC::XrHand hand, const CC::TrackedPose& aimPose)
{
    const int handIndex = (int)hand;
    const int selectAction = (hand == CC::XrHand::Left) ? Select : SelectSecondary;

    // Vector3's arithmetic is not const-qualified, so the pose is copied
    // before use rather than read through the const reference.
    CC::Vector3 aimPosition = aimPose.position;
    CC::Vector3 forward = aimPose.orientation.RotateVector(CC::Vector3(0.0f, 0.0f, -1.0f));

    CC::UiManager::Get()->GetPointerRouter().SubmitRay(
        handIndex, aimPosition, forward, RAY_LENGTH,
        CC::InputManager::Get()->IsPressed(selectAction));

    // A pulse as the ray crosses onto a panel. Without it there is no way to
    // feel the edge, and the ray has no shadow to judge it by. What the
    // router decided last frame, because this frame's ray has not been
    // resolved yet.
    bool isOnPanel = panelComponent->IsPointerOn(handIndex);
    if (isOnPanel && !wasHoveringPanel[handIndex])
    {
        CC::XrManager::Get()->TriggerHaptic(hand, HAPTIC_AMPLITUDE, HAPTIC_DURATION_SECONDS);
    }
    wasHoveringPanel[handIndex] = isOnPanel;
}

void AppStateXrDemo::UpdatePauseInput()
{
    CC::InputManager* input = CC::InputManager::Get();

    bool isPausePressed = input->EdgePositive(Pause)
                       || input->EdgePositive(PauseXrLeft)
                       || input->EdgePositive(PauseXrRight);

    if (isPausePressed || pendingTogglePause)
    {
        pendingTogglePause = false;
        TogglePause();
    }
}

void AppStateXrDemo::TogglePause()
{
    isPaused = !isPaused;

    // The world stops; the camera does not. A scene that stops tracking the
    // head while the head moves is what makes people ill.
    CC::SceneHierarchy::Get()->SetEnabled(!isPaused);
    CC::InputManager::Get()->SetPaused(isPaused);

    if (isPaused)
    {
        // Only the window's screen changes. The panel in the world keeps its
        // own surface and carries on showing what it was showing.
        CC::UiManager::Get()->GetWindowSurface()->Screens().SetScreen("PauseMenu");
        CC::InputManager::Get()->LockMouseCursor(false);
    }
    else
    {
        ShowSceneScreen();
    }
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
                 heldObject[(int)CC::XrHand::Left] != nullptr ? 1 : 0,
                 heldObject[(int)CC::XrHand::Right] != nullptr ? 1 : 0);
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
        heldObject[hand] = nullptr;
        if (isXrActive)
        {
            CC::XrManager::Get()->TriggerHaptic((CC::XrHand)hand, HAPTIC_AMPLITUDE, HAPTIC_DURATION_SECONDS);
        }
    }

    CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "AppStateXrDemo: grabbables recentred");
}
