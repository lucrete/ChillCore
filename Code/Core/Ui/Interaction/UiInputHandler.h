#ifndef UIINPUTHANDLER_H
#define UIINPUTHANDLER_H

#include <vector>
#include "UiCallbackMap.h"
#include "PlatformInput.h"

namespace CC
{
    class UiElement;
    class UiJoystick;

    class UiInputHandler
    {
    public:
        UiInputHandler();
        ~UiInputHandler();

        void SetRootElement(UiElement* root);
        void BuildNavigationList();
        void Update();

        void SetCallbackMap(UiCallbackMap* map);
        UiCallbackMap* GetCallbackMap() { return callbackMap; }

        UiElement* GetSelectedElement() const;
        bool IsHovered() const { return hoveredElement != nullptr || expandedDropdown != nullptr; }

    private:
        UiElement* rootElement = nullptr;
        std::vector<UiElement*> navigationList;
        int selectedIndex = -1;

        UiElement* hoveredElement = nullptr;
        UiElement* pressedElement = nullptr;
        class UiDropdown* expandedDropdown = nullptr;
        bool wasMouseDown = false;

        // One joystick capture per non-primary touch slot. Slot 0 (mouse
        // abstraction / primary finger) is handled by UpdateMouse; slots
        // 1..MAX-1 are tracked here for simultaneous multi-touch drags.
        UiJoystick* joystickTouchTracks[PlatformInput::MAX_TOUCH_POINTERS] = {};

        UiCallbackMap* callbackMap = nullptr;

        void UpdateMouse();
        void UpdateTouchPointers();
        void UpdateNavigation();
        void SelectElement(int index);
        void ActivateElement(UiElement* element);

        UiElement* HitTest(UiElement* element, float px, float py, bool joystickOnly);
        void CollectNavigableElements(UiElement* element);
        void UpdateSliderFromMouse(class UiSlider* slider, float mouseX);
        void UpdateJoystickFromMouse(class UiJoystick* joystick, float mouseX, float mouseY);
        void ReleaseJoystick(class UiJoystick* joystick);
        void UpdateDropdownNavigation(class UiDropdown* dropdown);
        int HitTestDropdownOptions(class UiDropdown* dropdown, float px, float py);
    };
}

#endif // UIINPUTHANDLER_H
