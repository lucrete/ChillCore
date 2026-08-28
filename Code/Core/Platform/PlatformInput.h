#ifndef PLATFORMINPUT_H
#define PLATFORMINPUT_H

#include "InputCodes.h"

namespace CC
{
    // ========================
    // PlatformInput
    // ========================

    struct GamepadState
    {
        bool buttons[GamepadButton::ButtonMax];
        float axes[GamepadAxis::AxisMax];
        bool connected;

        GamepadState()
            : connected(false)
        {
            for (int i = 0; i < GamepadButton::ButtonMax; i++)
            {
                buttons[i] = false;
            }
            for (int i = 0; i < GamepadAxis::AxisMax; i++)
            {
                axes[i] = 0.0f;
            }
        }
    };

    class PlatformInput
    {
    public:
        virtual ~PlatformInput() = default;

        // Keyboard
        virtual bool IsKeyDown(KeyCode::Key key) const = 0;

        // Mouse
        virtual void GetMousePosition(int& x, int& y) const = 0;
        virtual void SetMousePosition(int x, int y) = 0;
        virtual bool IsMouseButtonDown(MouseButton::Button button) const = 0;
        virtual void SetMouseCursorLocked(bool locked) = 0;
        virtual bool IsMouseCursorLocked() const = 0;

        // Gamepad
        virtual void UpdateGamepadState() = 0;
        virtual int GetConnectedGamepadIndex() const = 0;
        virtual const GamepadState& GetGamepadState() const = 0;

        // ========================
        // Multi-touch
        // ========================
        // Pointer slots 0..MAX-1. A slot stays bound to one physical finger
        // for that contact's lifetime; the slot index is therefore a stable
        // ID across frames. Slot 0 mirrors the mouse abstraction (primary
        // pointer / left button); slots 1..N are extra fingers exposed
        // for multi-touch UI like UiJoystick. Platforms without multi-touch
        // (Glfw mouse) leave all slots inactive — the mouse path covers it.
        static constexpr int MAX_TOUCH_POINTERS = 8;

        virtual bool IsTouchPointerActive(int slot) const
        {
            (void)slot;
            return false;
        }
        virtual void GetTouchPointer(int slot, int& outX, int& outY) const
        {
            (void)slot;
            outX = 0;
            outY = 0;
        }

        // True if any touch event (down / move / up / cancel) was
        // delivered to this PlatformInput since the last call. Reading
        // clears the latch. Lets InputManager catch tap-down + tap-up
        // pairs that arrive in the same frame's input buffer drain —
        // the slot-active poll alone misses those because by the time
        // the per-frame poll runs, the pointer is already inactive.
        // Default false for platforms without a separate touch path.
        virtual bool ConsumeTouchActivityLatch()
        {
            return false;
        }
    };
}

#endif // PLATFORMINPUT_H
