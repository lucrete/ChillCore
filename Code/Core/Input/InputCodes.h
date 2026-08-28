#ifndef INPUT_CODES_H
#define INPUT_CODES_H

namespace CC
{
    enum class ActiveInputType
    {
        KeyboardMouse,
        Gamepad,
        Touch
    };

    namespace KeyCode
    {
        enum Key
        {
            // Modifiers
            LeftControl,
            RightControl,
            LeftShift,
            RightShift,
            LeftAlt,
            RightAlt,

            // Function keys
            F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

            // Navigation
            Escape,
            Enter,
            Space,
            Backspace,
            Tab,

            // Arrow keys
            Left, Right, Up, Down,

            // Letters
            A, B, C, D, E, F, G, H, I, J, K, L, M,
            N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

            // Numbers
            Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

            // Punctuation
            GraveAccent,

            KeyMax
        };
    }

    namespace MouseButton
    {
        enum Button
        {
            Left,
            Right,
            Middle,
            ButtonMax
        };
    }

    namespace GamepadButton
    {
        enum Button
        {
            DpadLeft,
            DpadRight,
            DpadUp,
            DpadDown,
            FaceLeft,
            FaceRight,
            FaceUp,
            FaceDown,
            BumperLeft,
            BumperRight,
            StickLeft,
            StickRight,
            Start,
            Select,
            ButtonMax
        };
    }

    namespace GamepadAxis
    {
        enum Axis
        {
            LeftStickX,
            LeftStickY,
            RightStickX,
            RightStickY,
            TriggerLeft,
            TriggerRight,
            AxisMax
        };
    }
}

#endif