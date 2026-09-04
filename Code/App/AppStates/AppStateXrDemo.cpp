#include "AppStateXrDemo.h"

#include "CameraManager.h"
#include "CoreMain.h"
#include "GfxRenderApi.h"
#include "InputManager.h"
#include "MaterialManager.h"
#include "PrintManager.h"
#include "RenderManager.h"
#include "RenderableCube.h"
#include "RenderableQuad.h"
#include "RenderableSphere.h"
#include "SceneHierarchy.h"
#include "StateMachine.h"
#include "UiManager.h"
#include "UiElement.h"
#include "UiScreenSystem.h"
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
    readoutElement = CC::UiManager::Get()->GetElementById("readout");

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
        CC::UiManager::Get()->InvalidateLayout();
    }
}

AppStateXrDemo::AppStateXrDemo()
    : panelObject(nullptr)
    , panelController(nullptr)
    , panelPosition(PANEL_WORLD_POSITION)
    , isXrActive(false)
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

    panelController = new XrPanelController();
    CC::UiScreenSystem::Get()->RegisterScreen("XrPanel", "Data/Ui/XrPanel.html", "Data/Ui/XrPanel.css",
                                              panelController);
    CC::UiScreenSystem::Get()->SetScreen("XrPanel");

    CC::UiManager::Get()->RegisterButtonAction("xrRecentre", [this]() { RecentreGrabbables(); });
    CC::UiManager::Get()->RegisterButtonAction("xrExit", []() { CC::StateMachine::Get()->GotoState("Boot"); });
    CC::UiManager::Get()->RegisterButtonAction("xrCyclePreset", []() {});

    CreatePanelTarget();

    // The panel is flat and axis-aligned, so its basis is fixed. A panel that
    // could be moved would rebuild these whenever it did.
    panelNormal = CC::Vector3(0.0f, 0.0f, 1.0f);
    panelRight  = CC::Vector3(1.0f, 0.0f, 0.0f);
    panelUp     = CC::Vector3(0.0f, 1.0f, 0.0f);

    CC::InputManager::Get()->SetInteractionMode(CC::InteractionMode::World);

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
    actionMap.RegisterAction(CC::ActionDef(OpenPanel,       "OpenPanel",       CC::InputTrigger::XrMenu));

    actionMap.SetContext("AppStateXrDemo");
}

void AppStateXrDemo::SceneInit()
{
    CC::SceneHierarchy::Get()->LoadFromFile("Data/Scenes/ExampleScene.yaml");

    CC::MaterialManager* materials = CC::MaterialManager::Get();
    materials->CreateMaterial("XrHand", "LitColour", "", CC::Vector3(0.7f, 0.75f, 0.9f));
    materials->CreateMaterial("XrRay",  "LitColour", "", CC::Vector3(0.3f, 0.9f, 1.0f));
    materials->CreateMaterial("XrGrabbable", "LitColour", "", CC::Vector3(0.9f, 0.5f, 0.2f));
    materials->CreateMaterial("XrPanelSurface", "TextureShader");

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

    panelObject = new CC::SceneObject("XrPanel");
    panelObject->AddComponent(new CC::RenderableQuad(materials->GetMaterial("XrPanelSurface")));
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
}

void AppStateXrDemo::Shutdown()
{
    CC::UiManager::Get()->GetInputHandler().ClearPointerOverride();
    DestroyPanelTarget();
    SceneShutdown();
}

// ========================
// Panel target
// ========================

void AppStateXrDemo::CreatePanelTarget()
{
    CC::Gfx::RenderApi* gfxApi = CC::Gfx::RenderApi::Get();

    CC::Gfx::TextureDescription colorDesc;
    colorDesc.width          = PANEL_TEXTURE_WIDTH;
    colorDesc.height         = PANEL_TEXTURE_HEIGHT;
    colorDesc.format         = CC::Gfx::TextureFormat::Rgba8Unorm;
    colorDesc.isRenderTarget = true;
    colorDesc.debugName      = "AppStateXrDemo::PanelColor";
    panelColorTexture = gfxApi->CreateTexture(colorDesc);

    CC::Gfx::RenderTargetDescription targetDesc;
    targetDesc.width                       = PANEL_TEXTURE_WIDTH;
    targetDesc.height                      = PANEL_TEXTURE_HEIGHT;
    targetDesc.colorAttachmentCount        = 1;
    targetDesc.colorAttachments[0].texture = panelColorTexture;
    targetDesc.colorAttachments[0].loadOp  = CC::Gfx::LoadOp::Clear;
    targetDesc.colorAttachments[0].storeOp = CC::Gfx::StoreOp::Store;
    targetDesc.hasDepthStencil             = false;
    targetDesc.sampleCount                 = 1;
    targetDesc.debugName                   = "AppStateXrDemo::PanelTarget";
    panelTarget = gfxApi->CreateRenderTarget(targetDesc);

    CC::MaterialManager::Get()->GetMaterial("XrPanelSurface")->SetTextureHandleOverride(panelColorTexture);
}

void AppStateXrDemo::DestroyPanelTarget()
{
    CC::Gfx::RenderApi* gfxApi = CC::Gfx::RenderApi::Get();

    if (panelTarget.IsValid())
    {
        gfxApi->DestroyRenderTarget(panelTarget);
        panelTarget = CC::Gfx::RenderTargetHandle();
    }
    if (panelColorTexture.IsValid())
    {
        gfxApi->DestroyTexture(panelColorTexture);
        panelColorTexture = CC::Gfx::TextureHandle();
    }
}

// ========================
// Frame
// ========================

void AppStateXrDemo::Update()
{
    isXrActive = CC::XrManager::Get() != nullptr && CC::XrManager::Get()->IsSessionRunning();

    if (isXrActive)
    {
        UpdateHands();
        UpdatePanelPointer();
    }
    UpdateReadout();

    // Runs here rather than with the rest of the UI because the panel is a
    // texture the scene samples, and the scene draws before the frame's UI
    // pass. Its content is therefore one frame behind the pointer, which at
    // headset refresh is not perceptible.
    CC::UiManager::Get()->RenderToTarget(panelTarget, PANEL_TEXTURE_WIDTH, PANEL_TEXTURE_HEIGHT);
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

bool AppStateXrDemo::IntersectPanel(const CC::TrackedPose& aimPose, float& outPixelX, float& outPixelY) const
{
    bool isHit = false;

    CC::Vector3 forward = aimPose.orientation.RotateVector(CC::Vector3(0.0f, 0.0f, -1.0f));
    CC::Vector3 aimPosition = aimPose.position;

    CC::Vector3 planeNormal = panelNormal;
    CC::Vector3 planeOrigin = panelPosition;
    CC::Vector3 planeRight  = panelRight;
    CC::Vector3 planeUp     = panelUp;

    float denominator = forward.Dot(planeNormal);

    // Near zero means the ray runs along the panel rather than into it, and
    // the intersection is either nowhere or everywhere.
    if (denominator < -0.0001f || denominator > 0.0001f)
    {
        CC::Vector3 toPanel = planeOrigin - aimPosition;
        float distance = toPanel.Dot(planeNormal) / denominator;

        if (distance > 0.0f && distance <= RAY_LENGTH)
        {
            CC::Vector3 hit = aimPosition + forward * distance;
            CC::Vector3 local = hit - planeOrigin;

            float halfWidth = PANEL_WIDTH * 0.5f;
            float halfHeight = PANEL_HEIGHT * 0.5f;
            float localX = local.Dot(planeRight);
            float localY = local.Dot(planeUp);

            if (localX >= -halfWidth && localX <= halfWidth
             && localY >= -halfHeight && localY <= halfHeight)
            {
                // Panel space is centred and Y-up; the UI is corner-origin
                // and Y-down.
                outPixelX = ((localX + halfWidth) / PANEL_WIDTH) * (float)PANEL_TEXTURE_WIDTH;
                outPixelY = (1.0f - ((localY + halfHeight) / PANEL_HEIGHT)) * (float)PANEL_TEXTURE_HEIGHT;
                isHit = true;
            }
        }
    }

    return isHit;
}

void AppStateXrDemo::UpdatePanelPointer()
{
    CC::XrManager* xr = CC::XrManager::Get();
    CC::InputManager* input = CC::InputManager::Get();

    CC::UiPointerState pointer;

    // One pointer, whichever hand is on the panel. The right hand wins a tie
    // only because something has to; both hands driving one cursor at once
    // reads as a fight.
    for (int hand = 0; hand < HAND_COUNT; hand++)
    {
        const CC::XrHand handEnum = (CC::XrHand)hand;
        const CC::TrackedPose& aimPose = xr->GetHandPose(handEnum, CC::XrPoseKind::Aim);

        float pixelX = 0.0f;
        float pixelY = 0.0f;
        bool isOnPanel = aimPose.isTracked && IntersectPanel(aimPose, pixelX, pixelY);

        if (isOnPanel)
        {
            const int selectAction = (handEnum == CC::XrHand::Left) ? Select : SelectSecondary;

            pointer.x        = pixelX;
            pointer.y        = pixelY;
            pointer.isDown   = input->IsPressed(selectAction);
            pointer.isActive = true;
        }

        // A pulse as the ray crosses onto the panel. Without it there is no
        // way to feel the edge, and the ray has no shadow to judge it by.
        if (isOnPanel && !wasHoveringPanel[hand])
        {
            xr->TriggerHaptic(handEnum, HAPTIC_AMPLITUDE, HAPTIC_DURATION_SECONDS);
        }
        wasHoveringPanel[hand] = isOnPanel;
    }

    CC::UiManager::Get()->GetInputHandler().SetPointerOverride(pointer);
}

void AppStateXrDemo::UpdateReadout()
{
    std::string readout;

    if (!isXrActive)
    {
        readout = "No XR session - running flat";
    }
    else if (heldObject[(int)CC::XrHand::Left] != nullptr
          || heldObject[(int)CC::XrHand::Right] != nullptr)
    {
        readout = "Holding an object";
    }
    else if (wasHoveringPanel[(int)CC::XrHand::Left] || wasHoveringPanel[(int)CC::XrHand::Right])
    {
        readout = "Trigger to press";
    }
    else
    {
        readout = "Squeeze near a sphere to pick it up";
    }

    if (readout != lastReadout)
    {
        lastReadout = readout;
        panelController->SetReadout(readout);
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
    }
}
