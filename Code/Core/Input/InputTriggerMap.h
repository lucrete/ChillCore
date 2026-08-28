#ifndef INPUT_TRIGGER_MAP_H
#define INPUT_TRIGGER_MAP_H

#include "InputCodes.h"
#include "PlatformInput.h"
#include <vector>
#include <string>

namespace CC
{
    struct KeyboardTriggerDef
    {
        KeyCode::Key key;
        std::string keyName;

        KeyboardTriggerDef(KeyCode::Key k, const std::string& name)
            : key(k), keyName(name) {
        }
    };

    struct GamepadTriggerDef
    {
        int id;
        std::string name;
        bool isAxis;
        float axisThreshold;

        // Button constructor
        GamepadTriggerDef(GamepadButton::Button button, const std::string& name)
            : id(button), name(name), isAxis(false), axisThreshold(0.0f) {
        }

        // Axis-as-button constructor
        GamepadTriggerDef(GamepadAxis::Axis axis, const std::string& name, float threshold)
            : id(axis), name(name), isAxis(true), axisThreshold(threshold) {
        }
    };

    namespace InputTrigger
    {
        enum Trigger
        {
            DevF1,
            DevF2,
            DevF3,
            DevF4,
            DevGraveAccent,
            DevCtrlF5,
            DevF9,
            DevF10,
            DevKeysMax,

            GamepadDpadLeft,
            GamepadDpadUp,
            GamepadDpadRight,
            GamepadDpadDown,
            GamepadFaceLeft,
            GamepadFaceUp,
            GamepadFaceRight,
            GamepadFaceDown,
            GamepadBumperLeft,
            GamepadBumperRight,
            GamepadTriggerLeft,
            GamepadTriggerRight,
            GamepadSelect,
            GamepadStart,

            DevArrowUp,
            DevArrowDown,
            DevArrowLeft,
            DevArrowRight,
            DevEnter,
            DevEscape,

            // Generic editing chords. Used by any AppState wiring an
            // undoable editor (currently AudioTracker; future editors
            // can reuse the same triggers).
            EditUndo,
            EditRedo,

            TriggerMax
        };
    }

    class InputTriggerMap
    {
    public:
        static const KeyCode::Key ANY_CTRL = static_cast<KeyCode::Key>(-1);
        static const KeyCode::Key ANY_SHIFT = static_cast<KeyCode::Key>(-2);
        static const KeyCode::Key ANY_ALT = static_cast<KeyCode::Key>(-3);

        static constexpr float GAMEPAD_AXIS_ACTIVATION_THRESHOLD = 0.3f;

        InputTriggerMap(PlatformInput* physicalInput);

        void RegisterKeyboardTrigger(int trigger, const std::string& triggerName, std::vector<KeyboardTriggerDef> keys);
        void RegisterGamepadTrigger(int trigger, const std::string& triggerName, std::vector<GamepadTriggerDef> buttons);

        void Update();
        bool IsTriggered(int trigger);
        bool EdgePositive(int trigger);
        bool EdgeNegative(int trigger);

        ActiveInputType GetActiveInputType() const;
        void NotifyTouchActivity();
        const std::string& GetTriggerName(int trigger) const;
        std::string GetKeyComboString(int trigger) const;
        std::string GetKeyboardKeyComboString(int trigger) const;
        std::string GetGamepadKeyComboString(int trigger) const;

    private:
        PlatformInput* physicalInput;

        std::string triggerNames[InputTrigger::TriggerMax];
        std::vector<KeyboardTriggerDef> keyboardBindings[InputTrigger::TriggerMax];
        std::vector<GamepadTriggerDef> gamepadBindings[InputTrigger::TriggerMax];

        bool keyboardTriggersCurrent[InputTrigger::TriggerMax];
        bool gamepadTriggersCurrent[InputTrigger::TriggerMax];
        bool* triggersCurrent;
        bool triggersPrevious[InputTrigger::TriggerMax];

        ActiveInputType activeInputType;
        bool touchActivityPending;

        int lastMouseX;
        int lastMouseY;

        bool IsKeyboardKeyPressed(KeyCode::Key key);
        bool IsGamepadButtonPressed(GamepadButton::Button button);
        bool IsGamepadAxisTriggered(GamepadAxis::Axis axis, float threshold);

        bool EvaluateKeyboardTrigger(int trigger);
        bool EvaluateGamepadTrigger(int trigger);

        bool DetectMouseActivity();
    };
}

#endif