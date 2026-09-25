#include "XrHands.h"

#include "ComponentFactory.h"
#include "InputManager.h"
#include "MaterialManager.h"
#include "RenderableCube.h"
#include "SceneObject.h"
#include "UiManager.h"
#include "XrGrabbable.h"
#include "XrManager.h"

namespace CC
{
    namespace
    {
        const char* const HAND_MATERIAL_NAME = "XrHand";
        const char* const RAY_MATERIAL_NAME  = "XrRay";

        const Vector3 HAND_VISUAL_SCALE(0.05f, 0.05f, 0.09f);
    }

    XrHands::XrHands()
    {
        for (int hand = 0; hand < HAND_COUNT; hand++)
        {
            handObject[hand] = nullptr;
            rayObject[hand] = nullptr;
            heldObject[hand] = nullptr;
            wasPointerOnTarget[hand] = false;
        }
    }

    XrHands::~XrHands()
    {
    }

    void XrHands::Init()
    {
        MaterialManager* materials = MaterialManager::Get();
        if (!materials->HasMaterial(HAND_MATERIAL_NAME))
        {
            materials->CreateMaterial(HAND_MATERIAL_NAME, "LitColour", "", Vector3(0.7f, 0.75f, 0.9f));
        }
        if (!materials->HasMaterial(RAY_MATERIAL_NAME))
        {
            materials->CreateMaterial(RAY_MATERIAL_NAME, "LitColour", "", Vector3(0.3f, 0.9f, 1.0f));
        }

        for (int hand = 0; hand < HAND_COUNT; hand++)
        {
            const bool isLeft = hand == (int)XrHand::Left;

            // The hand itself is unscaled, so an object held in it keeps its
            // own size. The visible cube is a scaled child.
            handObject[hand] = new SceneObject(isLeft ? "XrHandLeft" : "XrHandRight");
            owner->AddChild(handObject[hand]);

            SceneObject* handVisual = new SceneObject(isLeft ? "XrHandLeftVisual" : "XrHandRightVisual");
            handVisual->AddComponent(new RenderableCube(materials->GetMaterial(HAND_MATERIAL_NAME)));
            handVisual->GetTransform().SetScale(HAND_VISUAL_SCALE);
            handObject[hand]->AddChild(handVisual);

            // A stretched cube rather than a line: there is no line
            // renderable, and at this thickness the difference is not
            // visible.
            rayObject[hand] = new SceneObject(isLeft ? "XrRayLeft" : "XrRayRight");
            rayObject[hand]->AddComponent(new RenderableCube(materials->GetMaterial(RAY_MATERIAL_NAME)));
            owner->AddChild(rayObject[hand]);
        }

        SetHandsVisible(false);
    }

    void XrHands::Update()
    {
        XrManager* xr = XrManager::Get();
        bool isSessionRunning = xr != nullptr && xr->IsSessionRunning();

        bool isGrabLive = isSessionRunning
            && InputManager::Get()->GetInputTarget(InputDomain::Headset) == InputTarget::Scene;

        if (!isGrabLive)
        {
            ReleaseAll();
        }

        if (isSessionRunning)
        {
            for (int hand = 0; hand < HAND_COUNT; hand++)
            {
                UpdateHand((XrHand)hand, isGrabLive);
            }
        }
        else
        {
            SetHandsVisible(false);
        }
    }

    void XrHands::Shutdown()
    {
        ReleaseAll();

        // The hands are children of the owner and are deleted with it.
        for (int hand = 0; hand < HAND_COUNT; hand++)
        {
            handObject[hand] = nullptr;
            rayObject[hand] = nullptr;
        }
    }

    void XrHands::ReleaseAll()
    {
        for (int hand = 0; hand < HAND_COUNT; hand++)
        {
            Release((XrHand)hand);
        }
    }

    bool XrHands::IsHolding(XrHand hand) const
    {
        return heldObject[(int)hand] != nullptr;
    }

    // ========================
    // Private helpers
    // ========================
    void XrHands::UpdateHand(XrHand hand, bool isGrabLive)
    {
        const int handIndex = (int)hand;
        XrManager* xr = XrManager::Get();

        const TrackedPose& gripPose = xr->GetHandPose(hand, XrPoseKind::Grip);
        const TrackedPose& aimPose  = xr->GetHandPose(hand, XrPoseKind::Aim);

        handObject[handIndex]->SetEnabled(gripPose.isTracked);
        rayObject[handIndex]->SetEnabled(aimPose.isTracked);

        if (gripPose.isTracked)
        {
            handObject[handIndex]->GetTransform().SetPosition(gripPose.position);
            handObject[handIndex]->GetTransform().SetRotationQuaternion(gripPose.orientation);

            if (isGrabLive)
            {
                UpdateGrab(hand, gripPose);
            }
        }

        if (aimPose.isTracked)
        {
            UpdateRay(hand, aimPose);
            SubmitPointer(hand, aimPose);
        }
    }

    void XrHands::UpdateRay(XrHand hand, const TrackedPose& aimPose)
    {
        const int handIndex = (int)hand;

        // Forward is -Z in the pose's own frame, the same convention the eye
        // views use.
        Vector3 forward = aimPose.orientation.RotateVector(Vector3(0.0f, 0.0f, -1.0f));

        // The cube is centred on its origin, so the bar sits half its length
        // along the ray to start at the hand.
        Vector3 aimPosition = aimPose.position;
        Vector3 centre = aimPosition + forward * (RAY_LENGTH * 0.5f);

        rayObject[handIndex]->GetTransform().SetPosition(centre);
        rayObject[handIndex]->GetTransform().SetRotationQuaternion(aimPose.orientation);
        rayObject[handIndex]->GetTransform().SetScale(Vector3(RAY_THICKNESS, RAY_THICKNESS, RAY_LENGTH));
    }

    void XrHands::SubmitPointer(XrHand hand, const TrackedPose& aimPose)
    {
        const int handIndex = (int)hand;
        const int selectAction = hand == XrHand::Left ? InputAction::XrSelectLeft : InputAction::XrSelectRight;

        // Vector3's arithmetic is not const-qualified, so the pose is copied
        // before use rather than read through the const reference.
        Vector3 aimPosition = aimPose.position;
        Vector3 forward = aimPose.orientation.RotateVector(Vector3(0.0f, 0.0f, -1.0f));

        UiPointerRouter& router = UiManager::Get()->GetPointerRouter();
        router.SubmitRay(handIndex, aimPosition, forward, RAY_LENGTH,
                         InputManager::Get()->IsPressed(selectAction));

        // A pulse as the ray crosses onto a target. Without it there is no
        // way to feel the edge, and the ray has no shadow to judge it by.
        // What the router decided last frame, because this frame's ray has
        // not been resolved yet.
        bool isOnTarget = router.IsPointerOnTarget(handIndex);
        if (isOnTarget && !wasPointerOnTarget[handIndex])
        {
            XrManager::Get()->TriggerHaptic(hand, HAPTIC_AMPLITUDE, HAPTIC_DURATION_SECONDS);
        }
        wasPointerOnTarget[handIndex] = isOnTarget;
    }

    void XrHands::UpdateGrab(XrHand hand, const TrackedPose& gripPose)
    {
        const int handIndex = (int)hand;
        const int grabAction = hand == XrHand::Left ? InputAction::XrGrabLeft : InputAction::XrGrabRight;

        InputManager* input = InputManager::Get();

        // Vector3's operators are not const-qualified, so the pose's vectors
        // are copied before use rather than read through the const reference.
        Vector3 gripPosition = gripPose.position;

        if (input->EdgePositive(grabAction) && heldObject[handIndex] == nullptr)
        {
            // Nearest wins. Two objects within reach of one hand is common
            // once they have been piled up, and taking the first found would
            // pick by registration order rather than by what the hand is
            // closest to. Only unparented objects: one already in a hand, or
            // placed under another object, is not free to take.
            SceneObject* nearest = nullptr;
            float nearestDistanceSquared = GRAB_RADIUS * GRAB_RADIUS;

            for (XrGrabbable* grabbable : XrGrabbable::GetRegistered())
            {
                SceneObject* candidate = grabbable->GetOwner();
                if (candidate != nullptr && candidate->GetParent() == nullptr && candidate->IsEnabled())
                {
                    Vector3 offset = candidate->GetWorldPosition() - gripPosition;
                    float distanceSquared = offset.Dot(offset);
                    if (distanceSquared < nearestDistanceSquared)
                    {
                        nearestDistanceSquared = distanceSquared;
                        nearest = candidate;
                    }
                }
            }

            if (nearest != nullptr)
            {
                // Held in the hand's frame, so the object keeps the offset
                // and angle it was picked up at instead of snapping to the
                // palm.
                Quaternion inverseGrip = gripPose.orientation.Inverse();
                Vector3 worldOffset = nearest->GetWorldPosition() - gripPosition;

                nearest->SetParent(handObject[handIndex]);
                nearest->GetTransform().SetPosition(inverseGrip.RotateVector(worldOffset));
                nearest->GetTransform().SetRotationQuaternion(
                    inverseGrip * nearest->GetTransform().GetRotationQuaternion());

                heldObject[handIndex] = nearest;
                XrManager::Get()->TriggerHaptic(hand, HAPTIC_AMPLITUDE, HAPTIC_DURATION_SECONDS);
            }
        }

        if (input->EdgeNegative(grabAction) && heldObject[handIndex] != nullptr)
        {
            Release(hand);
            XrManager::Get()->TriggerHaptic(hand, HAPTIC_AMPLITUDE, HAPTIC_DURATION_SECONDS);
        }
    }

    void XrHands::Release(XrHand hand)
    {
        const int handIndex = (int)hand;
        SceneObject* released = heldObject[handIndex];

        if (released != nullptr && handObject[handIndex] != nullptr)
        {
            // Put back where it visibly is, not where its local offset would
            // put it once the parent is gone.
            Vector3 worldPosition = released->GetWorldPosition();
            Quaternion worldRotation = handObject[handIndex]->GetTransform().GetRotationQuaternion()
                                     * released->GetTransform().GetRotationQuaternion();

            released->SetParent(nullptr);
            released->GetTransform().SetPosition(worldPosition);
            released->GetTransform().SetRotationQuaternion(worldRotation);
        }

        heldObject[handIndex] = nullptr;
    }

    void XrHands::SetHandsVisible(bool isVisible)
    {
        for (int hand = 0; hand < HAND_COUNT; hand++)
        {
            if (handObject[hand] != nullptr)
            {
                handObject[hand]->SetEnabled(isVisible);
            }
            if (rayObject[hand] != nullptr)
            {
                rayObject[hand]->SetEnabled(isVisible);
            }
        }
    }
}

// ========================
// Scene file registration
// ========================
static CC::Component* CreateXrHands(ryml::ConstNodeRef componentData)
{
    (void)componentData;
    return new CC::XrHands();
}

static CC::ComponentRegistrar xrHandsRegistrar("XrHands", CreateXrHands);
