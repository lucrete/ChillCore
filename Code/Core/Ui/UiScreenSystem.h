#ifndef UISCREENSYSTEM_H
#define UISCREENSYSTEM_H

#include <string>
#include <unordered_map>
#include "UiScreenDef.h"

namespace CC
{
    class UiScreenSystem
    {
    public:
        UiScreenSystem();
        ~UiScreenSystem();
        static UiScreenSystem* Get();

        // ========================
        // Registration
        // ========================

        void RegisterScreen(const std::string& screenId, const std::string& htmlPath,
                            const std::string& cssPath, UiScreenController* controller);
        void ClearAllScreens();

        // ========================
        // Navigation
        // ========================

        void TransitionForward(const std::string& screenId);
        void TransitionBack();
        void SetScreen(const std::string& screenId);

        // ========================
        // State queries
        // ========================

        bool IsTransitioning() const;
        int GetStackDepth() const { return stackDepth; }

        // ========================
        // Per-frame
        // ========================

        void Update();

    private:
        static UiScreenSystem* instance;

        std::unordered_map<std::string, UiScreenDef> screenRegistry;

        // Screen stack
        static constexpr int MAX_STACK_DEPTH = 8;
        std::string screenStack[MAX_STACK_DEPTH];
        int stackDepth = 0;

        // Transition state machine
        enum class TransitionState
        {
            Idle,
            FadingOut,
            FadingIn
        };
        TransitionState transitionState = TransitionState::Idle;
        std::string pendingScreenId;
        bool isPendingPop = false;
        float fadeAlpha = 1.0f;
        static constexpr float FADE_DURATION = 0.25f;

        void ActivateCurrentScreen();
        void DeactivateCurrentScreen();
        UiScreenDef* FindScreenDef(const std::string& screenId);
    };
}

#endif // UISCREENSYSTEM_H
