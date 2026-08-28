#ifndef PLATFORMINPUTANDROID_H
#define PLATFORMINPUTANDROID_H

#include "PlatformInput.h"

namespace CC
{
    // ========================
    // PlatformInputAndroid
    // ========================
    //
    // Routes GameActivity touch + gamepad events to the rest of the engine.
    // Multi-pointer aware: slot 0 is the primary pointer and drives the
    // mouse abstraction (left-button-when-down) so single-finger menu
    // hit-testing works unchanged. Slots 1..MAX-1 are extra fingers exposed
    // through PlatformInput::GetTouchPointer for the UI layer to use for
    // multi-touch elements like UiJoystick.

    class PlatformInputAndroid : public PlatformInput
    {
    public:
        PlatformInputAndroid();
        virtual ~PlatformInputAndroid();

        // Multi-pointer event sinks. MainAndroid calls these as it drains
        // the GameActivity motion-event queue. androidId is the platform's
        // stable pointerId for the lifetime of one finger contact.
        void OnPointerDown(int androidId, int x, int y);
        void OnPointerMove(int androidId, int x, int y);
        void OnPointerUp(int androidId);
        void OnPointerCancelAll();

        // Singleton access for MainAndroid's event-pump.
        static PlatformInputAndroid* Get();

        bool                IsKeyDown(KeyCode::Key key) const override;
        void                GetMousePosition(int& x, int& y) const override;
        void                SetMousePosition(int x, int y) override;
        bool                IsMouseButtonDown(MouseButton::Button button) const override;
        void                SetMouseCursorLocked(bool locked) override;
        bool                IsMouseCursorLocked() const override;
        void                UpdateGamepadState() override;
        int                 GetConnectedGamepadIndex() const override;
        const GamepadState& GetGamepadState() const override;

        bool                IsTouchPointerActive(int slot) const override;
        void                GetTouchPointer(int slot, int& outX, int& outY) const override;
        bool                ConsumeTouchActivityLatch() override;

    private:
        struct PointerSlot
        {
            bool active = false;
            int  androidId = -1;
            int  x = 0;
            int  y = 0;
        };

        static PlatformInputAndroid* instance;

        PointerSlot  pointers[MAX_TOUCH_POINTERS];
        GamepadState gamepadState;
        bool         touchActivityLatched = false;

        int  FindSlotByAndroidId(int androidId) const;
        int  AllocateSlot();
    };
}

#endif // PLATFORMINPUTANDROID_H
