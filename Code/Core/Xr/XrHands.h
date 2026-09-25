#ifndef XRHANDS_H
#define XRHANDS_H

#include "Component.h"
#include "XrTypes.h"

namespace CC
{
    class SceneObject;

    // The engine's tracked hands: a visible hand and aim ray for each
    // controller, pointing at interface objects, and grabbing registered
    // grabbables. Added to one scene object in an XR scene; everything else
    // follows from the session and the Headset input target.
    //
    // Never pauses. Rays are offered to the pointer router every frame,
    // because the pause panel is pointed at while the world is frozen. Grab
    // is live only while the Headset target is the scene, and anything held
    // is let go the moment it stops being.
    class XrHands : public Component
    {
    public:
        XrHands();
        virtual ~XrHands();

        virtual const char* GetTypeName() const override { return "XrHands"; }

        virtual void Init() override;
        virtual void Update() override;
        virtual void Shutdown() override;

        // Lets go of anything held, where it visibly is.
        void ReleaseAll();
        bool IsHolding(XrHand hand) const;

    private:
        static const int HAND_COUNT = (int)XrHand::Max;

        // How close the grip has to be to pick something up. Generous: the
        // grip pose sits inside the fist, so a hand that looks like it is
        // touching an object is already some way inside it.
        static constexpr float GRAB_RADIUS = 0.25f;

        static constexpr float RAY_LENGTH = 3.0f;
        static constexpr float RAY_THICKNESS = 0.006f;

        static constexpr float HAPTIC_AMPLITUDE = 0.4f;
        static constexpr float HAPTIC_DURATION_SECONDS = 0.03f;

        SceneObject* handObject[HAND_COUNT];
        SceneObject* rayObject[HAND_COUNT];
        SceneObject* heldObject[HAND_COUNT];
        bool         wasPointerOnTarget[HAND_COUNT];

        void UpdateHand(XrHand hand, bool isGrabLive);
        void UpdateRay(XrHand hand, const TrackedPose& aimPose);
        void SubmitPointer(XrHand hand, const TrackedPose& aimPose);
        void UpdateGrab(XrHand hand, const TrackedPose& gripPose);
        void Release(XrHand hand);
        void SetHandsVisible(bool isVisible);
    };
}

#endif // XRHANDS_H
