#include "InputActionMap.h"
#include "CCAssert.h"

namespace CC
{
    InputActionMap::InputActionMap(PlatformInput* physicalInput)
        : activeBindings(nullptr)
        , maxActionIndex(0)
    {
        triggerMap = new InputTriggerMap(physicalInput);
    }

    void InputActionMap::Update()
    {
        triggerMap->Update();
    }

    void InputActionMap::CreateContext(const std::string& context)
    {
        auto& bindings = contexts[context];
        for (auto& binding : bindings)
        {
            binding.trigger = UNBOUND;
            binding.name.clear();
        }
        SetContext(context);
        RegisterDevActions(bindings);
    }

    void InputActionMap::SetContext(const std::string& context)
    {
        CC_ASSERT(contexts.find(context) != contexts.end(), "Context does not exist");
        currentContext = context;
        activeBindings = contexts[context].data();
        UpdateMaxActionIndex();
    }

    const std::string& InputActionMap::GetContext() const
    {
        return currentContext;
    }

    void InputActionMap::RegisterAction(const std::string& context, const ActionDef& def)
    {
        CC_ASSERT(contexts.find(context) != contexts.end(), "Context does not exist");
        CC_ASSERT(def.action >= 0 && def.action < MAX_ACTIONS, "Action out of range");
        contexts[context][def.action].trigger = def.trigger;
        contexts[context][def.action].name = def.actionName;

        if (context == currentContext)
        {
            UpdateMaxActionIndex();
        }
    }

    void InputActionMap::RegisterAction(const ActionDef& def)
    {
        CC_ASSERT(activeBindings != nullptr, "No context set");
        CC_ASSERT(def.action >= 0 && def.action < MAX_ACTIONS, "Action out of range");
        activeBindings[def.action].trigger = def.trigger;
        activeBindings[def.action].name = def.actionName;
        UpdateMaxActionIndex();
    }

    bool InputActionMap::IsPressed(int action)
    {
        CC_ASSERT(activeBindings != nullptr, "No context set");
        CC_ASSERT(action >= 0 && action < MAX_ACTIONS, "Action out of range");
        int trigger = activeBindings[action].trigger;
        CC_ASSERT(trigger != UNBOUND, "Action not bound");
        return triggerMap->IsTriggered(trigger);
    }

    bool InputActionMap::EdgePositive(int action)
    {
        CC_ASSERT(activeBindings != nullptr, "No context set");
        CC_ASSERT(action >= 0 && action < MAX_ACTIONS, "Action out of range");
        int trigger = activeBindings[action].trigger;
        CC_ASSERT(trigger != UNBOUND, "Action not bound");
        return triggerMap->EdgePositive(trigger);
    }

    bool InputActionMap::EdgeNegative(int action)
    {
        CC_ASSERT(activeBindings != nullptr, "No context set");
        CC_ASSERT(action >= 0 && action < MAX_ACTIONS, "Action out of range");
        int trigger = activeBindings[action].trigger;
        CC_ASSERT(trigger != UNBOUND, "Action not bound");
        return triggerMap->EdgeNegative(trigger);
    }

    int InputActionMap::GetMaxActionIndex() const
    {
        return maxActionIndex;
    }

    bool InputActionMap::IsBound(int action) const
    {
        CC_ASSERT(activeBindings != nullptr, "No context set");
        CC_ASSERT(action >= 0 && action < MAX_ACTIONS, "Action out of range");
        return activeBindings[action].trigger != UNBOUND;
    }

    const std::string& InputActionMap::GetActionName(int action) const
    {
        CC_ASSERT(activeBindings != nullptr, "No context set");
        CC_ASSERT(action >= 0 && action < MAX_ACTIONS, "Action out of range");
        return activeBindings[action].name;
    }

    const std::string& InputActionMap::GetTriggerName(int action) const
    {
        CC_ASSERT(activeBindings != nullptr, "No context set");
        CC_ASSERT(action >= 0 && action < MAX_ACTIONS, "Action out of range");
        int trigger = activeBindings[action].trigger;
        CC_ASSERT(trigger != UNBOUND, "Action not bound");
        return triggerMap->GetTriggerName(trigger);
    }

    std::string InputActionMap::GetKeyComboString(int action) const
    {
        CC_ASSERT(activeBindings != nullptr, "No context set");
        CC_ASSERT(action >= 0 && action < MAX_ACTIONS, "Action out of range");
        int trigger = activeBindings[action].trigger;
        CC_ASSERT(trigger != UNBOUND, "Action not bound");
        return triggerMap->GetKeyComboString(trigger);
    }

    void InputActionMap::RegisterDevActions(std::array<ActionBinding, MAX_ACTIONS>& contextBindings)
    {
        contextBindings[InputAction::DevReleaseMouse] = { InputTrigger::DevF1, "DevReleaseMouse" };
        contextBindings[InputAction::DevToggleFullscreenQuad] = { InputTrigger::DevF2, "DevToggleFullscreenQuad" };
        contextBindings[InputAction::DevToggleFreeCam] = { InputTrigger::DevF9, "DevToggleFreeCam" };
        contextBindings[InputAction::DevToggleFullscreen] = { InputTrigger::DevF10, "DevToggleFullscreen" };
        contextBindings[InputAction::DevFreeCamMoveForward] = { InputTrigger::GamepadDpadUp, "DevFreeCamMoveForward" };
        contextBindings[InputAction::DevFreeCamMoveBackward] = { InputTrigger::GamepadDpadDown, "DevFreeCamMoveBackward" };
        contextBindings[InputAction::DevFreeCamMoveLeft] = { InputTrigger::GamepadDpadLeft, "DevFreeCamMoveLeft" };
        contextBindings[InputAction::DevFreeCamMoveRight] = { InputTrigger::GamepadDpadRight, "DevFreeCamMoveRight" };
        contextBindings[InputAction::DevFreeCamMoveUp] = { InputTrigger::GamepadBumperLeft, "DevFreeCamMoveUp" };
        contextBindings[InputAction::DevFreeCamMoveDown] = { InputTrigger::GamepadBumperRight, "DevFreeCamMoveDown" };
        contextBindings[InputAction::DevReloadScene] = { InputTrigger::DevCtrlF5, "DevReloadScene" };
        contextBindings[InputAction::DevToggleFrameProfile] = { InputTrigger::DevF3, "DevToggleFrameProfile" };
        contextBindings[InputAction::DevToggleSceneHierarchy] = { InputTrigger::DevF4, "DevToggleSceneHierarchy" };
        contextBindings[InputAction::DevToggleCommandConsole] = { InputTrigger::DevGraveAccent, "DevToggleCommandConsole" };

        // UI navigation actions
        contextBindings[InputAction::UiNavigateUp] = { InputTrigger::DevArrowUp, "UiNavigateUp" };
        contextBindings[InputAction::UiNavigateDown] = { InputTrigger::DevArrowDown, "UiNavigateDown" };
        contextBindings[InputAction::UiNavigateLeft] = { InputTrigger::DevArrowLeft, "UiNavigateLeft" };
        contextBindings[InputAction::UiNavigateRight] = { InputTrigger::DevArrowRight, "UiNavigateRight" };
        contextBindings[InputAction::UiConfirm] = { InputTrigger::DevEnter, "UiConfirm" };
        contextBindings[InputAction::UiCancel] = { InputTrigger::DevEscape, "UiCancel" };
    }

    void InputActionMap::UpdateMaxActionIndex()
    {
        CC_ASSERT(activeBindings != nullptr, "No context set");
        maxActionIndex = 0;
        for (int i = MAX_ACTIONS - 1; i >= 0; i--)
        {
            if (activeBindings[i].trigger != UNBOUND)
            {
                maxActionIndex = i;
                break;
            }
        }
    }

    ActiveInputType InputActionMap::GetActiveInputType() const
    {
        return triggerMap->GetActiveInputType();
    }

    void InputActionMap::NotifyTouchActivity()
    {
        triggerMap->NotifyTouchActivity();
    }
}