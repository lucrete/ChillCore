#include "PlatformInputAndroid.h"

namespace CC
{
    PlatformInputAndroid* PlatformInputAndroid::instance = nullptr;

    PlatformInputAndroid::PlatformInputAndroid()
    {
        instance = this;
    }

    PlatformInputAndroid::~PlatformInputAndroid()
    {
        if (instance == this)
        {
            instance = nullptr;
        }
    }

    PlatformInputAndroid* PlatformInputAndroid::Get()
    {
        return instance;
    }

    // ========================
    // Pointer routing
    // ========================

    int PlatformInputAndroid::FindSlotByAndroidId(int androidId) const
    {
        int result = -1;
        for (int i = 0; i < MAX_TOUCH_POINTERS && result == -1; i++)
        {
            if (pointers[i].active && pointers[i].androidId == androidId)
            {
                result = i;
            }
        }
        return result;
    }

    int PlatformInputAndroid::AllocateSlot()
    {
        int result = -1;
        for (int i = 0; i < MAX_TOUCH_POINTERS && result == -1; i++)
        {
            if (!pointers[i].active)
            {
                result = i;
            }
        }
        return result;
    }

    void PlatformInputAndroid::OnPointerDown(int androidId, int x, int y)
    {
        int slot = FindSlotByAndroidId(androidId);
        if (slot == -1)
        {
            slot = AllocateSlot();
        }

        if (slot != -1)
        {
            pointers[slot].active = true;
            pointers[slot].androidId = androidId;
            pointers[slot].x = x;
            pointers[slot].y = y;
        }
        touchActivityLatched = true;
    }

    void PlatformInputAndroid::OnPointerMove(int androidId, int x, int y)
    {
        int slot = FindSlotByAndroidId(androidId);
        if (slot != -1)
        {
            pointers[slot].x = x;
            pointers[slot].y = y;
        }
        touchActivityLatched = true;
    }

    void PlatformInputAndroid::OnPointerUp(int androidId)
    {
        int slot = FindSlotByAndroidId(androidId);
        if (slot != -1)
        {
            pointers[slot].active = false;
            pointers[slot].androidId = -1;
        }
        touchActivityLatched = true;
    }

    void PlatformInputAndroid::OnPointerCancelAll()
    {
        for (int i = 0; i < MAX_TOUCH_POINTERS; i++)
        {
            pointers[i].active = false;
            pointers[i].androidId = -1;
        }
        touchActivityLatched = true;
    }

    // ========================
    // PlatformInput interface
    // ========================

    bool PlatformInputAndroid::IsKeyDown(KeyCode::Key key) const
    {
        (void)key;
        return false;
    }

    void PlatformInputAndroid::GetMousePosition(int& x, int& y) const
    {
        // Mouse abstraction = slot 0 (primary pointer).
        x = pointers[0].x;
        y = pointers[0].y;
    }

    void PlatformInputAndroid::SetMousePosition(int x, int y)
    {
        (void)x;
        (void)y;
    }

    bool PlatformInputAndroid::IsMouseButtonDown(MouseButton::Button button) const
    {
        // Single-finger touch maps to the left mouse button via slot 0.
        bool result = false;
        if (button == MouseButton::Left)
        {
            result = pointers[0].active;
        }
        return result;
    }

    void PlatformInputAndroid::SetMouseCursorLocked(bool locked)
    {
        (void)locked;
    }

    bool PlatformInputAndroid::IsMouseCursorLocked() const
    {
        return false;
    }

    void PlatformInputAndroid::UpdateGamepadState()
    {
    }

    int PlatformInputAndroid::GetConnectedGamepadIndex() const
    {
        return -1;
    }

    const GamepadState& PlatformInputAndroid::GetGamepadState() const
    {
        return gamepadState;
    }

    // ========================
    // Multi-touch
    // ========================

    bool PlatformInputAndroid::IsTouchPointerActive(int slot) const
    {
        bool result = false;
        if (slot >= 0 && slot < MAX_TOUCH_POINTERS)
        {
            result = pointers[slot].active;
        }
        return result;
    }

    void PlatformInputAndroid::GetTouchPointer(int slot, int& outX, int& outY) const
    {
        outX = 0;
        outY = 0;
        if (slot >= 0 && slot < MAX_TOUCH_POINTERS)
        {
            outX = pointers[slot].x;
            outY = pointers[slot].y;
        }
    }

    bool PlatformInputAndroid::ConsumeTouchActivityLatch()
    {
        bool result = touchActivityLatched;
        touchActivityLatched = false;
        return result;
    }
}
