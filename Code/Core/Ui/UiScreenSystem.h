#ifndef UISCREENSYSTEM_H
#define UISCREENSYSTEM_H

#include <string>
#include <unordered_map>
#include "UiScreenDef.h"

namespace CC
{
    class UiSurface;

    // The screen stack of one surface. Screens are instances rather than
    // templates: a definition holds its built tree, so it lives on the one
    // surface it was registered with. Two panels showing the same markup
    // register it on each.
    class UiScreenSystem
    {
    public:
        UiScreenSystem() = delete;
        explicit UiScreenSystem(UiSurface* surface);
        ~UiScreenSystem();

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

        // The transition fade, applied by the surface before its own draw
        // bracket so surfaces fade independently of each other.
        float GetFadeAlpha() const { return fadeAlpha; }

        // ========================
        // Per-frame
        // ========================

        void Update();

    private:
        UiSurface* surface = nullptr;

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
