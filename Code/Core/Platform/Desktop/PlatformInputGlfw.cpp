#include "PlatformInputGlfw.h"

namespace CC
{
    namespace
    {
        const int keyCodeToGLFW[KeyCode::KeyMax] = {
            // Modifiers
            GLFW_KEY_LEFT_CONTROL,   // LeftControl
            GLFW_KEY_RIGHT_CONTROL,  // RightControl
            GLFW_KEY_LEFT_SHIFT,     // LeftShift
            GLFW_KEY_RIGHT_SHIFT,    // RightShift
            GLFW_KEY_LEFT_ALT,       // LeftAlt
            GLFW_KEY_RIGHT_ALT,      // RightAlt

            // Function keys
            GLFW_KEY_F1, GLFW_KEY_F2, GLFW_KEY_F3, GLFW_KEY_F4,
            GLFW_KEY_F5, GLFW_KEY_F6, GLFW_KEY_F7, GLFW_KEY_F8,
            GLFW_KEY_F9, GLFW_KEY_F10, GLFW_KEY_F11, GLFW_KEY_F12,

            // Navigation
            GLFW_KEY_ESCAPE,
            GLFW_KEY_ENTER,
            GLFW_KEY_SPACE,
            GLFW_KEY_BACKSPACE,
            GLFW_KEY_TAB,

            // Arrow keys
            GLFW_KEY_LEFT, GLFW_KEY_RIGHT, GLFW_KEY_UP, GLFW_KEY_DOWN,

            // Letters
            GLFW_KEY_A, GLFW_KEY_B, GLFW_KEY_C, GLFW_KEY_D, GLFW_KEY_E,
            GLFW_KEY_F, GLFW_KEY_G, GLFW_KEY_H, GLFW_KEY_I, GLFW_KEY_J,
            GLFW_KEY_K, GLFW_KEY_L, GLFW_KEY_M, GLFW_KEY_N, GLFW_KEY_O,
            GLFW_KEY_P, GLFW_KEY_Q, GLFW_KEY_R, GLFW_KEY_S, GLFW_KEY_T,
            GLFW_KEY_U, GLFW_KEY_V, GLFW_KEY_W, GLFW_KEY_X, GLFW_KEY_Y,
            GLFW_KEY_Z,

            // Numbers
            GLFW_KEY_0, GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4,
            GLFW_KEY_5, GLFW_KEY_6, GLFW_KEY_7, GLFW_KEY_8, GLFW_KEY_9,

            // Punctuation
            GLFW_KEY_GRAVE_ACCENT
        };

        const int mouseButtonToGLFW[MouseButton::ButtonMax] = {
            GLFW_MOUSE_BUTTON_LEFT,
            GLFW_MOUSE_BUTTON_RIGHT,
            GLFW_MOUSE_BUTTON_MIDDLE
        };

        const int gamepadButtonToGLFW[GamepadButton::ButtonMax] = {
            GLFW_GAMEPAD_BUTTON_DPAD_LEFT,
            GLFW_GAMEPAD_BUTTON_DPAD_RIGHT,
            GLFW_GAMEPAD_BUTTON_DPAD_UP,
            GLFW_GAMEPAD_BUTTON_DPAD_DOWN,
            GLFW_GAMEPAD_BUTTON_X,
            GLFW_GAMEPAD_BUTTON_B,
            GLFW_GAMEPAD_BUTTON_Y,
            GLFW_GAMEPAD_BUTTON_A,
            GLFW_GAMEPAD_BUTTON_LEFT_BUMPER,
            GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER,
            GLFW_GAMEPAD_BUTTON_LEFT_THUMB,
            GLFW_GAMEPAD_BUTTON_RIGHT_THUMB,
            GLFW_GAMEPAD_BUTTON_START,
            GLFW_GAMEPAD_BUTTON_BACK
        };

        const int gamepadAxisToGLFW[GamepadAxis::AxisMax] = {
            GLFW_GAMEPAD_AXIS_LEFT_X,
            GLFW_GAMEPAD_AXIS_LEFT_Y,
            GLFW_GAMEPAD_AXIS_RIGHT_X,
            GLFW_GAMEPAD_AXIS_RIGHT_Y,
            GLFW_GAMEPAD_AXIS_LEFT_TRIGGER,
            GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER
        };
    }

    PlatformInputGlfw::PlatformInputGlfw()
        : activeGamepadIndex(-1)
    {
    }

    bool PlatformInputGlfw::IsKeyDown(KeyCode::Key key) const
    {
        int glfwKey = TranslateKeyCode(key);
        return glfwGetKey(glfwGetCurrentContext(), glfwKey) == GLFW_PRESS;
    }

    void PlatformInputGlfw::GetMousePosition(int& x, int& y) const
    {
        double mx, my;
        glfwGetCursorPos(glfwGetCurrentContext(), &mx, &my);
        x = static_cast<int>(mx);
        y = static_cast<int>(my);
    }

    void PlatformInputGlfw::SetMousePosition(int x, int y)
    {
        glfwSetCursorPos(glfwGetCurrentContext(), x, y);
    }

    bool PlatformInputGlfw::IsMouseButtonDown(MouseButton::Button button) const
    {
        int glfwButton = TranslateMouseButton(button);
        return glfwGetMouseButton(glfwGetCurrentContext(), glfwButton) == GLFW_PRESS;
    }

    void PlatformInputGlfw::SetMouseCursorLocked(bool locked)
    {
        glfwSetInputMode(glfwGetCurrentContext(), GLFW_CURSOR, locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }

    bool PlatformInputGlfw::IsMouseCursorLocked() const
    {
        return glfwGetInputMode(glfwGetCurrentContext(), GLFW_CURSOR) != GLFW_CURSOR_NORMAL;
    }

    void PlatformInputGlfw::UpdateGamepadState()
    {
        activeGamepadIndex = -1;
        gamepadState = GamepadState();

        for (int i = GLFW_JOYSTICK_1; i <= GLFW_JOYSTICK_LAST; i++)
        {
            if (glfwJoystickIsGamepad(i))
            {
                GLFWgamepadstate glfwState;
                if (glfwGetGamepadState(i, &glfwState))
                {
                    activeGamepadIndex = i;
                    gamepadState.connected = true;

                    // Map buttons
                    for (int b = 0; b < GamepadButton::ButtonMax; b++)
                    {
                        int glfwButton = gamepadButtonToGLFW[b];
                        gamepadState.buttons[b] = (glfwState.buttons[glfwButton] == GLFW_PRESS);
                    }

                    // Map axes
                    for (int a = 0; a < GamepadAxis::AxisMax; a++)
                    {
                        int glfwAxis = gamepadAxisToGLFW[a];
                        gamepadState.axes[a] = glfwState.axes[glfwAxis];
                    }

                    // Normalize triggers from -1..1 to 0..1
                    gamepadState.axes[GamepadAxis::TriggerLeft] = (gamepadState.axes[GamepadAxis::TriggerLeft] + 1.0f) * 0.5f;
                    gamepadState.axes[GamepadAxis::TriggerRight] = (gamepadState.axes[GamepadAxis::TriggerRight] + 1.0f) * 0.5f;
                    break;
                }
            }
        }
    }

    int PlatformInputGlfw::GetConnectedGamepadIndex() const
    {
        return activeGamepadIndex;
    }

    const GamepadState& PlatformInputGlfw::GetGamepadState() const
    {
        return gamepadState;
    }

    int PlatformInputGlfw::TranslateKeyCode(KeyCode::Key key) const
    {
        int result = GLFW_KEY_UNKNOWN;
        if (key >= 0 && key < KeyCode::KeyMax)
        {
            result = keyCodeToGLFW[key];
        }
        return result;
    }

    int PlatformInputGlfw::TranslateMouseButton(MouseButton::Button button) const
    {
        int result = GLFW_MOUSE_BUTTON_LEFT;
        if (button >= 0 && button < MouseButton::ButtonMax)
        {
            result = mouseButtonToGLFW[button];
        }
        return result;
    }
}
