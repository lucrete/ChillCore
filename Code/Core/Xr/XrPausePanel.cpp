#include "XrPausePanel.h"

#include <cmath>

#include "CCAssert.h"
#include "CCQuaternion.h"
#include "SceneObject.h"
#include "UiWorldPanel.h"
#include "XrManager.h"

namespace CC
{
    XrPausePanel::XrPausePanel()
    {
    }

    XrPausePanel::~XrPausePanel()
    {
    }

    void XrPausePanel::Init()
    {
        CC_ASSERT(owner != nullptr, "XrPausePanel needs an owner");

        panel = owner->GetComponent<UiWorldPanel>();
        CC_ASSERT(panel != nullptr, "XrPausePanel needs a UiWorldPanel on the same owner");

        Hide();
    }

    void XrPausePanel::Show()
    {
        PlaceInFrontOfHead();
        owner->SetEnabled(true);
        panel->SetEnabled(true);
    }

    // The owner goes too: the panel's quad is a sibling component and would
    // otherwise keep drawing.
    void XrPausePanel::Hide()
    {
        owner->SetEnabled(false);
        panel->SetEnabled(false);
    }

    UiSurface* XrPausePanel::GetSurface() const
    {
        return panel != nullptr ? panel->GetSurface() : nullptr;
    }

    // ========================
    // Private helpers
    // ========================
    void XrPausePanel::PlaceInFrontOfHead()
    {
        XrManager* xr = XrManager::Get();
        TrackedPose head;
        if (xr != nullptr)
        {
            head = xr->GetHeadPose();
        }

        if (head.isTracked)
        {
            // Along the head's yaw only, so looking down when pausing does
            // not put the panel on the floor or tilt it.
            Vector3 forward = head.orientation.RotateVector(Vector3(0.0f, 0.0f, -1.0f));
            forward.y = 0.0f;
            if (forward.Magnitude() < 0.001f)
            {
                forward = Vector3(0.0f, 0.0f, -1.0f);
            }
            forward.Normalize();

            Vector3 headPosition = head.position;
            owner->GetTransform().SetPosition(headPosition + forward * DISTANCE_FROM_HEAD_METRES);

            // An unrotated panel faces +Z. Turning it about Y until +Z points
            // back along the forward direction faces it at the player.
            float yaw = atan2f(-forward.x, -forward.z);
            owner->GetTransform().SetRotationQuaternion(Quaternion::FromAxisAngle(Vector3(0.0f, 1.0f, 0.0f), yaw));
        }
    }
}
