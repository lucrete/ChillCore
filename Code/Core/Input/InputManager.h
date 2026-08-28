#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H

#include "PlatformInput.h"
#include "InputActionMap.h"

namespace CC
{
    enum class InteractionMode
    {
        Ui,
        World
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
        // Joystick override (HUD on-screen joysticks)
        // Values written by the UI persist until overwritten. UI updates run
        // after scene/camera reads, so a value written on frame N is read by
        // scene/camera on frame N+1.
        // ========================

        void SetJoystickOverride(AnalogStick stick, float x, float y);
        void ClearJoystickOverride(AnalogStick stick);
        void ClearAllJoystickOverrides();

        // ========================
        // Input routing
        // ========================

        void SetInteractionMode(InteractionMode mode);
        bool IsUiInteractable() const;
        bool IsHudInteractable() const;
        bool IsWorldInteractable() const;

        void SetInputBlocked(bool blocked);
        bool IsInputBlocked() const;

    private:
        static InputManager* instance;

        PlatformInput* physicalInput;
        InputActionMap* actionMap;

        bool mouseButtonCurrent[2];
        bool mouseButtonPrevious[2];

        static constexpr int MAX_MOUSE_DEFLECTION = 100;
        static constexpr float STICK_DEADZONE = 0.15f;

        float analogStickRightX;
        float analogStickRightY;
        bool useMouseAsRightStick;

        float joystickOverrideX[STICK_MAX];
        float joystickOverrideY[STICK_MAX];
        bool joystickOverrideSet[STICK_MAX];

        InteractionMode interactionMode = InteractionMode::World;
        bool isInputBlocked = false;

        void UpdateMouseAsAnalogStick();
        void ResetMouseToCenter();
        float ApplyDeadzone(float value) const;
    };
}

#endif // INPUTMANAGER_H