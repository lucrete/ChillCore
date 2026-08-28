#include "StateMachine.h"
#include "CCAssert.h"
#include "FrameTimer.h"
#include "InputManager.h"
#include "RenderManager.h"
#include "AudioManager.h"
#include "stddef.h"

namespace CC
{
    StateMachine* StateMachine::instance = nullptr;

    StateMachine::StateMachine()
        : activeState(NULL)
        , transitionState(TransitionState::Idle)
        , fadeAlpha(0.0f)
    {
        CC_ASSERT(instance == nullptr, "StateMachine already created");
        instance = this;
    }

    StateMachine::~StateMachine()
    {
        if (NULL != activeState)
        {
            activeState->Shutdown();
            for (auto i = stateMap.begin(); i != stateMap.end(); i++)
            {
                delete(i->second);
            }
        }
        instance = nullptr;
    }

    StateMachine* StateMachine::Get()
    {
        CC_ASSERT(instance != nullptr, "StateMachine not created yet");
        return instance;
    }

    void StateMachine::RegisterState(std::string stateName, StateMachineState* state)
    {
        stateMap[stateName] = state;
    }

    void StateMachine::GotoState(std::string stateName)
    {
        // Initial state (no active state) — execute directly
        if (NULL == activeState)
        {
            activeState = stateMap[stateName];
            activeState->Init();
            return;
        }

        // During gameplay — defer the transition
        pendingState = stateName;
        AudioManager::Get()->PlaySfx(SfxId::UiAdvance);
    }

    bool StateMachine::IsTransitioning() const
    {
        return transitionState != TransitionState::Idle || !pendingState.empty();
    }

    void StateMachine::Update()
    {
        if (transitionState == TransitionState::Idle)
        {
            if (NULL != activeState)
            {
                activeState->Update();
            }

            if (!pendingState.empty())
            {
                transitionState = TransitionState::FadingOut;
                fadeAlpha = 0.0f;
                InputManager::Get()->SetInputBlocked(true);
            }
        }

        if (transitionState != TransitionState::Idle)
        {
            UpdateTransition();
        }
    }

    void StateMachine::UpdateTransition()
    {
        float deltaTime = FrameTimer::Get()->DeltaTime();

        switch (transitionState)
        {
        case TransitionState::FadingOut:
        {
            fadeAlpha += deltaTime / FADE_DURATION;
            if (fadeAlpha >= 1.0f)
            {
                fadeAlpha = 1.0f;
                transitionState = TransitionState::FullyFaded;
            }
            break;
        }

        case TransitionState::FullyFaded:
        {
            ExecuteStateSwap();
            transitionState = TransitionState::StateInitComplete;
            break;
        }

        case TransitionState::StateInitComplete:
        {
            // One-frame before starting fade to avoid a large delta time.
            transitionState = TransitionState::FadingIn;
            break;
        }

        case TransitionState::FadingIn:
        {
            fadeAlpha -= deltaTime / FADE_DURATION;
            if (fadeAlpha <= 0.0f)
            {
                fadeAlpha = 0.0f;
                transitionState = TransitionState::Idle;
                InputManager::Get()->SetInputBlocked(false);
            }
            break;
        }

        default:
            break;
        }

        RenderManager::Get()->SetScreenFadeAlpha(fadeAlpha);
    }

    void StateMachine::ExecuteStateSwap()
    {
        if (NULL != activeState)
        {
            activeState->Shutdown();
        }

        activeState = stateMap[pendingState];
        pendingState.clear();
        activeState->Init();
    }
}
