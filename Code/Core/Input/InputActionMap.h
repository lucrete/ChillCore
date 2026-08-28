#ifndef INPUTACTIONMAP_H
#define INPUTACTIONMAP_H

#include "InputTriggerMap.h"
#include <string>
#include <unordered_map>
#include <array>

namespace CC
{
    struct ActionDef
    {
        int action;
        std::string actionName;
        int trigger;

        ActionDef(int a, const std::string& name, int t) : action(a), actionName(name), trigger(t) {}
    };

    namespace InputAction
    {
        enum Action
        {
            DevReleaseMouse,
            DevToggleFullscreenQuad,
            DevToggleFreeCam,
            DevToggleFullscreen,
            DevFreeCamMoveForward,
            DevFreeCamMoveBackward,
            DevFreeCamMoveLeft,
            DevFreeCamMoveRight,
            DevFreeCamMoveUp,
            DevFreeCamMoveDown,
            DevReloadScene,
            DevToggleFrameProfile,
            DevToggleSceneHierarchy,
            DevToggleCommandConsole,
            DevMax,

            GameActionStart = 100,

            UiNavigateUp = 200,
            UiNavigateDown,
            UiNavigateLeft,
            UiNavigateRight,
            UiConfirm,
            UiCancel,
            UiActionMax
        };
    }

    class InputActionMap
    {
    public:
        static const int MAX_ACTIONS = 256;
        static const int UNBOUND = -1;

        InputActionMap(PlatformInput* physicalInput);

        void Update();

        void CreateContext(const std::string& context);
        void SetContext(const std::string& context);
        const std::string& GetContext() const;

        void RegisterAction(const std::string& context, const ActionDef& def);
        void RegisterAction(const ActionDef& def);

        bool IsPressed(int action);
        bool EdgePositive(int action);
        bool EdgeNegative(int action);

        int GetMaxActionIndex() const;
        bool IsBound(int action) const;
        const std::string& GetActionName(int action) const;
        const std::string& GetTriggerName(int action) const;
        std::string GetKeyComboString(int action) const;

        ActiveInputType GetActiveInputType() const;
        void NotifyTouchActivity();

    private:
        struct ActionBinding
        {
            int trigger = UNBOUND;
            std::string name;
        };

        InputTriggerMap* triggerMap;
        std::string currentContext;
        int maxActionIndex;

        std::unordered_map<std::string, std::array<ActionBinding, MAX_ACTIONS>> contexts;
        ActionBinding* activeBindings;

        void RegisterDevActions(std::array<ActionBinding, MAX_ACTIONS>& contextBindings);
        void UpdateMaxActionIndex();
    };
}

#endif // INPUTACTIONMAP_H