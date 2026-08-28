#include "InputManager.h"
#include "PlatformWindow.h"
#include "DevUi.h"
#include "CCAssert.h"
#include "CCMath.h"
#include <memory.h>
#include <cmath>

#ifdef __ANDROID__
    #include "PlatformInputAndroid.h"
#else
    #include "PlatformInputGlfw.h"
#endif

namespace CC
{
    InputManager* InputManager::instance = nullptr;

    InputManager::InputManager()
        : analogStickRightX(0.0f)
        , analogStickRightY(0.0f)
        , useMouseAsRightStick(true)
    {
        CC_ASSERT(instance == nullptr, "InputManager already created");
        instance = this;

        memset(mouseButtonCurrent, 0, sizeof(mouseButtonCurrent));
        memset(mouseButtonPrevious, 0, sizeof(mouseButtonPrevious));
        memset(joystickOverrideX, 0, sizeof(joystickOverrideX));
        memset(joystickOverrideY, 0, sizeof(joystickOverrideY));
        memset(joystickOverrideSet, 0, sizeof(joystickOverrideSet));

#ifdef __ANDROID__
        physicalInput = new PlatformInputAndroid();
#else
        physicalInput = new PlatformInputGlfw();
#endif
        actionMap = new InputActionMap(physicalInput);
        actionMap->CreateContext("Default");
        actionMap->SetContext("Default");
    }

    InputManager::~InputManager()
    {
        delete actionMap;
        delete physicalInput;
        instance = nullptr;
    }

    InputManager* InputManager::Get()
    {
        CC_ASSERT(instance != nullptr, "InputManager not created yet");
        return instance;
    }

    void InputManager::PollPlatform()
    {
        PlatformWindow::Get()->PollEvents();
    }

    void InputManager::Update()
    {
        // Detect touch activity at the platform level. Two paths because
        // each catches a case the other misses:
        //  - Slot-active poll: fires every frame a finger is held, even
        //    if no down/move/up event landed in the buffer this frame.
        //    Necessary because slot 0 is also the mouse abstraction on
        //    Android, so a steady finger contact would otherwise let
        //    DetectMouseActivity read mouse-button-down as keyboard
        //    activity.
        //  - Event latch: fires for any down/move/up/cancel that landed
        //    since the last update, including taps whose down + up both
        //    arrived in the same frame's input drain. The slot-active
        //    poll alone misses those because the slot is already
        //    inactive by the time poll runs, but DetectMouseActivity
        //    still sees the position change and would otherwise flip
        //    activeInputType to KeyboardMouse.
        for (int slot = 0; slot < PlatformInput::MAX_TOUCH_POINTERS; slot++)
        {
            if (physicalInput->IsTouchPointerActive(slot))
            {
                actionMap->NotifyTouchActivity();
            }
        }
        if (physicalInput->ConsumeTouchActivityLatch())
        {
            actionMap->NotifyTouchActivity();
        }

        actionMap->Update();

        // Update mouse buttons
        mouseButtonPrevious[0] = mouseButtonCurrent[0];
        mouseButtonPrevious[1] = mouseButtonCurrent[1];
        mouseButtonCurrent[0] = physicalInput->IsMouseButtonDown(MouseButton::Left);
        mouseButtonCurrent[1] = physicalInput->IsMouseButtonDown(MouseButton::Right);

        if (physicalInput->IsMouseCursorLocked())
        {
            UpdateMouseAsAnalogStick();
        }
    }

    bool InputManager::IsPressed(int inputAction)
    {
        bool result = !isInputBlocked && actionMap->IsPressed(inputAction);
        return result;
    }

    bool InputManager::EdgePositive(int inputAction)
    {
        bool result = !isInputBlocked && actionMap->EdgePositive(inputAction);
        return result;
    }

    bool InputManager::EdgeNegative(int inputAction)
    {
        bool result = !isInputBlocked && actionMap->EdgeNegative(inputAction);
        return result;
    }

    void InputManager::LockMouseCursor(bool isLocked)
    {
        physicalInput->SetMouseCursorLocked(isLocked);
        analogStickRightX = 0.0f;
        analogStickRightY = 0.0f;
        ResetMouseToCenter();
    }

    bool InputManager::IsMouseCursorLocked() const
    {
        return physicalInput->IsMouseCursorLocked();
    }

    void InputManager::GetMousePosition(int& x, int& y) const
    {
        physicalInput->GetMousePosition(x, y);
    }

    bool InputManager::IsMouseButtonDown(MouseButton::Button button) const
    {
        return physicalInput->IsMouseButtonDown(button);
    }

    void InputManager::GetAnalogStickValues(AnalogStick stick, float& x, float& y)
    {
        if (joystickOverrideSet[stick])
        {
            x = joystickOverrideX[stick];
            y = joystickOverrideY[stick];
        }
        else
        {
            ActiveInputType inputType = actionMap->GetActiveInputType();

            if (inputType == ActiveInputType::Gamepad)
            {
                const GamepadState& state = physicalInput->GetGamepadState();

                if (stick == STICK_LEFT)
                {
                    x = ApplyDeadzone(state.axes[GamepadAxis::LeftStickX]);
                    y = ApplyDeadzone(-state.axes[GamepadAxis::LeftStickY]);
                }
                else
                {
                    x = ApplyDeadzone(state.axes[GamepadAxis::RightStickX]);
                    y = ApplyDeadzone(-state.axes[GamepadAxis::RightStickY]);
                }
            }
            else
            {
                if (stick == STICK_RIGHT)
                {
                    x = analogStickRightX;
                    y = analogStickRightY;
                }
                else
                {
                    x = 0.0f;
                    y = 0.0f;
                }
            }
        }
    }

    // ========================
    // Joystick override
    // ========================

    void InputManager::SetJoystickOverride(AnalogStick stick, float x, float y)
    {
        CC_ASSERT(stick >= 0 && stick < STICK_MAX, "Invalid stick");
        joystickOverrideX[stick] = x;
        joystickOverrideY[stick] = y;
        joystickOverrideSet[stick] = true;

        // Treat real stick deflection as touch activity. The (0, 0) snap on
        // release shouldn't flip the input type back to Touch, so skip it.
        if (x != 0.0f || y != 0.0f)
        {
            actionMap->NotifyTouchActivity();
        }
    }

    void InputManager::ClearJoystickOverride(AnalogStick stick)
    {
        CC_ASSERT(stick >= 0 && stick < STICK_MAX, "Invalid stick");
        joystickOverrideX[stick] = 0.0f;
        joystickOverrideY[stick] = 0.0f;
        joystickOverrideSet[stick] = false;
    }

    void InputManager::ClearAllJoystickOverrides()
    {
        for (int i = 0; i < STICK_MAX; i++)
        {
            joystickOverrideX[i] = 0.0f;
            joystickOverrideY[i] = 0.0f;
            joystickOverrideSet[i] = false;
        }
    }

    bool InputManager::EdgePositiveMouseButton(bool isLeftButton)
    {
        int index = isLeftButton ? 0 : 1;
        return mouseButtonCurrent[index] && !mouseButtonPrevious[index];
    }

    bool InputManager::IsTouchPointerActive(int slot) const
    {
        return physicalInput->IsTouchPointerActive(slot);
    }

    void InputManager::GetTouchPointer(int slot, int& x, int& y) const
    {
        physicalInput->GetTouchPointer(slot, x, y);
    }

    void InputManager::UpdateMouseAsAnalogStick()
    {
        if (useMouseAsRightStick)
        {
            int screenWidth = 0;
            int screenHeight = 0;
            PlatformWindow::Get()->GetFramebufferSize(screenWidth, screenHeight);

            int centerX = screenWidth >> 1;
            int centerY = screenHeight >> 1;

            int currentX = 0;
            int currentY = 0;
            physicalInput->GetMousePosition(currentX, currentY);

            int offsetX = centerX - currentX;
            int offsetY = centerY - currentY;

            analogStickRightX = Math::Clamp(-1.0f, 1.0f, static_cast<float>(offsetX) / MAX_MOUSE_DEFLECTION);
            analogStickRightY = Math::Clamp(-1.0f, 1.0f, static_cast<float>(offsetY) / MAX_MOUSE_DEFLECTION);

            physicalInput->SetMousePosition(centerX, centerY);
        }
    }

    void InputManager::ResetMouseToCenter()
    {
        int screenWidth = 0;
        int screenHeight = 0;
        PlatformWindow::Get()->GetFramebufferSize(screenWidth, screenHeight);

        int centerX = screenWidth >> 1;
        int centerY = screenHeight >> 1;

        physicalInput->SetMousePosition(centerX, centerY);
    }

    float InputManager::ApplyDeadzone(float value) const
    {
        float result = 0.0f;
        if (std::abs(value) >= STICK_DEADZONE)
        {
            float sign = value > 0.0f ? 1.0f : -1.0f;
            float magnitude = std::abs(value);
            result = sign * (magnitude - STICK_DEADZONE) / (1.0f - STICK_DEADZONE);
        }
        return result;
    }

    ActiveInputType InputManager::GetActiveInputType() const
    {
        return actionMap->GetActiveInputType();
    }

    // ========================
    // Input routing
    // ========================

    void InputManager::SetInteractionMode(InteractionMode mode)
    {
        interactionMode = mode;
    }

    bool InputManager::IsUiInteractable() const
    {
        bool result = !isInputBlocked && !DevUi::Get()->IsHovered() && interactionMode == InteractionMode::Ui;
        return result;
    }

    bool InputManager::IsHudInteractable() const
    {
        bool result = !isInputBlocked && !DevUi::Get()->IsHovered();
        return result;
    }

    bool InputManager::IsWorldInteractable() const
    {
        bool result = !isInputBlocked && !DevUi::Get()->IsHovered() && interactionMode == InteractionMode::World;
        return result;
    }

    void InputManager::SetInputBlocked(bool blocked)
    {
        isInputBlocked = blocked;
    }

    bool InputManager::IsInputBlocked() const
    {
        return isInputBlocked;
    }
}