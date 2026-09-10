#include "UiInputHandler.h"

#include "UiDropdown.h"
#include "UiElement.h"
#include "UiJoystick.h"
#include "UiSlider.h"
#include "UiSurface.h"
#include "UiToggle.h"

#include "AudioManager.h"
#include "InputManager.h"
#include "StateMachine.h"

#include <algorithm>
#include <cmath>

namespace CC
{
    UiInputHandler::UiInputHandler()
    {
    }

    UiInputHandler::~UiInputHandler()
    {
    }

    void UiInputHandler::SetPointer(int pointerId, const UiPointerState& pointer)
    {
        if (pointerId >= 0 && pointerId < MAX_POINTERS)
        {
            pointers[pointerId].state = pointer;
        }
    }

    void UiInputHandler::ClearPointer(int pointerId)
    {
        if (pointerId >= 0 && pointerId < MAX_POINTERS)
        {
            pointers[pointerId].state = UiPointerState();
        }
    }

    void UiInputHandler::SetCallbackMap(UiCallbackMap* map)
    {
        callbackMap = map;
    }

    void UiInputHandler::SetRootElement(UiElement* root)
    {
        for (UiElement* element : statedElements)
        {
            element->SetState(UiElementState::Normal);
        }
        statedElements.clear();

        rootElement = root;
        expandedDropdown = nullptr;
        selectedIndex = -1;
        navigationList.clear();

        for (int pointer = 0; pointer < MAX_POINTERS; pointer++)
        {
            pointers[pointer] = UiPointer();
        }

        if (rootElement)
        {
            BuildNavigationList();
        }
    }

    void UiInputHandler::BuildNavigationList()
    {
        navigationList.clear();
        if (rootElement)
        {
            CollectNavigableElements(rootElement);
        }
    }

    void UiInputHandler::CollectNavigableElements(UiElement* element)
    {
        if (!element->IsVisible() || element->computedStyle.display == DisplayType::None)
        {
            return;
        }

        if (element->IsNavigable() && element->IsEnabled())
        {
            navigationList.push_back(element);
        }

        for (UiElement* child : element->GetChildren())
        {
            CollectNavigableElements(child);
        }
    }

    bool UiInputHandler::IsHovered() const
    {
        bool isHovered = expandedDropdown != nullptr;

        for (int pointer = 0; pointer < MAX_POINTERS && !isHovered; pointer++)
        {
            isHovered = pointers[pointer].hoveredElement != nullptr;
        }

        return isHovered;
    }

    // ========================
    // Per-frame
    // ========================

    void UiInputHandler::Update()
    {
        if (rootElement != nullptr && callbackMap != nullptr)
        {
            if (pointerSource == UiPointerSource::Platform)
            {
                SamplePlatformPointers();
            }

            for (int pointer = 0; pointer < MAX_POINTERS; pointer++)
            {
                UpdatePointer(pointer);
            }

            ApplyPointerStates();

            // Directional navigation is one input stream with no spatial
            // origin, so only the surface reading the platform consumes it.
            // Panels are pointed at.
            if (pointerSource == UiPointerSource::Platform && InputManager::Get()->IsUiInteractable())
            {
                UpdateNavigation();
            }
        }
    }

    // The mouse abstraction is slot 0; extra fingers take the slots above
    // it. Both go through the same path from here on, so multi-touch is no
    // longer a case of its own.
    void UiInputHandler::SamplePlatformPointers()
    {
        InputManager* input = InputManager::Get();

        UiPointerState primary;
        if (!input->IsMouseCursorLocked())
        {
            int mouseX = 0;
            int mouseY = 0;
            input->GetMousePosition(mouseX, mouseY);

            primary.x        = (float)mouseX;
            primary.y        = (float)mouseY;
            primary.isDown   = input->IsMouseButtonDown(MouseButton::Left);
            primary.isActive = true;
        }
        pointers[0].state = primary;

        for (int slot = 1; slot < MAX_POINTERS; slot++)
        {
            UiPointerState touch;
            if (input->IsTouchPointerActive(slot))
            {
                int x = 0;
                int y = 0;
                input->GetTouchPointer(slot, x, y);

                touch.x        = (float)x;
                touch.y        = (float)y;
                touch.isDown   = true;
                touch.isActive = true;
            }
            pointers[slot].state = touch;
        }
    }

    void UiInputHandler::UpdatePointer(int pointerId)
    {
        UiPointer& pointer = pointers[pointerId];

        if (!pointer.state.isActive)
        {
            ReleasePointer(pointerId);
        }
        else
        {
            InputManager* input = InputManager::Get();

            // A HUD running in World mode only accepts joystick
            // interactions; buttons and sliders need full UI focus. An
            // external pointer is the exception: it exists because something
            // is deliberately pointing at a surface, which is UI focus by
            // definition whatever the desktop is doing.
            bool joystickOnly = pointerSource == UiPointerSource::External
                ? false
                : !input->IsUiInteractable();

            const float pointerX = pointer.state.x;
            const float pointerY = pointer.state.y;
            const bool isDown = pointer.state.isDown;
            const bool justPressed = isDown && !pointer.wasDown;
            const bool justReleased = !isDown && pointer.wasDown;

            bool isDropdownConsumed = false;

            // Expanded dropdown hover and clicks (UI focus only).
            if (expandedDropdown != nullptr && !joystickOnly)
            {
                int optionIndex = HitTestDropdownOptions(expandedDropdown, pointerX, pointerY);
                if (optionIndex != expandedDropdown->GetHoveredOption())
                {
                    expandedDropdown->SetHoveredOption(optionIndex);
                }

                if (justPressed)
                {
                    if (optionIndex >= 0)
                    {
                        expandedDropdown->SetSelectedOption(optionIndex);
                        expandedDropdown->SetExpanded(false);
                        expandedDropdown->SetHoveredOption(-1);

                        const std::string& action = expandedDropdown->GetDataAction();
                        if (!action.empty())
                        {
                            callbackMap->InvokeDropdown(action, expandedDropdown->GetSelectedOption());
                        }

                        expandedDropdown = nullptr;
                        isDropdownConsumed = true;
                    }
                    else
                    {
                        // Clicked outside the option list — close it.
                        expandedDropdown->SetHoveredOption(-1);
                        expandedDropdown->SetExpanded(false);
                        expandedDropdown = nullptr;
                    }
                }
            }

            if (!isDropdownConsumed)
            {
                pointer.hoveredElement = HitTest(rootElement, pointerX, pointerY, joystickOnly);

                // Keyboard focus follows the platform pointer, so a click
                // after a hover carries on from where the hand was.
                if (pointerSource == UiPointerSource::Platform && pointer.hoveredElement != nullptr
                    && pointer.hoveredElement->IsNavigable() && pointer.hoveredElement->IsEnabled())
                {
                    for (int i = 0; i < (int)navigationList.size(); i++)
                    {
                        if (navigationList[i] == pointer.hoveredElement)
                        {
                            selectedIndex = i;
                            break;
                        }
                    }
                }

                if (justPressed && pointer.hoveredElement != nullptr
                    && pointer.hoveredElement->IsNavigable() && pointer.hoveredElement->IsEnabled())
                {
                    pointer.pressedElement = pointer.hoveredElement;
                }

                if (isDown && pointer.pressedElement != nullptr)
                {
                    if (pointer.pressedElement->GetType() == UiElementType::Slider)
                    {
                        UpdateSliderFromPointer(static_cast<UiSlider*>(pointer.pressedElement), pointerX);
                    }
                    else if (pointer.pressedElement->GetType() == UiElementType::Joystick)
                    {
                        UpdateJoystickFromPointer(static_cast<UiJoystick*>(pointer.pressedElement),
                                                  pointerX, pointerY);
                    }
                }

                if (justReleased && pointer.pressedElement != nullptr)
                {
                    UiElementType pressedType = pointer.pressedElement->GetType();
                    bool isDragType = pressedType == UiElementType::Slider
                                   || pressedType == UiElementType::Joystick;

                    if (pointer.pressedElement == pointer.hoveredElement && !isDragType)
                    {
                        ActivateElement(pointer.pressedElement);
                    }

                    if (pressedType == UiElementType::Joystick)
                    {
                        ReleaseJoystick(static_cast<UiJoystick*>(pointer.pressedElement));
                    }

                    pointer.pressedElement = nullptr;
                }
            }

            pointer.wasDown = isDown;
        }
    }

    // A pointer that has left the surface releases whatever it was holding
    // rather than leaving it stuck pressed.
    void UiInputHandler::ReleasePointer(int pointerId)
    {
        UiPointer& pointer = pointers[pointerId];

        if (pointer.pressedElement != nullptr
            && pointer.pressedElement->GetType() == UiElementType::Joystick)
        {
            ReleaseJoystick(static_cast<UiJoystick*>(pointer.pressedElement));
        }

        pointer.hoveredElement = nullptr;
        pointer.pressedElement = nullptr;
        pointer.wasDown = false;
    }

    // An element is pressed if any pointer presses it, hovered if any
    // pointer hovers it, and back to normal when none does.
    void UiInputHandler::ApplyPointerStates()
    {
        std::vector<UiElement*> nextStated;

        for (int pointer = 0; pointer < MAX_POINTERS; pointer++)
        {
            UiElement* pressed = pointers[pointer].pressedElement;
            if (pressed != nullptr && pressed->IsEnabled())
            {
                pressed->SetState(UiElementState::Pressed);
                nextStated.push_back(pressed);
            }
        }

        for (int pointer = 0; pointer < MAX_POINTERS; pointer++)
        {
            UiElement* hovered = pointers[pointer].hoveredElement;
            bool isAlreadyStated = hovered != nullptr
                && std::find(nextStated.begin(), nextStated.end(), hovered) != nextStated.end();

            if (hovered != nullptr && !isAlreadyStated
                && hovered->IsNavigable() && hovered->IsEnabled())
            {
                hovered->SetState(UiElementState::Hovered);
                nextStated.push_back(hovered);
            }
        }

        for (UiElement* element : statedElements)
        {
            bool isStillStated = std::find(nextStated.begin(), nextStated.end(), element) != nextStated.end();

            // Anything that has since been disabled keeps its Disabled
            // state rather than being pulled back to Normal.
            if (!isStillStated && element->GetState() != UiElementState::Disabled)
            {
                element->SetState(UiElementState::Normal);
            }
        }

        statedElements.swap(nextStated);
    }

    UiElement* UiInputHandler::HitTest(UiElement* element, float px, float py, bool joystickOnly)
    {
        UiElement* result = nullptr;

        if (element->IsVisible() && element->computedStyle.display != DisplayType::None)
        {
            // Test children in reverse order (front-to-back)
            const std::vector<UiElement*>& children = element->GetChildren();
            for (int i = (int)children.size() - 1; i >= 0 && result == nullptr; i--)
            {
                result = HitTest(children[i], px, py, joystickOnly);
            }

            // Test this element
            if (result == nullptr && element->IsNavigable() && element->layoutRect.Contains(px, py))
            {
                bool typeAllowed = !joystickOnly || element->GetType() == UiElementType::Joystick;
                if (typeAllowed)
                {
                    result = element;
                }
            }
        }

        return result;
    }

    // ========================
    // Keyboard/gamepad navigation
    // ========================

    void UiInputHandler::UpdateNavigation()
    {
        if (navigationList.empty())
        {
            return;
        }

        InputManager* input = InputManager::Get();

        // Check if an expanded dropdown has focus
        UiElement* selected = GetSelectedElement();
        if (selected && selected->GetType() == UiElementType::Dropdown)
        {
            UiDropdown* dropdown = static_cast<UiDropdown*>(selected);
            if (dropdown->IsExpanded())
            {
                UpdateDropdownNavigation(dropdown);
                return;
            }
        }

        if (input->EdgePositive(InputAction::UiNavigateDown))
        {
            SelectElement(selectedIndex + 1);
            AudioManager::Get()->PlaySfx(SfxId::UiMove);
        }
        else if (input->EdgePositive(InputAction::UiNavigateUp))
        {
            SelectElement(selectedIndex - 1);
            AudioManager::Get()->PlaySfx(SfxId::UiMove);
        }
        else if (input->EdgePositive(InputAction::UiNavigateRight))
        {
            selected = GetSelectedElement();
            if (selected && selected->GetType() == UiElementType::Slider)
            {
                UiSlider* slider = static_cast<UiSlider*>(selected);
                float step = (slider->GetMaxValue() - slider->GetMinValue()) * 0.05f;
                slider->SetCurrentValue(slider->GetCurrentValue() + step);
                const std::string& action = slider->GetDataAction();
                if (!action.empty())
                {
                    callbackMap->InvokeSlider(action, slider->GetCurrentValue());
                }
            }
            else
            {
                SelectElement(selectedIndex + 1);
            }
        }
        else if (input->EdgePositive(InputAction::UiNavigateLeft))
        {
            selected = GetSelectedElement();
            if (selected && selected->GetType() == UiElementType::Slider)
            {
                UiSlider* slider = static_cast<UiSlider*>(selected);
                float step = (slider->GetMaxValue() - slider->GetMinValue()) * 0.05f;
                slider->SetCurrentValue(slider->GetCurrentValue() - step);
                const std::string& action = slider->GetDataAction();
                if (!action.empty())
                {
                    callbackMap->InvokeSlider(action, slider->GetCurrentValue());
                }
            }
            else
            {
                SelectElement(selectedIndex - 1);
            }
        }

        if (input->EdgePositive(InputAction::UiConfirm))
        {
            selected = GetSelectedElement();
            if (selected)
            {
                selected->SetState(UiElementState::Pressed);
                ActivateElement(selected);
                selected->SetState(UiElementState::Hovered);
            }
        }
    }

    void UiInputHandler::UpdateDropdownNavigation(UiDropdown* dropdown)
    {
        InputManager* input = InputManager::Get();
        int optionCount = (int)dropdown->GetOptions().size();

        if (input->EdgePositive(InputAction::UiNavigateDown))
        {
            int next = dropdown->GetSelectedOption() + 1;
            if (next >= optionCount)
            {
                next = 0;
            }
            dropdown->SetSelectedOption(next);
        }
        else if (input->EdgePositive(InputAction::UiNavigateUp))
        {
            int prev = dropdown->GetSelectedOption() - 1;
            if (prev < 0)
            {
                prev = optionCount - 1;
            }
            dropdown->SetSelectedOption(prev);
        }

        if (input->EdgePositive(InputAction::UiConfirm))
        {
            dropdown->SetExpanded(false);
            expandedDropdown = nullptr;

            const std::string& action = dropdown->GetDataAction();
            if (!action.empty())
            {
                callbackMap->InvokeDropdown(action, dropdown->GetSelectedOption());
            }
        }
    }

    void UiInputHandler::SelectElement(int index)
    {
        if (navigationList.empty())
        {
            return;
        }

        // Deselect current
        UiElement* current = GetSelectedElement();
        if (current && current->GetState() == UiElementState::Hovered)
        {
            current->SetState(UiElementState::Normal);
        }

        // Wrap around
        if (index < 0)
        {
            index = (int)navigationList.size() - 1;
        }
        else if (index >= (int)navigationList.size())
        {
            index = 0;
        }

        selectedIndex = index;

        // Select new
        UiElement* newSelected = GetSelectedElement();
        if (newSelected && newSelected->IsEnabled())
        {
            newSelected->SetState(UiElementState::Hovered);
        }
    }

    UiElement* UiInputHandler::GetSelectedElement() const
    {
        if (selectedIndex >= 0 && selectedIndex < (int)navigationList.size())
        {
            return navigationList[selectedIndex];
        }
        return nullptr;
    }

    void UiInputHandler::ActivateElement(UiElement* element)
    {
        if (!element || !element->IsEnabled())
        {
            return;
        }

        const std::string& action = element->GetDataAction();
        if (action.empty())
        {
            return;
        }

        switch (element->GetType())
        {
        case UiElementType::Button:
        {
            // Suppress UiMove when this click triggers a screen transition or
            // an app-state transition — UiAdvance / UiBack play instead via
            // UiScreenSystem / StateMachine.
            bool wasUiTransitioning = surface->Screens().IsTransitioning();
            bool wasStateTransitioning = StateMachine::Get()->IsTransitioning();
            callbackMap->InvokeButton(action);
            bool didStartUiTransition = !wasUiTransitioning && surface->Screens().IsTransitioning();
            bool didStartStateTransition = !wasStateTransitioning && StateMachine::Get()->IsTransitioning();
            if (!didStartUiTransition && !didStartStateTransition)
            {
                AudioManager::Get()->PlaySfx(SfxId::UiMove);
            }
            break;
        }
        case UiElementType::Toggle:
        {
            UiToggle* toggle = static_cast<UiToggle*>(element);
            toggle->Toggle();
            callbackMap->InvokeToggle(action, toggle->IsChecked());
            AudioManager::Get()->PlaySfx(SfxId::UiMove);
            break;
        }
        case UiElementType::Dropdown:
        {
            UiDropdown* dropdown = static_cast<UiDropdown*>(element);
            dropdown->ToggleExpanded();
            expandedDropdown = dropdown->IsExpanded() ? dropdown : nullptr;
            break;
        }
        default:
            break;
        }
    }

    void UiInputHandler::UpdateSliderFromPointer(UiSlider* slider, float pointerX)
    {
        int surfaceWidth = 0;
        int surfaceHeight = 0;
        surface->GetSize(surfaceWidth, surfaceHeight);
        float scaleFactor = (float)surfaceHeight / 1080.0f;

        const UiRect& rect = slider->layoutRect;
        float trackPadding = slider->computedStyle.padding.left * scaleFactor;
        float trackStart = rect.x + trackPadding;
        float trackWidth = rect.width - trackPadding * 2.0f;

        if (trackWidth <= 0.0f)
        {
            return;
        }

        float normalized = (pointerX - trackStart) / trackWidth;
        if (normalized < 0.0f) normalized = 0.0f;
        if (normalized > 1.0f) normalized = 1.0f;

        float newValue = slider->GetMinValue() + normalized * (slider->GetMaxValue() - slider->GetMinValue());
        slider->SetCurrentValue(newValue);

        const std::string& action = slider->GetDataAction();
        if (!action.empty())
        {
            callbackMap->InvokeSlider(action, slider->GetCurrentValue());
        }
    }

    void UiInputHandler::UpdateJoystickFromPointer(UiJoystick* joystick, float pointerX, float pointerY)
    {
        const UiRect& rect = joystick->layoutRect;
        float halfWidth = rect.width * 0.5f;
        float halfHeight = rect.height * 0.5f;
        float radius = halfWidth < halfHeight ? halfWidth : halfHeight;

        float normalizedX = 0.0f;
        float normalizedY = 0.0f;

        if (radius > 0.0f)
        {
            float centerX = rect.x + halfWidth;
            float centerY = rect.y + halfHeight;

            float deltaX = (pointerX - centerX) / radius;
            // Screen y grows downward; negate so up = +y to match analog stick convention.
            float deltaY = -(pointerY - centerY) / radius;

            float magnitude = sqrtf(deltaX * deltaX + deltaY * deltaY);
            if (magnitude > 1.0f)
            {
                deltaX /= magnitude;
                deltaY /= magnitude;
            }

            normalizedX = deltaX;
            normalizedY = deltaY;
        }

        joystick->SetKnobNormalized(normalizedX, normalizedY);

        const std::string& action = joystick->GetDataAction();
        if (!action.empty())
        {
            callbackMap->InvokeJoystick(action, normalizedX, normalizedY);
        }
    }

    void UiInputHandler::ReleaseJoystick(UiJoystick* joystick)
    {
        joystick->SetKnobNormalized(0.0f, 0.0f);

        const std::string& action = joystick->GetDataAction();
        if (!action.empty())
        {
            callbackMap->InvokeJoystick(action, 0.0f, 0.0f);
        }
    }

    int UiInputHandler::HitTestDropdownOptions(UiDropdown* dropdown, float px, float py)
    {
        int surfaceWidth = 0;
        int surfaceHeight = 0;
        surface->GetSize(surfaceWidth, surfaceHeight);
        float scaleFactor = (float)surfaceHeight / 1080.0f;

        const UiRect& rect = dropdown->layoutRect;
        const UiStyleProperties& style = dropdown->computedStyle;
        float fontSize = style.hasFontSize ? style.fontSize * scaleFactor : 16.0f * scaleFactor;
        float optionHeight = fontSize * 1.5f;
        float optionY = rect.y + rect.height;
        int optionCount = (int)dropdown->GetOptions().size();

        if (px < rect.x || px > rect.x + rect.width)
        {
            return -1;
        }

        for (int i = 0; i < optionCount; i++)
        {
            if (py >= optionY && py <= optionY + optionHeight)
            {
                return i;
            }
            optionY += optionHeight;
        }

        return -1;
    }
}
