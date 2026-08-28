#include "InputTriggerMap.h"
#include "CCAssert.h"
#include "DevUi.h"
#include <memory.h>

namespace CC
{
    InputTriggerMap::InputTriggerMap(PlatformInput* physicalInput)
        : physicalInput(physicalInput)
#ifdef __ANDROID__
        , triggersCurrent(gamepadTriggersCurrent)
        , activeInputType(ActiveInputType::Touch)
#else
        , triggersCurrent(keyboardTriggersCurrent)
        , activeInputType(ActiveInputType::KeyboardMouse)
#endif
        , touchActivityPending(false)
        , lastMouseX(0)
        , lastMouseY(0)
    {
        memset(keyboardTriggersCurrent, 0, sizeof(keyboardTriggersCurrent));
        memset(gamepadTriggersCurrent, 0, sizeof(gamepadTriggersCurrent));
        memset(triggersPrevious, 0, sizeof(triggersPrevious));

        // Dev keyboard bindings
        RegisterKeyboardTrigger(InputTrigger::DevF1, "KeyboardF1", { { KeyCode::F1, "F1" } });
        RegisterKeyboardTrigger(InputTrigger::DevF2, "KeyboardF2", { { KeyCode::F2, "F2" } });
        RegisterKeyboardTrigger(InputTrigger::DevF3, "KeyboardF3", { { KeyCode::F3, "F3" } });
        RegisterKeyboardTrigger(InputTrigger::DevF4, "KeyboardF4", { { KeyCode::F4, "F4" } });
        RegisterKeyboardTrigger(InputTrigger::DevGraveAccent, "KeyboardGraveAccent", { { KeyCode::GraveAccent, "`" } });
        RegisterKeyboardTrigger(InputTrigger::DevCtrlF5, "KeyboardCtrlF5", { { ANY_CTRL, "Ctrl" }, { KeyCode::F5, "F5" } });
        RegisterKeyboardTrigger(InputTrigger::DevF9, "KeyboardF9", { { KeyCode::F9, "F9" } });
        RegisterKeyboardTrigger(InputTrigger::DevF10, "KeyboardF10", { { KeyCode::F10, "F10" } });

        // Gamepad keyboard bindings (keyboard mapped to gamepad-style triggers)
        RegisterKeyboardTrigger(InputTrigger::GamepadDpadLeft, "GamepadDpadLeft", { { KeyCode::A, "A" } });
        RegisterKeyboardTrigger(InputTrigger::GamepadDpadUp, "GamepadDpadUp", { { KeyCode::W, "W" } });
        RegisterKeyboardTrigger(InputTrigger::GamepadDpadRight, "GamepadDpadRight", { { KeyCode::D, "D" } });
        RegisterKeyboardTrigger(InputTrigger::GamepadDpadDown, "GamepadDpadDown", { { KeyCode::S, "S" } });
        RegisterKeyboardTrigger(InputTrigger::GamepadFaceLeft, "GamepadFaceLeft", { { KeyCode::RightAlt, "RAlt" } });
        RegisterKeyboardTrigger(InputTrigger::GamepadFaceUp, "GamepadFaceUp", { { KeyCode::Enter, "Enter" } });
        RegisterKeyboardTrigger(InputTrigger::GamepadFaceRight, "GamepadFaceRight", { { KeyCode::RightControl, "RCtrl" } });
        RegisterKeyboardTrigger(InputTrigger::GamepadFaceDown, "GamepadFaceDown", { { KeyCode::RightShift, "RShift" } });
        RegisterKeyboardTrigger(InputTrigger::GamepadBumperLeft, "GamepadBumperLeft", { { KeyCode::Q, "Q" } });
        RegisterKeyboardTrigger(InputTrigger::GamepadBumperRight, "GamepadBumperRight", { { KeyCode::E, "E" } });
        RegisterKeyboardTrigger(InputTrigger::GamepadTriggerLeft, "GamepadTriggerLeft", { { KeyCode::F7, "F7" } });
        RegisterKeyboardTrigger(InputTrigger::GamepadTriggerRight, "GamepadTriggerRight", { { KeyCode::F8, "F8" } });
        RegisterKeyboardTrigger(InputTrigger::GamepadSelect, "GamepadSelect", { { KeyCode::F1, "F1" } });
        RegisterKeyboardTrigger(InputTrigger::GamepadStart, "GamepadStart", { { KeyCode::Escape, "Esc" } });

        // UI navigation keyboard bindings
        RegisterKeyboardTrigger(InputTrigger::DevArrowUp, "KeyboardArrowUp", { { KeyCode::Up, "Up" } });
        RegisterKeyboardTrigger(InputTrigger::DevArrowDown, "KeyboardArrowDown", { { KeyCode::Down, "Down" } });
        RegisterKeyboardTrigger(InputTrigger::DevArrowLeft, "KeyboardArrowLeft", { { KeyCode::Left, "Left" } });
        RegisterKeyboardTrigger(InputTrigger::DevArrowRight, "KeyboardArrowRight", { { KeyCode::Right, "Right" } });
        RegisterKeyboardTrigger(InputTrigger::DevEnter, "KeyboardEnter", { { KeyCode::Enter, "Enter" } });
        RegisterKeyboardTrigger(InputTrigger::DevEscape, "KeyboardEscape", { { KeyCode::Escape, "Esc" } });

        // Editor undo/redo. Ctrl+Z and Ctrl+Y. Note that Ctrl+Shift+Z
        // would be a third common binding but the chord-trigger
        // evaluator fires on subset matches (Ctrl+Z would also fire
        // when Ctrl+Shift+Z is pressed) — Ctrl+Y avoids that conflict
        // and matches Windows convention.
        RegisterKeyboardTrigger(InputTrigger::EditUndo, "KeyboardCtrlZ", { { ANY_CTRL, "Ctrl" }, { KeyCode::Z, "Z" } });
        RegisterKeyboardTrigger(InputTrigger::EditRedo, "KeyboardCtrlY", { { ANY_CTRL, "Ctrl" }, { KeyCode::Y, "Y" } });

        // Gamepad bindings (actual gamepad buttons)
        RegisterGamepadTrigger(InputTrigger::GamepadDpadLeft, "GamepadDpadLeft", { { GamepadButton::DpadLeft, "DPad Left" } });
        RegisterGamepadTrigger(InputTrigger::GamepadDpadUp, "GamepadDpadUp", { { GamepadButton::DpadUp, "DPad Up" } });
        RegisterGamepadTrigger(InputTrigger::GamepadDpadRight, "GamepadDpadRight", { { GamepadButton::DpadRight, "DPad Right" } });
        RegisterGamepadTrigger(InputTrigger::GamepadDpadDown, "GamepadDpadDown", { { GamepadButton::DpadDown, "DPad Down" } });
        RegisterGamepadTrigger(InputTrigger::GamepadFaceLeft, "GamepadFaceLeft", { { GamepadButton::FaceLeft, "X" } });
        RegisterGamepadTrigger(InputTrigger::GamepadFaceUp, "GamepadFaceUp", { { GamepadButton::FaceUp, "Y" } });
        RegisterGamepadTrigger(InputTrigger::GamepadFaceRight, "GamepadFaceRight", { { GamepadButton::FaceRight, "B" } });
        RegisterGamepadTrigger(InputTrigger::GamepadFaceDown, "GamepadFaceDown", { { GamepadButton::FaceDown, "A" } });
        RegisterGamepadTrigger(InputTrigger::GamepadBumperLeft, "GamepadBumperLeft", { { GamepadButton::BumperLeft, "LB" } });
        RegisterGamepadTrigger(InputTrigger::GamepadBumperRight, "GamepadBumperRight", { { GamepadButton::BumperRight, "RB" } });
        RegisterGamepadTrigger(InputTrigger::GamepadTriggerLeft, "GamepadTriggerLeft", { { GamepadAxis::TriggerLeft, "LT", 0.5f } });
        RegisterGamepadTrigger(InputTrigger::GamepadTriggerRight, "GamepadTriggerRight", { { GamepadAxis::TriggerRight, "RT", 0.5f } });
        RegisterGamepadTrigger(InputTrigger::GamepadSelect, "GamepadSelect", { { GamepadButton::Select, "Back" } });
        RegisterGamepadTrigger(InputTrigger::GamepadStart, "GamepadStart", { { GamepadButton::Start, "Start" } });
    }

    void InputTriggerMap::RegisterKeyboardTrigger(int trigger, const std::string& triggerName, std::vector<KeyboardTriggerDef> keys)
    {
        CC_ASSERT(trigger >= 0 && trigger < InputTrigger::TriggerMax, "Trigger out of range");
        triggerNames[trigger] = triggerName;
        keyboardBindings[trigger] = keys;
    }

    void InputTriggerMap::RegisterGamepadTrigger(int trigger, const std::string& triggerName, std::vector<GamepadTriggerDef> buttons)
    {
        CC_ASSERT(trigger >= 0 && trigger < InputTrigger::TriggerMax, "Trigger out of range");
        triggerNames[trigger] = triggerName;
        gamepadBindings[trigger] = buttons;
    }

    void InputTriggerMap::Update()
    {
        physicalInput->UpdateGamepadState();

        bool keyboardActivityDetected = DetectMouseActivity();
        bool gamepadActivityDetected = false;

        bool devUiCapturingKeyboard = DevUi::Get()->IsCapturingKeyboard();

        // Evaluate all triggers for both input types
        for (int i = 0; i < InputTrigger::TriggerMax; i++)
        {
            triggersPrevious[i] = triggersCurrent[i];

            bool suppressKeyboard = devUiCapturingKeyboard && i >= InputTrigger::DevKeysMax;
            keyboardTriggersCurrent[i] = suppressKeyboard ? false : EvaluateKeyboardTrigger(i);
            gamepadTriggersCurrent[i] = EvaluateGamepadTrigger(i);

            if (keyboardTriggersCurrent[i])
            {
                keyboardActivityDetected = true;
            }
            if (gamepadTriggersCurrent[i])
            {
                gamepadActivityDetected = true;
            }
        }

        bool touchActivityDetected = touchActivityPending;
        touchActivityPending = false;

        // Decide active input type by priority: Touch > Gamepad > Keyboard.
        // Touch wins because the mouse abstraction on Android maps slot 0
        // through the same channels as a desktop mouse, so a finger landing
        // would otherwise also fire keyboard/mouse activity and override
        // the touch signal.
        ActiveInputType target = activeInputType;
        if (touchActivityDetected)
        {
            target = ActiveInputType::Touch;
        }
        else if (gamepadActivityDetected)
        {
            target = ActiveInputType::Gamepad;
        }
        else if (keyboardActivityDetected)
        {
            target = ActiveInputType::KeyboardMouse;
        }

        if (target != activeInputType)
        {
            activeInputType = target;
            triggersCurrent = (target == ActiveInputType::KeyboardMouse)
                ? keyboardTriggersCurrent
                : gamepadTriggersCurrent;
        }
    }

    void InputTriggerMap::NotifyTouchActivity()
    {
        touchActivityPending = true;
    }

    bool InputTriggerMap::EvaluateKeyboardTrigger(int trigger)
    {
        if (keyboardBindings[trigger].empty())
        {
            return false;
        }

        bool allPressed = true;
        for (const KeyboardTriggerDef& def : keyboardBindings[trigger])
        {
            if (!IsKeyboardKeyPressed(def.key))
            {
                allPressed = false;
                break;
            }
        }
        return allPressed;
    }

    bool InputTriggerMap::EvaluateGamepadTrigger(int trigger)
    {
        if (gamepadBindings[trigger].empty())
        {
            return false;
        }

        if (physicalInput->GetConnectedGamepadIndex() < 0)
        {
            return false;
        }

        bool allPressed = true;
        for (const GamepadTriggerDef& def : gamepadBindings[trigger])
        {
            bool pressed = false;
            if (def.isAxis)
            {
                pressed = IsGamepadAxisTriggered(static_cast<GamepadAxis::Axis>(def.id), def.axisThreshold);
            }
            else
            {
                pressed = IsGamepadButtonPressed(static_cast<GamepadButton::Button>(def.id));
            }

            if (!pressed)
            {
                allPressed = false;
                break;
            }
        }
        return allPressed;
    }

    bool InputTriggerMap::IsTriggered(int trigger)
    {
        CC_ASSERT(trigger >= 0 && trigger < InputTrigger::TriggerMax, "Trigger out of range");
        return triggersCurrent[trigger];
    }

    bool InputTriggerMap::EdgePositive(int trigger)
    {
        CC_ASSERT(trigger >= 0 && trigger < InputTrigger::TriggerMax, "Trigger out of range");
        return triggersCurrent[trigger] && !triggersPrevious[trigger];
    }

    bool InputTriggerMap::EdgeNegative(int trigger)
    {
        CC_ASSERT(trigger >= 0 && trigger < InputTrigger::TriggerMax, "Trigger out of range");
        return !triggersCurrent[trigger] && triggersPrevious[trigger];
    }

    bool InputTriggerMap::IsKeyboardKeyPressed(KeyCode::Key key)
    {
        if (key == ANY_CTRL)
        {
            return physicalInput->IsKeyDown(KeyCode::LeftControl) ||
                physicalInput->IsKeyDown(KeyCode::RightControl);
        }
        if (key == ANY_SHIFT)
        {
            return physicalInput->IsKeyDown(KeyCode::LeftShift) ||
                physicalInput->IsKeyDown(KeyCode::RightShift);
        }
        if (key == ANY_ALT)
        {
            return physicalInput->IsKeyDown(KeyCode::LeftAlt) ||
                physicalInput->IsKeyDown(KeyCode::RightAlt);
        }
        return physicalInput->IsKeyDown(key);
    }

    bool InputTriggerMap::IsGamepadButtonPressed(GamepadButton::Button button)
    {
        const GamepadState& state = physicalInput->GetGamepadState();
        return state.buttons[button];
    }

    bool InputTriggerMap::IsGamepadAxisTriggered(GamepadAxis::Axis axis, float threshold)
    {
        const GamepadState& state = physicalInput->GetGamepadState();
        float value = state.axes[axis];
        if (threshold > 0)
        {
            return value > threshold;
        }
        else
        {
            return value < threshold;
        }
    }

    bool InputTriggerMap::DetectMouseActivity()
    {
        int currentMouseX = 0;
        int currentMouseY = 0;
        physicalInput->GetMousePosition(currentMouseX, currentMouseY);

        if (currentMouseX != lastMouseX || currentMouseY != lastMouseY)
        {
            lastMouseX = currentMouseX;
            lastMouseY = currentMouseY;
            return true;
        }

        if (physicalInput->IsMouseButtonDown(MouseButton::Left) ||
            physicalInput->IsMouseButtonDown(MouseButton::Right))
        {
            return true;
        }

        return false;
    }

    ActiveInputType InputTriggerMap::GetActiveInputType() const
    {
        return activeInputType;
    }

    const std::string& InputTriggerMap::GetTriggerName(int trigger) const
    {
        CC_ASSERT(trigger >= 0 && trigger < InputTrigger::TriggerMax, "Trigger out of range");
        return triggerNames[trigger];
    }

    std::string InputTriggerMap::GetKeyComboString(int trigger) const
    {
        if (activeInputType == ActiveInputType::KeyboardMouse)
        {
            return GetKeyboardKeyComboString(trigger);
        }
        else
        {
            return GetGamepadKeyComboString(trigger);
        }
    }

    std::string InputTriggerMap::GetKeyboardKeyComboString(int trigger) const
    {
        CC_ASSERT(trigger >= 0 && trigger < InputTrigger::TriggerMax, "Trigger out of range");

        const std::vector<KeyboardTriggerDef>& keys = keyboardBindings[trigger];

        if (keys.empty())
        {
            return "(unbound)";
        }

        std::string result;
        for (size_t i = 0; i < keys.size(); i++)
        {
            if (i > 0)
            {
                result += " + ";
            }
            result += keys[i].keyName;
        }

        return result;
    }

    std::string InputTriggerMap::GetGamepadKeyComboString(int trigger) const
    {
        CC_ASSERT(trigger >= 0 && trigger < InputTrigger::TriggerMax, "Trigger out of range");

        const std::vector<GamepadTriggerDef>& buttons = gamepadBindings[trigger];

        if (buttons.empty())
        {
            return "(unbound)";
        }

        std::string result;
        for (size_t i = 0; i < buttons.size(); i++)
        {
            if (i > 0)
            {
                result += " + ";
            }
            result += buttons[i].name;
        }

        return result;
    }
}