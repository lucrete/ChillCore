#ifndef UIINPUTHANDLER_H
#define UIINPUTHANDLER_H

#include <vector>
#include "UiCallbackMap.h"
#include "PlatformInput.h"

namespace CC
{
    class UiElement;
    class UiJoystick;
    class UiSurface;

    // Where the pointer is and whether it is pressed, in surface-space
    // pixels. Normally the mouse or a finger. A ray hitting a panel in world
    // space converts its hit to the same pixels and supplies it instead,
    // which is how a controller drives a UI that knows nothing about
    // controllers.
    struct UiPointerState
    {
        float x = 0.0f;
        float y = 0.0f;
        bool  isDown = false;
        // False where the pointer is off the surface entirely, which is not
        // the same as being at (0,0) and released.
        bool  isActive = false;
    };

    // Where a surface's pointers come from. The window reads them from the
    // platform; every other surface is given them by whatever is pointing
    // at it.
    enum class UiPointerSource
    {
        Platform,
        External
    };

    // A surface accepts several pointers at once, each with its own hover
    // and press, so two hands can use one panel and a press stays with the
    // hand that began it. An element is hovered if any pointer hovers it.
    class UiInputHandler
    {
    public:
        static const int MAX_POINTERS = PlatformInput::MAX_TOUCH_POINTERS;

        UiInputHandler();
        ~UiInputHandler();

        void SetSurface(UiSurface* _surface) { surface = _surface; }
        void SetPointerSource(UiPointerSource source) { pointerSource = source; }

        void SetRootElement(UiElement* root);
        void BuildNavigationList();
        void Update();

        void SetCallbackMap(UiCallbackMap* map);
        UiCallbackMap* GetCallbackMap() { return callbackMap; }

        // Supplies one pointer for the coming frame. External surfaces only;
        // the window reads its own from the platform.
        void SetPointer(int pointerId, const UiPointerState& pointer);
        void ClearPointer(int pointerId);

        UiElement* GetSelectedElement() const;
        bool IsHovered() const;

    private:
        struct UiPointer
        {
            UiPointerState state;
            UiElement*     hoveredElement = nullptr;
            UiElement*     pressedElement = nullptr;
            bool           wasDown = false;
        };

        UiSurface*      surface = nullptr;
        UiPointerSource pointerSource = UiPointerSource::Platform;

        UiElement* rootElement = nullptr;
        std::vector<UiElement*> navigationList;
        int selectedIndex = -1;

        UiPointer pointers[MAX_POINTERS];

        // Elements this handler last put into a pointer-driven state. Reset
        // on the way to computing the next frame's, so an element no pointer
        // is on returns to Normal without anything tracking who left it.
        std::vector<UiElement*> statedElements;

        class UiDropdown* expandedDropdown = nullptr;

        UiCallbackMap* callbackMap = nullptr;

        void SamplePlatformPointers();
        void UpdatePointer(int pointerId);
        void ReleasePointer(int pointerId);
        void ApplyPointerStates();

        void UpdateNavigation();
        void SelectElement(int index);
        void ActivateElement(UiElement* element);

        UiElement* HitTest(UiElement* element, float px, float py);
        void CollectNavigableElements(UiElement* element);
        void UpdateSliderFromPointer(class UiSlider* slider, float pointerX);
        void UpdateJoystickFromPointer(class UiJoystick* joystick, float pointerX, float pointerY);
        void ReleaseJoystick(class UiJoystick* joystick);
        void UpdateDropdownNavigation(class UiDropdown* dropdown);
        int HitTestDropdownOptions(class UiDropdown* dropdown, float px, float py);
    };
}

#endif // UIINPUTHANDLER_H
