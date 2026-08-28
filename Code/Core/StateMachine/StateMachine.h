#ifndef STATEMACHINE_H
#define STATEMACHINE_H

#include "StateMachineState.h"
#include <string>
#include <map>

namespace CC
{
    class StateMachine
    {
    public:
        StateMachine();
        virtual ~StateMachine();

        static StateMachine* Get();

        void RegisterState(std::string stateName, StateMachineState* state);
        void GotoState(std::string stateName);
        void Update();

        bool IsTransitioning() const;

    private:
        static StateMachine* instance;

        std::map<std::string, StateMachineState*> stateMap;
        StateMachineState* activeState;
        std::string pendingState;

        enum class TransitionState
        {
            Idle,
            FadingOut,
            FullyFaded,
            StateInitComplete,
            FadingIn
        };
        TransitionState transitionState = TransitionState::Idle;
        float fadeAlpha = 0.0f;
        static constexpr float FADE_DURATION = 0.4f;

        void UpdateTransition();
        void ExecuteStateSwap();
    };
}
#endif // STATEMACHINE_H
