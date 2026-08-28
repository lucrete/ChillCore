#ifndef PLATFORMINPUTGLFW_H
#define PLATFORMINPUTGLFW_H

#include "PlatformInput.h"
#include <GLFW/glfw3.h>

namespace CC
{
    class PlatformInputGlfw : public PlatformInput
    {
    public:
        PlatformInputGlfw();
        virtual ~PlatformInputGlfw() = default;

        // Keyboard
        bool IsKeyDown(KeyCode::Key key) const override;

        // Mouse
        void GetMousePosition(int& x, int& y) const override;
        void SetMousePosition(int x, int y) override;
        bool IsMouseButtonDown(MouseButton::Button button) const override;
        void SetMouseCursorLocked(bool locked) override;
        bool IsMouseCursorLocked() const override;

        // Gamepad
        void UpdateGamepadState() override;
        int GetConnectedGamepadIndex() const override;
        const GamepadState& GetGamepadState() const override;

    private:
        GamepadState gamepadState;
        int activeGamepadIndex;

        int TranslateKeyCode(KeyCode::Key key) const;
        int TranslateMouseButton(MouseButton::Button button) const;
    };
}

#endif // PLATFORMINPUTGLFW_H
