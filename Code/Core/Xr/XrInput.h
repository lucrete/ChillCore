#ifndef XRINPUT_H
#define XRINPUT_H

#include <openxr/openxr.h>

#include "XrTypes.h"

namespace CC
{
    // ========================
    // XrInput
    // ========================
    //
    // The OpenXR action set: one action per thing the application asks about,
    // each bound to both hands through a subaction path, plus the suggested
    // bindings that tell the runtime which physical control serves which
    // action on each controller it knows.
    //
    // This header names OpenXR types and is included only from inside
    // Core/Xr. Everything the engine reads is republished by XrManager in
    // engine types.
    //
    // Buttons leave here through the three input layers, because they are
    // remappable and an application should bind an action rather than a
    // control. Poses and axes do not: there is no sense in which a hand
    // position is remappable, and the action map has no value type for one.

    class XrInput
    {
    public:
        XrInput();
        ~XrInput();

        bool Init(XrInstance instance, XrSession session);
        void Shutdown();

        // Reads this frame's action state and pushes the button half into the
        // input system. predictedDisplayTime is the same one the views were
        // located against, so hands and head agree.
        void SyncActions(XrSpace baseSpace, XrTime predictedDisplayTime);

        const TrackedPose& GetHandPose(XrHand hand, XrPoseKind kind) const;
        void  GetThumbstick(XrHand hand, float& outX, float& outY) const;

        // Actions only carry input while the session is focused. Visible but
        // unfocused still locates poses, so hands and rays keep working while
        // every button and axis reads zero — which looks exactly like a
        // binding that never arrived.
        bool IsFocused() const { return isFocused; }
        float GetTriggerValue(XrHand hand) const;
        float GetSqueezeValue(XrHand hand) const;

        void TriggerHaptic(XrHand hand, float amplitude, float durationSeconds);

        // Logs which interaction profile the runtime actually bound per hand,
        // and whether each action came back bound. A profile the runtime
        // declined leaves poses working through the simple controller while
        // squeeze and thumbstick silently do not exist, which is otherwise
        // indistinguishable from an action that is bound but never pressed.
        void LogActiveProfile();

    private:
        static const int HAND_COUNT = (int)XrHand::Max;
        static const int POSE_KIND_COUNT = (int)XrPoseKind::Max;

        // A value below this reads as released. Analogue triggers rest a
        // little above zero and drift, so a bare non-zero test chatters.
        static constexpr float BUTTON_PRESS_THRESHOLD = 0.5f;

        // Below this a stick is treated as centred. Sticks do not return to
        // exactly zero, and a scene reading the raw value drifts.
        static constexpr float THUMBSTICK_DEAD_ZONE = 0.15f;

        // How far a thumbstick has to be pushed before it reads as a
        // navigation direction. Well clear of the dead zone, so a resting
        // thumb does not walk a menu selection.
        static constexpr float THUMBSTICK_DIRECTION_THRESHOLD = 0.5f;

        XrInstance instance;
        XrSession  session;

        bool isFocused = false;

        XrActionSet actionSet;
        XrPath      handPath[HAND_COUNT];

        XrAction gripPoseAction;
        XrAction aimPoseAction;
        XrAction triggerAction;
        XrAction squeezeAction;
        XrAction thumbstickAction;
        XrAction thumbstickClickAction;
        XrAction primaryClickAction;
        XrAction secondaryClickAction;
        XrAction menuClickAction;
        XrAction hapticAction;

        XrSpace poseSpace[HAND_COUNT][POSE_KIND_COUNT];

        TrackedPose handPose[HAND_COUNT][POSE_KIND_COUNT];
        float       thumbstickX[HAND_COUNT];
        float       thumbstickY[HAND_COUNT];
        float       triggerValue[HAND_COUNT];
        float       squeezeValue[HAND_COUNT];

        bool CreateActions();
        bool SuggestBindings();
        bool CreateActionSpaces();
        bool AttachActionSet();

        // Suggests one profile's bindings. A profile the runtime does not
        // know is not an error: it declines, and the profiles it does know
        // still apply.
        void SuggestProfile(const char* profilePath, const char* const* bindingPaths, int bindingCount,
                            const XrAction* bindingActions);

        void ReadFloat(XrAction action, XrHand hand, float& outValue);
        void ReadBoolean(XrAction action, XrHand hand, int trigger);
        void ReadVector2(XrAction action, XrHand hand, float& outX, float& outY);
        void ReadPose(XrHand hand, XrPoseKind kind, XrSpace baseSpace, XrTime predictedDisplayTime);

        // Either hand's stick past the threshold, as hand-neutral direction
        // triggers.
        void PushThumbstickDirections();
    };
}

#endif // XRINPUT_H
