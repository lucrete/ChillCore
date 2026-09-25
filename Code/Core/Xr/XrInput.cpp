#include "XrInput.h"

#include <string.h>
#include <vector>

#include "CCAssert.h"
#include "InputManager.h"
#include "InputTriggerMap.h"
#include "PrintManager.h"

namespace CC
{
    namespace
    {
        bool XrInputSucceeded(XrResult result, const char* what)
        {
            bool isSuccess = XR_SUCCEEDED(result);
            if (!isSuccess)
            {
                CCPrint(PrintManager::CHANNEL_RENDER, "OpenXR input: %s failed (%d)", what, (int)result);
            }
            return isSuccess;
        }

        // Every binding table below lists the left hand's paths then the
        // right hand's, in the same order as the action array beside it.
        const int BINDINGS_PER_HAND = 9;
    }

    XrInput::XrInput()
        : instance(XR_NULL_HANDLE)
        , session(XR_NULL_HANDLE)
        , actionSet(XR_NULL_HANDLE)
        , gripPoseAction(XR_NULL_HANDLE)
        , aimPoseAction(XR_NULL_HANDLE)
        , triggerAction(XR_NULL_HANDLE)
        , squeezeAction(XR_NULL_HANDLE)
        , thumbstickAction(XR_NULL_HANDLE)
        , thumbstickClickAction(XR_NULL_HANDLE)
        , primaryClickAction(XR_NULL_HANDLE)
        , secondaryClickAction(XR_NULL_HANDLE)
        , menuClickAction(XR_NULL_HANDLE)
        , hapticAction(XR_NULL_HANDLE)
    {
        memset(handPath, 0, sizeof(handPath));
        memset(poseSpace, 0, sizeof(poseSpace));
        memset(thumbstickX, 0, sizeof(thumbstickX));
        memset(thumbstickY, 0, sizeof(thumbstickY));
        memset(triggerValue, 0, sizeof(triggerValue));
        memset(squeezeValue, 0, sizeof(squeezeValue));
    }

    XrInput::~XrInput()
    {
        Shutdown();
    }

    // ========================
    // Setup
    // ========================
    bool XrInput::Init(XrInstance _instance, XrSession _session)
    {
        instance = _instance;
        session  = _session;

        bool isReady = XrInputSucceeded(xrStringToPath(instance, "/user/hand/left",
                                                       &handPath[(int)XrHand::Left]),
                                        "xrStringToPath(left)")
                    && XrInputSucceeded(xrStringToPath(instance, "/user/hand/right",
                                                       &handPath[(int)XrHand::Right]),
                                        "xrStringToPath(right)");

        if (isReady)
        {
            XrActionSetCreateInfo actionSetInfo = { XR_TYPE_ACTION_SET_CREATE_INFO };
            strcpy_s(actionSetInfo.actionSetName, "gameplay");
            strcpy_s(actionSetInfo.localizedActionSetName, "Gameplay");
            actionSetInfo.priority = 0;
            isReady = XrInputSucceeded(xrCreateActionSet(instance, &actionSetInfo, &actionSet),
                                       "xrCreateActionSet");
        }

        isReady = isReady && CreateActions();
        isReady = isReady && SuggestBindings();
        isReady = isReady && CreateActionSpaces();
        isReady = isReady && AttachActionSet();

        return isReady;
    }

    void XrInput::Shutdown()
    {
        for (int hand = 0; hand < HAND_COUNT; hand++)
        {
            for (int kind = 0; kind < POSE_KIND_COUNT; kind++)
            {
                if (poseSpace[hand][kind] != XR_NULL_HANDLE)
                {
                    xrDestroySpace(poseSpace[hand][kind]);
                    poseSpace[hand][kind] = XR_NULL_HANDLE;
                }
            }
        }

        if (actionSet != XR_NULL_HANDLE)
        {
            // Destroying the set destroys every action created in it.
            xrDestroyActionSet(actionSet);
            actionSet = XR_NULL_HANDLE;
        }
    }

    bool XrInput::CreateActions()
    {
        struct ActionDefinition
        {
            XrAction*      action;
            XrActionType   type;
            const char*    name;
            const char*    localizedName;
        };

        const ActionDefinition definitions[] =
        {
            { &gripPoseAction,        XR_ACTION_TYPE_POSE_INPUT,     "grip_pose",        "Grip Pose" },
            { &aimPoseAction,         XR_ACTION_TYPE_POSE_INPUT,     "aim_pose",         "Aim Pose" },
            { &triggerAction,         XR_ACTION_TYPE_FLOAT_INPUT,    "trigger",          "Trigger" },
            { &squeezeAction,         XR_ACTION_TYPE_FLOAT_INPUT,    "squeeze",          "Squeeze" },
            { &thumbstickAction,      XR_ACTION_TYPE_VECTOR2F_INPUT, "thumbstick",       "Thumbstick" },
            { &thumbstickClickAction, XR_ACTION_TYPE_BOOLEAN_INPUT,  "thumbstick_click", "Thumbstick Click" },
            { &primaryClickAction,    XR_ACTION_TYPE_BOOLEAN_INPUT,  "primary_click",    "Primary Button" },
            { &secondaryClickAction,  XR_ACTION_TYPE_BOOLEAN_INPUT,  "secondary_click",  "Secondary Button" },
            { &menuClickAction,       XR_ACTION_TYPE_BOOLEAN_INPUT,  "menu_click",       "Menu" },
            { &hapticAction,          XR_ACTION_TYPE_VIBRATION_OUTPUT, "haptic",         "Haptic Feedback" },
        };

        bool isReady = true;
        const int definitionCount = (int)(sizeof(definitions) / sizeof(definitions[0]));

        for (int i = 0; i < definitionCount && isReady; i++)
        {
            // Every action is declared for both hands through subaction
            // paths, so one action serves both and the reads pick a hand.
            XrActionCreateInfo actionInfo = { XR_TYPE_ACTION_CREATE_INFO };
            actionInfo.actionType = definitions[i].type;
            strcpy_s(actionInfo.actionName, definitions[i].name);
            strcpy_s(actionInfo.localizedActionName, definitions[i].localizedName);
            actionInfo.countSubactionPaths = HAND_COUNT;
            actionInfo.subactionPaths      = handPath;

            isReady = XrInputSucceeded(xrCreateAction(actionSet, &actionInfo, definitions[i].action),
                                       "xrCreateAction");
        }

        return isReady;
    }

    void XrInput::SuggestProfile(const char* profilePath, const char* const* bindingPaths, int bindingCount,
                                 const XrAction* bindingActions)
    {
        XrPath interactionProfile = XR_NULL_PATH;
        if (XR_FAILED(xrStringToPath(instance, profilePath, &interactionProfile)))
        {
            CCPrint(PrintManager::CHANNEL_RENDER, "OpenXR input: bad profile path %s", profilePath);
        }
        else
        {
            std::vector<XrActionSuggestedBinding> suggested;
            suggested.reserve(bindingCount);

            for (int i = 0; i < bindingCount; i++)
            {
                XrPath bindingPath = XR_NULL_PATH;
                if (bindingPaths[i] != nullptr
                    && XR_SUCCEEDED(xrStringToPath(instance, bindingPaths[i], &bindingPath)))
                {
                    XrActionSuggestedBinding entry;
                    entry.action  = bindingActions[i];
                    entry.binding = bindingPath;
                    suggested.push_back(entry);
                }
            }

            XrInteractionProfileSuggestedBinding suggestedBinding = { XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
            suggestedBinding.interactionProfile     = interactionProfile;
            suggestedBinding.suggestedBindings      = suggested.data();
            suggestedBinding.countSuggestedBindings = (uint32_t)suggested.size();

            // A runtime that does not know this controller declines the whole
            // profile. That is expected, not a failure: the profiles it does
            // know still apply.
            if (XR_FAILED(xrSuggestInteractionProfileBindings(instance, &suggestedBinding)))
            {
                CCPrint(PrintManager::CHANNEL_RENDER, "OpenXR input: runtime declined %s", profilePath);
            }
        }
    }

    bool XrInput::SuggestBindings()
    {
        // One action per column, left hand then right. Kept parallel so a
        // profile is read as a table rather than as a list of pairs.
        const XrAction actionsPerHand[BINDINGS_PER_HAND * 2] =
        {
            gripPoseAction, aimPoseAction, triggerAction, squeezeAction, thumbstickAction,
            thumbstickClickAction, primaryClickAction, secondaryClickAction, hapticAction,

            gripPoseAction, aimPoseAction, triggerAction, squeezeAction, thumbstickAction,
            thumbstickClickAction, primaryClickAction, secondaryClickAction, hapticAction,
        };

        // Touch: X/Y on the left, A/B on the right, so primary and secondary
        // mean different letters per hand.
        const char* const touchBindings[BINDINGS_PER_HAND * 2] =
        {
            "/user/hand/left/input/grip/pose",
            "/user/hand/left/input/aim/pose",
            "/user/hand/left/input/trigger/value",
            "/user/hand/left/input/squeeze/value",
            "/user/hand/left/input/thumbstick",
            "/user/hand/left/input/thumbstick/click",
            "/user/hand/left/input/x/click",
            "/user/hand/left/input/y/click",
            "/user/hand/left/output/haptic",

            "/user/hand/right/input/grip/pose",
            "/user/hand/right/input/aim/pose",
            "/user/hand/right/input/trigger/value",
            "/user/hand/right/input/squeeze/value",
            "/user/hand/right/input/thumbstick",
            "/user/hand/right/input/thumbstick/click",
            "/user/hand/right/input/a/click",
            "/user/hand/right/input/b/click",
            "/user/hand/right/output/haptic",
        };

        // Menu rides along in the same table. A second suggest call for a
        // profile replaces its bindings rather than adding to them, so every
        // binding a profile has must go in one call.
        const XrAction touchActions[BINDINGS_PER_HAND * 2 + 1] =
        {
            gripPoseAction, aimPoseAction, triggerAction, squeezeAction, thumbstickAction,
            thumbstickClickAction, primaryClickAction, secondaryClickAction, hapticAction,

            gripPoseAction, aimPoseAction, triggerAction, squeezeAction, thumbstickAction,
            thumbstickClickAction, primaryClickAction, secondaryClickAction, hapticAction,

            menuClickAction,
        };
        const char* const touchBindingsWithMenu[BINDINGS_PER_HAND * 2 + 1] =
        {
            touchBindings[0], touchBindings[1], touchBindings[2], touchBindings[3], touchBindings[4],
            touchBindings[5], touchBindings[6], touchBindings[7], touchBindings[8],

            touchBindings[9],  touchBindings[10], touchBindings[11], touchBindings[12], touchBindings[13],
            touchBindings[14], touchBindings[15], touchBindings[16], touchBindings[17],

            // Touch carries menu on the left hand only; the right-hand system
            // button is reserved by the runtime.
            "/user/hand/left/input/menu/click",
        };

        // Index: A/B on both hands, and squeeze reports a force as well as a
        // value. The value is the portable one.
        const char* const indexBindings[BINDINGS_PER_HAND * 2] =
        {
            "/user/hand/left/input/grip/pose",
            "/user/hand/left/input/aim/pose",
            "/user/hand/left/input/trigger/value",
            "/user/hand/left/input/squeeze/value",
            "/user/hand/left/input/thumbstick",
            "/user/hand/left/input/thumbstick/click",
            "/user/hand/left/input/a/click",
            "/user/hand/left/input/b/click",
            "/user/hand/left/output/haptic",

            "/user/hand/right/input/grip/pose",
            "/user/hand/right/input/aim/pose",
            "/user/hand/right/input/trigger/value",
            "/user/hand/right/input/squeeze/value",
            "/user/hand/right/input/thumbstick",
            "/user/hand/right/input/thumbstick/click",
            "/user/hand/right/input/a/click",
            "/user/hand/right/input/b/click",
            "/user/hand/right/output/haptic",
        };

        SuggestProfile("/interaction_profiles/oculus/touch_controller",
                       touchBindingsWithMenu, BINDINGS_PER_HAND * 2 + 1, touchActions);
        SuggestProfile("/interaction_profiles/valve/index_controller",
                       indexBindings, BINDINGS_PER_HAND * 2, actionsPerHand);

        // The simple controller is the fallback every runtime must support.
        // It has a pose, one button and a menu, and nothing else — enough
        // that an unknown controller still points and selects.
        const XrAction simpleActions[] =
        {
            gripPoseAction, aimPoseAction, primaryClickAction, menuClickAction, hapticAction,
            gripPoseAction, aimPoseAction, primaryClickAction, menuClickAction, hapticAction,
        };
        const char* const simpleBindings[] =
        {
            "/user/hand/left/input/grip/pose",
            "/user/hand/left/input/aim/pose",
            "/user/hand/left/input/select/click",
            "/user/hand/left/input/menu/click",
            "/user/hand/left/output/haptic",

            "/user/hand/right/input/grip/pose",
            "/user/hand/right/input/aim/pose",
            "/user/hand/right/input/select/click",
            "/user/hand/right/input/menu/click",
            "/user/hand/right/output/haptic",
        };
        SuggestProfile("/interaction_profiles/khr/simple_controller",
                       simpleBindings, (int)(sizeof(simpleBindings) / sizeof(simpleBindings[0])),
                       simpleActions);

        return true;
    }

    bool XrInput::CreateActionSpaces()
    {
        bool isReady = true;

        for (int hand = 0; hand < HAND_COUNT && isReady; hand++)
        {
            for (int kind = 0; kind < POSE_KIND_COUNT && isReady; kind++)
            {
                XrActionSpaceCreateInfo spaceInfo = { XR_TYPE_ACTION_SPACE_CREATE_INFO };
                spaceInfo.action            = (kind == (int)XrPoseKind::Grip) ? gripPoseAction : aimPoseAction;
                spaceInfo.subactionPath     = handPath[hand];
                spaceInfo.poseInActionSpace.orientation.w = 1.0f;

                isReady = XrInputSucceeded(xrCreateActionSpace(session, &spaceInfo, &poseSpace[hand][kind]),
                                           "xrCreateActionSpace");
            }
        }

        return isReady;
    }

    bool XrInput::AttachActionSet()
    {
        XrSessionActionSetsAttachInfo attachInfo = { XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
        attachInfo.countActionSets = 1;
        attachInfo.actionSets      = &actionSet;

        // Attaching freezes the set: no action can be created or rebound
        // afterwards, for the life of the session.
        return XrInputSucceeded(xrAttachSessionActionSets(session, &attachInfo),
                                "xrAttachSessionActionSets");
    }

    // ========================
    // Frame
    // ========================
    void XrInput::SyncActions(XrSpace baseSpace, XrTime predictedDisplayTime)
    {
        XrActiveActionSet activeActionSet;
        activeActionSet.actionSet     = actionSet;
        activeActionSet.subactionPath = XR_NULL_PATH;

        XrActionsSyncInfo syncInfo = { XR_TYPE_ACTIONS_SYNC_INFO };
        syncInfo.countActiveActionSets = 1;
        syncInfo.activeActionSets      = &activeActionSet;

        // XR_SESSION_NOT_FOCUSED is a success code, not a failure, so a bare
        // XR_SUCCEEDED test walks straight past it and reads actions that the
        // runtime has already said are inactive. Every value then reads zero
        // for a reason nothing reports.
        XrResult syncResult = xrSyncActions(session, &syncInfo);

        const bool wasFocused = isFocused;
        isFocused = syncResult == XR_SUCCESS;

        if (isFocused != wasFocused)
        {
            CCPrint(PrintManager::CHANNEL_RENDER, "OpenXR input: session %s (sync %d)",
                    isFocused ? "focused, actions live" : "NOT FOCUSED, actions inactive",
                    (int)syncResult);
        }

        if (isFocused)
        {
            for (int hand = 0; hand < HAND_COUNT; hand++)
            {
                const XrHand handEnum = (XrHand)hand;
                const bool isLeft = handEnum == XrHand::Left;

                ReadFloat(triggerAction, handEnum, triggerValue[hand]);
                ReadFloat(squeezeAction, handEnum, squeezeValue[hand]);
                ReadVector2(thumbstickAction, handEnum, thumbstickX[hand], thumbstickY[hand]);

                // An analogue pull is a button once it passes the threshold,
                // so the action map can bind it like any other.
                InputManager::Get()->SetXrTriggerState(
                    isLeft ? InputTrigger::XrTriggerLeft : InputTrigger::XrTriggerRight,
                    triggerValue[hand] >= BUTTON_PRESS_THRESHOLD);
                InputManager::Get()->SetXrTriggerState(
                    isLeft ? InputTrigger::XrSqueezeLeft : InputTrigger::XrSqueezeRight,
                    squeezeValue[hand] >= BUTTON_PRESS_THRESHOLD);

                ReadBoolean(thumbstickClickAction, handEnum,
                            isLeft ? InputTrigger::XrThumbstickClickLeft : InputTrigger::XrThumbstickClickRight);
                ReadBoolean(primaryClickAction, handEnum,
                            isLeft ? InputTrigger::XrPrimaryLeft : InputTrigger::XrPrimaryRight);
                ReadBoolean(secondaryClickAction, handEnum,
                            isLeft ? InputTrigger::XrSecondaryLeft : InputTrigger::XrSecondaryRight);

                if (isLeft)
                {
                    ReadBoolean(menuClickAction, handEnum, InputTrigger::XrMenu);
                }

                ReadPose(handEnum, XrPoseKind::Grip, baseSpace, predictedDisplayTime);
                ReadPose(handEnum, XrPoseKind::Aim,  baseSpace, predictedDisplayTime);

                // Only a deliberate input counts as activity. A tracked hand
                // is not one: signalling on tracking alone pins the active
                // input type to Xr for the life of the session, and the
                // keyboard can then never win it back.
                const bool isStickDeflected = (thumbstickX[hand] * thumbstickX[hand]
                                             + thumbstickY[hand] * thumbstickY[hand])
                                            > (THUMBSTICK_DEAD_ZONE * THUMBSTICK_DEAD_ZONE);
                const bool isPulled = triggerValue[hand] >= BUTTON_PRESS_THRESHOLD
                                   || squeezeValue[hand] >= BUTTON_PRESS_THRESHOLD;
                if (isStickDeflected || isPulled)
                {
                    InputManager::Get()->NotifyXrActivity();
                }
            }

            PushThumbstickDirections();
        }
    }

    void XrInput::PushThumbstickDirections()
    {
        bool isUp    = false;
        bool isDown  = false;
        bool isLeft  = false;
        bool isRight = false;

        for (int hand = 0; hand < HAND_COUNT; hand++)
        {
            isUp    = isUp    || thumbstickY[hand] >=  THUMBSTICK_DIRECTION_THRESHOLD;
            isDown  = isDown  || thumbstickY[hand] <= -THUMBSTICK_DIRECTION_THRESHOLD;
            isLeft  = isLeft  || thumbstickX[hand] <= -THUMBSTICK_DIRECTION_THRESHOLD;
            isRight = isRight || thumbstickX[hand] >=  THUMBSTICK_DIRECTION_THRESHOLD;
        }

        InputManager::Get()->SetXrTriggerState(InputTrigger::XrThumbstickUp,    isUp);
        InputManager::Get()->SetXrTriggerState(InputTrigger::XrThumbstickDown,  isDown);
        InputManager::Get()->SetXrTriggerState(InputTrigger::XrThumbstickLeft,  isLeft);
        InputManager::Get()->SetXrTriggerState(InputTrigger::XrThumbstickRight, isRight);
    }

    void XrInput::ReadFloat(XrAction action, XrHand hand, float& outValue)
    {
        XrActionStateGetInfo getInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getInfo.action        = action;
        getInfo.subactionPath = handPath[(int)hand];

        XrActionStateFloat actionState = { XR_TYPE_ACTION_STATE_FLOAT };
        if (XR_SUCCEEDED(xrGetActionStateFloat(session, &getInfo, &actionState)) && actionState.isActive)
        {
            outValue = actionState.currentState;
        }
        else
        {
            outValue = 0.0f;
        }
    }

    void XrInput::ReadBoolean(XrAction action, XrHand hand, int trigger)
    {
        XrActionStateGetInfo getInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getInfo.action        = action;
        getInfo.subactionPath = handPath[(int)hand];

        XrActionStateBoolean actionState = { XR_TYPE_ACTION_STATE_BOOLEAN };
        bool isPressed = false;
        if (XR_SUCCEEDED(xrGetActionStateBoolean(session, &getInfo, &actionState)) && actionState.isActive)
        {
            isPressed = actionState.currentState == XR_TRUE;
        }

        InputManager::Get()->SetXrTriggerState(trigger, isPressed);
    }

    void XrInput::ReadVector2(XrAction action, XrHand hand, float& outX, float& outY)
    {
        XrActionStateGetInfo getInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
        getInfo.action        = action;
        getInfo.subactionPath = handPath[(int)hand];

        XrActionStateVector2f actionState = { XR_TYPE_ACTION_STATE_VECTOR2F };
        if (XR_SUCCEEDED(xrGetActionStateVector2f(session, &getInfo, &actionState)) && actionState.isActive)
        {
            outX = actionState.currentState.x;
            outY = actionState.currentState.y;
        }
        else
        {
            outX = 0.0f;
            outY = 0.0f;
        }
    }

    void XrInput::ReadPose(XrHand hand, XrPoseKind kind, XrSpace baseSpace, XrTime predictedDisplayTime)
    {
        TrackedPose& pose = handPose[(int)hand][(int)kind];

        XrSpaceLocation location = { XR_TYPE_SPACE_LOCATION };
        XrResult result = xrLocateSpace(poseSpace[(int)hand][(int)kind], baseSpace,
                                        predictedDisplayTime, &location);

        const XrSpaceLocationFlags requiredFlags = XR_SPACE_LOCATION_POSITION_VALID_BIT
                                                 | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;

        // A pose that stops being valid keeps its last value rather than
        // snapping to the origin, and says so through isTracked. A controller
        // put down mid-frame otherwise throws whatever it holds to the floor.
        if (XR_SUCCEEDED(result) && (location.locationFlags & requiredFlags) == requiredFlags)
        {
            pose.position    = Vector3(location.pose.position.x,
                                       location.pose.position.y,
                                       location.pose.position.z);
            pose.orientation = Quaternion(location.pose.orientation.x,
                                          location.pose.orientation.y,
                                          location.pose.orientation.z,
                                          location.pose.orientation.w);
            pose.isTracked   = true;
        }
        else
        {
            pose.isTracked = false;
        }
    }

    // ========================
    // Queries
    // ========================
    const TrackedPose& XrInput::GetHandPose(XrHand hand, XrPoseKind kind) const
    {
        CC_ASSERT(hand < XrHand::Max && kind < XrPoseKind::Max, "GetHandPose: out of range");
        return handPose[(int)hand][(int)kind];
    }

    void XrInput::GetThumbstick(XrHand hand, float& outX, float& outY) const
    {
        CC_ASSERT(hand < XrHand::Max, "GetThumbstick: hand out of range");

        const float x = thumbstickX[(int)hand];
        const float y = thumbstickY[(int)hand];

        // Zeroed as a pair, not per axis: killing one axis alone bends a
        // diagonal push towards the surviving axis near the centre.
        if ((x * x + y * y) > (THUMBSTICK_DEAD_ZONE * THUMBSTICK_DEAD_ZONE))
        {
            outX = x;
            outY = y;
        }
        else
        {
            outX = 0.0f;
            outY = 0.0f;
        }
    }

    float XrInput::GetTriggerValue(XrHand hand) const
    {
        CC_ASSERT(hand < XrHand::Max, "GetTriggerValue: hand out of range");
        return triggerValue[(int)hand];
    }

    float XrInput::GetSqueezeValue(XrHand hand) const
    {
        CC_ASSERT(hand < XrHand::Max, "GetSqueezeValue: hand out of range");
        return squeezeValue[(int)hand];
    }

    void XrInput::LogActiveProfile()
    {
        for (int hand = 0; hand < HAND_COUNT; hand++)
        {
            const char* handName = (hand == (int)XrHand::Left) ? "left" : "right";

            XrInteractionProfileState profileState = { XR_TYPE_INTERACTION_PROFILE_STATE };
            if (XR_SUCCEEDED(xrGetCurrentInteractionProfile(session, handPath[hand], &profileState)))
            {
                char pathText[XR_MAX_PATH_LENGTH] = {};
                uint32_t pathLength = 0;

                if (profileState.interactionProfile == XR_NULL_PATH)
                {
                    CCPrint(PrintManager::CHANNEL_RENDER, "OpenXR input: %s hand has no profile", handName);
                }
                else if (XR_SUCCEEDED(xrPathToString(instance, profileState.interactionProfile,
                                                     sizeof(pathText), &pathLength, pathText)))
                {
                    CCPrint(PrintManager::CHANNEL_RENDER, "OpenXR input: %s hand profile %s", handName, pathText);
                }
            }

            // isActive is the runtime saying this action resolved to a real
            // control on the bound profile.
            struct NamedAction { XrAction action; const char* name; };
            const NamedAction actions[] =
            {
                { triggerAction,         "trigger" },
                { squeezeAction,         "squeeze" },
                { thumbstickClickAction, "thumbstickClick" },
                { primaryClickAction,    "primary" },
                { secondaryClickAction,  "secondary" },
                { menuClickAction,       "menu" },
            };

            for (int i = 0; i < (int)(sizeof(actions) / sizeof(actions[0])); i++)
            {
                XrActionStateGetInfo getInfo = { XR_TYPE_ACTION_STATE_GET_INFO };
                getInfo.action        = actions[i].action;
                getInfo.subactionPath = handPath[hand];

                bool isActive = false;
                if (actions[i].action == triggerAction || actions[i].action == squeezeAction)
                {
                    XrActionStateFloat actionState = { XR_TYPE_ACTION_STATE_FLOAT };
                    isActive = XR_SUCCEEDED(xrGetActionStateFloat(session, &getInfo, &actionState))
                            && actionState.isActive;
                }
                else
                {
                    XrActionStateBoolean actionState = { XR_TYPE_ACTION_STATE_BOOLEAN };
                    isActive = XR_SUCCEEDED(xrGetActionStateBoolean(session, &getInfo, &actionState))
                            && actionState.isActive;
                }

                CCPrint(PrintManager::CHANNEL_RENDER, "OpenXR input: %s %s %s",
                        handName, actions[i].name, isActive ? "bound" : "UNBOUND");
            }
        }
    }

    void XrInput::TriggerHaptic(XrHand hand, float amplitude, float durationSeconds)
    {
        CC_ASSERT(hand < XrHand::Max, "TriggerHaptic: hand out of range");

        XrHapticVibration vibration = { XR_TYPE_HAPTIC_VIBRATION };
        vibration.amplitude = amplitude;
        vibration.duration  = (XrDuration)(durationSeconds * 1000000000.0);
        vibration.frequency = XR_FREQUENCY_UNSPECIFIED;

        XrHapticActionInfo hapticInfo = { XR_TYPE_HAPTIC_ACTION_INFO };
        hapticInfo.action        = hapticAction;
        hapticInfo.subactionPath = handPath[(int)hand];

        xrApplyHapticFeedback(session, &hapticInfo, (const XrHapticBaseHeader*)&vibration);
    }
}
