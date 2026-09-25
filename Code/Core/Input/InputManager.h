#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H

#include "PlatformInput.h"
#include "InputActionMap.h"

namespace CC
{
    class UiSurface;

    // A group of devices with one consumer at a time. Window is everything
    // that arrives through the platform: keyboard, mouse, gamepad, touch.
    // Headset is everything that arrives through the XR runtime's
    // controllers. Head tracking belongs to neither; it always drives the XR
    // camera.
    enum class InputDomain
    {
        Window = 0,
        Headset,
        Max
    };

    // What a domain's input goes to. Scene steers the world; Surface names
    // one UI surface that takes the domain's pointers and navigation.
    enum class InputTarget
    {
        Scene = 0,
        Surface
    };

    class InputManager
    {
    public:
        enum AnalogStick
        {
            STICK_LEFT,
            STICK_RIGHT,
            STICK_MAX
        };

        InputManager();
        virtual ~InputManager();

        static InputManager* Get();

        // Pump OS events into GLFW state. Must run BEFORE DevUi::StartFrame
        // so ImGui_ImplGlfw_NewFrame consumes fresh input. Update (action
        // map resolution + mouse state) then runs after ImGui::NewFrame so
        // its IsHovered / IsCapturingKeyboard queries see this-frame state.
        void PollPlatform();

        void Update();

        InputActionMap& GetActionMap() { return *actionMap; }

        bool IsPressed(int inputAction);
        bool EdgePositive(int inputAction);
        bool EdgeNegative(int inputAction);

        void LockMouseCursor(bool isLocked);
        bool IsMouseCursorLocked() const;
        void GetMousePosition(int& x, int& y) const;
        bool IsMouseButtonDown(MouseButton::Button button) const;
        void GetAnalogStickValues(AnalogStick stick, float& x, float& y);
        bool EdgePositiveMouseButton(bool isLeftButton);

        // Multi-touch passthrough. Slot 0 mirrors the mouse abstraction;
        // slots 1..MAX-1 are extra fingers (Android only).
        bool IsTouchPointerActive(int slot) const;
        void GetTouchPointer(int slot, int& x, int& y) const;

        ActiveInputType GetActiveInputType() const;

        // ========================
        // Tracked controller buttons
        // ========================
        //
        // Written by the XR subsystem each frame, after it syncs its actions
        // and before the action map resolves. Poses and axes are read from
        // XrManager directly; only the button half goes through the three
        // layers, because only the button half is remappable.

        void SetXrTriggerState(int trigger, bool isPressed);
        void NotifyXrActivity();

        // ========================
        // Joystick override (HUD on-screen joysticks)
        // Values written by the UI persist until overwritten. UI updates run
        // after scene/camera reads, so a value written on frame N is read by
        // scene/camera on frame N+1.
        // ========================

        void SetJoystickOverride(AnalogStick stick, float x, float y);
        void ClearJoystickOverride(AnalogStick stick);
        void ClearAllJoystickOverrides();

        // A value is only ever set by a UI control that holds the input, so
        // reading one is safe whatever the Window target is.
        bool IsJoystickOverrideSet(AnalogStick stick) const;

        // ========================
        // Input targets
        // ========================
        // Each domain has exactly one target, set by the AppState. The cursor
        // lock follows the Window target: a Surface target frees the cursor,
        // and a Scene target puts back what it was before.
        void SetInputTargetScene(InputDomain domain);
        void SetInputTargetSurface(InputDomain domain, UiSurface* surface);

        // Back to Scene for every domain, discarding the remembered cursor
        // lock. What an AppState swap does: the outgoing AppState's targets
        // mean nothing to the incoming one, which sets its own in Init.
        void ResetInputTargets();

        // The target in effect, which is the one requested unless an XR
        // session has taken the window over.
        InputTarget GetInputTarget(InputDomain domain) const;
        UiSurface* GetInputTargetSurface(InputDomain domain) const;

        // Whether the scene or a given surface takes this domain's input
        // right now. For the Window domain this also answers false while
        // input is blocked or the developer overlay has the mouse.
        bool DoesSceneReceiveInput(InputDomain domain) const;
        bool DoesSurfaceReceiveInput(InputDomain domain, const UiSurface* surface) const;

        // The surface the window draws to. The XR bridge below points the
        // Window target at it.
        void SetWindowSurface(UiSurface* surface);

        // While an XR session runs, the window is a mirror: its target is the
        // window surface whatever the AppState asked for, so a click only
        // focuses the window and never captures the cursor. Stands in for
        // AppStates learning when a session starts and stops.
        void SetXrOwnsInput(bool _doesXrOwnInput);
        bool DoesXrOwnInput() const { return doesXrOwnInput; }

        void SetInputBlocked(bool blocked);
        bool IsInputBlocked() const;

    private:
        static InputManager* instance;

        PlatformInput* physicalInput;
        InputActionMap* actionMap;

        bool mouseButtonCurrent[2];
        bool mouseButtonPrevious[2];

        // Mouse deflection is a displacement, not a stick position: pixels
        // moved since the last frame, capped per second so the total is
        // frame-rate independent. It can exceed 1.0 on a long frame.
        static constexpr int MAX_MOUSE_DEFLECTION = 100;
        static constexpr float MAX_MOUSE_DEFLECTION_PER_SECOND = 6000.0f;

        static constexpr float STICK_DEADZONE = 0.15f;

        float analogStickRightX;
        float analogStickRightY;
        bool useMouseAsRightStick;
        bool wasWindowFocused;

        float joystickOverrideX[STICK_MAX];
        float joystickOverrideY[STICK_MAX];
        bool joystickOverrideSet[STICK_MAX];

        struct RequestedInputTarget
        {
            InputTarget target = InputTarget::Scene;
            UiSurface*  surface = nullptr;
        };

        RequestedInputTarget requestedTargets[(int)InputDomain::Max];
        UiSurface* windowSurface = nullptr;
        bool doesXrOwnInput = false;
        bool isInputBlocked = false;

        // The Window target the cursor was last set for, and the lock it had
        // before a Surface target freed it.
        InputTarget appliedWindowTarget = InputTarget::Scene;
        bool wasMouseLockedBeforeSurface = false;

        bool IsWindowInputOpen() const;
        void ApplyWindowTargetToCursor();

        void UpdateMouseAsAnalogStick();
        void ResetMouseToCenter();
        float ApplyDeadzone(float value) const;
    };
}

#endif // INPUTMANAGER_H