#include "UiInputHandler.h"

#include "UiElement.h"
#include "UiSlider.h"
#include "UiToggle.h"
#include "UiDropdown.h"
#include "UiJoystick.h"
#include "InputManager.h"
#include "RenderManager.h"
#include "PrintManager.h"
#include "AudioManager.h"
#include "UiScreenSystem.h"
#include "StateMachine.h"

#include <cmath>

namespace CC
{
    UiInputHandler::UiInputHandler()
    {
    }

    UiInputHandler::~UiInputHandler()
    {
    }

    void UiInputHandler::SetCallbackMap(UiCallbackMap* map)
    {
        callbackMap = map;
    }

    void UiInputHandler::SetRootElement(UiElement* root)
    {
        if (hoveredElement)
        {
            hoveredElement->SetState(UiElementState::Normal);
        }
        if (pressedElement)
        {
            pressedElement->SetState(UiElementState::Normal);
        }

        rootElement = root;
        hoveredElement = nullptr;
        pressedElement = nullptr;
        expandedDropdown = nullptr;
        selectedIndex = -1;
        navigationList.clear();
        for (int i = 0; i < PlatformInput::MAX_TOUCH_POINTERS; i++)
        {
            joystickTouchTracks[i] = nullptr;
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

    void UiInputHandler::Update()
    {
        if (!rootElement || !callbackMap)
        {
            return;
        }

        UpdateMouse();
        UpdateTouchPointers();

        // Keyboard/gamepad navigation is only meaningful when the screen is
        // the focus (UI mode). HUD overlays running in World mode get mouse
        // and touch input only.
        if (InputManager::Get()->IsUiInteractable())
        {
            UpdateNavigation();
        }
    }

    // ========================
    // Mouse interaction
    // ========================

    void UiInputHandler::UpdateMouse()
    {
        InputManager* input = InputManager::Get();

        if (input->IsMouseCursorLocked())
        {
            return;
        }

        // World-mode HUD overlays only accept joystick interactions.
        // Other elements (buttons, sliders, etc.) need full UI focus.
        bool joystickOnly = !input->IsUiInteractable();

        int mouseX, mouseY;
        input->GetMousePosition(mouseX, mouseY);

        bool isMouseDown = input->IsMouseButtonDown(MouseButton::Left);
        bool mouseJustPressed = isMouseDown && !wasMouseDown;
        bool mouseJustReleased = !isMouseDown && wasMouseDown;

        // Handle expanded dropdown hover and clicks (UI mode only)
        if (expandedDropdown && !joystickOnly)
        {
            int optionIndex = HitTestDropdownOptions(expandedDropdown, (float)mouseX, (float)mouseY);
            expandedDropdown->SetHoveredOption(optionIndex);

            if (mouseJustPressed)
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
                    wasMouseDown = isMouseDown;
                    return;
                }
                else
                {
                    // Clicked outside the option list — close it
                    expandedDropdown->SetHoveredOption(-1);
                    expandedDropdown->SetExpanded(false);
                    expandedDropdown = nullptr;
                }
            }
        }

        // Hit test
        UiElement* hitElement = HitTest(rootElement, (float)mouseX, (float)mouseY, joystickOnly);

        // Update hover state
        if (hitElement != hoveredElement)
        {
            if (hoveredElement && hoveredElement->GetState() == UiElementState::Hovered)
            {
                hoveredElement->SetState(UiElementState::Normal);
            }
            hoveredElement = hitElement;
            if (hoveredElement && hoveredElement->IsNavigable() && hoveredElement->IsEnabled() &&
                hoveredElement->GetState() == UiElementState::Normal)
            {
                hoveredElement->SetState(UiElementState::Hovered);

                // Sync navigation selection to hovered element
                for (int i = 0; i < (int)navigationList.size(); i++)
                {
                    if (navigationList[i] == hoveredElement)
                    {
                        selectedIndex = i;
                        break;
                    }
                }
            }
        }

        // Press state
        if (mouseJustPressed && hoveredElement && hoveredElement->IsNavigable() && hoveredElement->IsEnabled())
        {
            pressedElement = hoveredElement;
            pressedElement->SetState(UiElementState::Pressed);
        }

        // Slider drag: update value while mouse is held
        if (isMouseDown && pressedElement && pressedElement->GetType() == UiElementType::Slider)
        {
            UpdateSliderFromMouse(static_cast<UiSlider*>(pressedElement), (float)mouseX);
        }

        // Joystick drag: update knob and invoke callback while mouse is held
        if (isMouseDown && pressedElement && pressedElement->GetType() == UiElementType::Joystick)
        {
            UpdateJoystickFromMouse(static_cast<UiJoystick*>(pressedElement),
                (float)mouseX, (float)mouseY);
        }

        // Release -> activate (or snap joystick back to centre)
        if (mouseJustReleased && pressedElement)
        {
            UiElementType pressedType = pressedElement->GetType();
            bool isDragType = pressedType == UiElementType::Slider || pressedType == UiElementType::Joystick;

            if (pressedElement == hoveredElement && !isDragType)
            {
                ActivateElement(pressedElement);
            }

            if (pressedType == UiElementType::Joystick)
            {
                ReleaseJoystick(static_cast<UiJoystick*>(pressedElement));
            }

            if (pressedElement->GetState() == UiElementState::Pressed)
            {
                pressedElement->SetState(hoveredElement == pressedElement ?
                    UiElementState::Hovered : UiElementState::Normal);
            }
            pressedElement = nullptr;
        }

        wasMouseDown = isMouseDown;
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
            bool wasUiTransitioning = UiScreenSystem::Get()->IsTransitioning();
            bool wasStateTransitioning = StateMachine::Get()->IsTransitioning();
            callbackMap->InvokeButton(action);
            bool didStartUiTransition = !wasUiTransitioning && UiScreenSystem::Get()->IsTransitioning();
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

    void UiInputHandler::UpdateSliderFromMouse(UiSlider* slider, float mouseX)
    {
        int screenWidth, screenHeight;
        RenderManager::Get()->GetWindowSize(screenWidth, screenHeight);
        float scaleFactor = (float)screenHeight / 1080.0f;

        const UiRect& rect = slider->layoutRect;
        float trackPadding = slider->computedStyle.padding.left * scaleFactor;
        float trackStart = rect.x + trackPadding;
        float trackWidth = rect.width - trackPadding * 2.0f;

        if (trackWidth <= 0.0f)
        {
            return;
        }

        float normalized = (mouseX - trackStart) / trackWidth;
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

    // ========================
    // Multi-touch (extra fingers beyond the primary mouse pointer)
    // ========================

    void UiInputHandler::UpdateTouchPointers()
    {
        InputManager* input = InputManager::Get();

        // Slot 0 is the mouse abstraction and is already driven by
        // UpdateMouse. Slots 1..MAX-1 are extra fingers.
        for (int slot = 1; slot < PlatformInput::MAX_TOUCH_POINTERS; slot++)
        {
            bool active = input->IsTouchPointerActive(slot);
            UiJoystick* tracked = joystickTouchTracks[slot];

            if (active)
            {
                int x = 0;
                int y = 0;
                input->GetTouchPointer(slot, x, y);

                // First time we see this finger: hit-test for a joystick.
                // Non-joystick UI is reachable via the primary pointer only.
                if (tracked == nullptr)
                {
                    UiElement* hit = HitTest(rootElement, (float)x, (float)y, true);
                    if (hit != nullptr && hit->GetType() == UiElementType::Joystick)
                    {
                        tracked = static_cast<UiJoystick*>(hit);
                        joystickTouchTracks[slot] = tracked;
                    }
                }

                if (tracked != nullptr)
                {
                    UpdateJoystickFromMouse(tracked, (float)x, (float)y);
                }
            }
            else if (tracked != nullptr)
            {
                ReleaseJoystick(tracked);
                joystickTouchTracks[slot] = nullptr;
            }
        }
    }

    void UiInputHandler::UpdateJoystickFromMouse(UiJoystick* joystick, float mouseX, float mouseY)
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

            float deltaX = (mouseX - centerX) / radius;
            // Screen y grows downward; negate so up = +y to match analog stick convention.
            float deltaY = -(mouseY - centerY) / radius;

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
        int screenWidth, screenHeight;
        RenderManager::Get()->GetWindowSize(screenWidth, screenHeight);
        float scaleFactor = (float)screenHeight / 1080.0f;

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
