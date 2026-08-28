#include "UiScreenSystem.h"
#include "UiScreenController.h"
#include "UiManager.h"
#include "UiElement.h"
#include "UiRenderer.h"
#include "TextRenderer.h"
#include "InputManager.h"
#include "InputActionMap.h"
#include "CCAssert.h"
#include "PrintManager.h"
#include "FrameTimer.h"
#include "AudioManager.h"

namespace CC
{
    UiScreenSystem* UiScreenSystem::instance = nullptr;

    UiScreenSystem::UiScreenSystem()
    {
        CC_ASSERT(instance == nullptr, "UiScreenSystem already created");
        instance = this;
    }

    UiScreenSystem::~UiScreenSystem()
    {
        ClearAllScreens();
        instance = nullptr;
    }

    UiScreenSystem* UiScreenSystem::Get()
    {
        CC_ASSERT(instance != nullptr, "UiScreenSystem not created yet");
        return instance;
    }

    // ========================
    // Registration
    // ========================

    void UiScreenSystem::RegisterScreen(const std::string& screenId, const std::string& htmlPath,
                                        const std::string& cssPath, UiScreenController* controller)
    {
        CC_ASSERT(controller != nullptr, "Screen controller is null");
        CC_ASSERT(screenRegistry.find(screenId) == screenRegistry.end(), "Screen already registered");

        // Insert into registry first so the callback map has a stable address
        UiScreenDef& screenDef = screenRegistry[screenId];
        screenDef.htmlPath = htmlPath;
        screenDef.cssPath = cssPath;
        screenDef.controller = controller;

        // Build the element tree at registration time. The CSS rules
        // are captured into screenDef so dynamic-subtree restyling
        // (UiManager::ApplyStylesToDynamicSubtree) works on this screen.
        screenDef.rootElement = UiManager::Get()->BuildScreen(htmlPath, cssPath, screenDef.cssRules);
        CC_ASSERT(screenDef.rootElement != nullptr, "Failed to build screen");

        // Temporarily make this tree active so controller->Init() can access elements via UiManager
        UiManager::Get()->LoadScreen(screenDef.rootElement, screenDef.cssRules);
        UiManager::Get()->GetInputHandler().SetCallbackMap(&screenDef.callbackMap);
        controller->Init();

        // Detach tree from UiManager (it stays owned by the screen def)
        UiManager::Get()->GetInputHandler().SetCallbackMap(nullptr);
        UiManager::Get()->LoadScreen(static_cast<UiElement*>(nullptr));

        CCPrint(PrintManager::CHANNEL_ALWAYS, "Screen registered: %s", screenId.c_str());
    }

    void UiScreenSystem::ClearAllScreens()
    {
        DeactivateCurrentScreen();

        for (auto& pair : screenRegistry)
        {
            if (pair.second.rootElement)
            {
                delete pair.second.rootElement;
                pair.second.rootElement = nullptr;
            }
            if (pair.second.controller)
            {
                delete pair.second.controller;
                pair.second.controller = nullptr;
            }
        }
        screenRegistry.clear();
        stackDepth = 0;
        transitionState = TransitionState::Idle;
        fadeAlpha = 1.0f;
    }

    // ========================
    // Navigation
    // ========================

    void UiScreenSystem::SetScreen(const std::string& screenId)
    {
        UiScreenDef* screenDef = FindScreenDef(screenId);
        CC_ASSERT(screenDef != nullptr, "Screen not found");

        DeactivateCurrentScreen();
        stackDepth = 0;
        screenStack[stackDepth++] = screenId;
        ActivateCurrentScreen();
        fadeAlpha = 1.0f;
        transitionState = TransitionState::Idle;
    }

    void UiScreenSystem::TransitionForward(const std::string& screenId)
    {
        if (transitionState == TransitionState::Idle)
        {
            CC_ASSERT(FindScreenDef(screenId) != nullptr, "Screen not found");
            CC_ASSERT(stackDepth < MAX_STACK_DEPTH, "Screen stack overflow");

            pendingScreenId = screenId;
            isPendingPop = false;
            transitionState = TransitionState::FadingOut;
            fadeAlpha = 1.0f;

            AudioManager::Get()->PlaySfx(SfxId::UiAdvance);
        }
    }

    void UiScreenSystem::TransitionBack()
    {
        if (transitionState == TransitionState::Idle && stackDepth > 1)
        {
            isPendingPop = true;
            transitionState = TransitionState::FadingOut;
            fadeAlpha = 1.0f;

            AudioManager::Get()->PlaySfx(SfxId::UiBack);
        }
    }

    // ========================
    // State queries
    // ========================

    bool UiScreenSystem::IsTransitioning() const
    {
        return transitionState != TransitionState::Idle;
    }

    // ========================
    // Per-frame
    // ========================

    void UiScreenSystem::Update()
    {
        float deltaTime = FrameTimer::Get()->DeltaTime();

        switch (transitionState)
        {
            case TransitionState::Idle:
            {
                if (stackDepth > 0)
                {
                    UiScreenDef* activeScreenDef = FindScreenDef(screenStack[stackDepth - 1]);
                    if (activeScreenDef && activeScreenDef->controller)
                    {
                        activeScreenDef->controller->OnUpdate();
                    }
                }

                // Auto-back on Cancel when stack has more than one screen
                if (stackDepth > 1 && InputManager::Get()->IsUiInteractable()
                    && InputManager::Get()->EdgePositive(InputAction::UiCancel))
                {
                    TransitionBack();
                }
                break;
            }

            case TransitionState::FadingOut:
            {
                fadeAlpha -= deltaTime / FADE_DURATION;
                if (fadeAlpha <= 0.0f)
                {
                    fadeAlpha = 0.0f;
                    DeactivateCurrentScreen();

                    if (isPendingPop)
                    {
                        stackDepth--;
                    }
                    else
                    {
                        screenStack[stackDepth++] = pendingScreenId;
                    }

                    ActivateCurrentScreen();
                    transitionState = TransitionState::FadingIn;
                }
                break;
            }

            case TransitionState::FadingIn:
            {
                fadeAlpha += deltaTime / FADE_DURATION;
                if (fadeAlpha >= 1.0f)
                {
                    fadeAlpha = 1.0f;
                    transitionState = TransitionState::Idle;
                }
                break;
            }
        }

        UiRenderer::Get()->SetGlobalAlpha(fadeAlpha);
        TextRenderer::Get()->SetGlobalAlpha(fadeAlpha);
    }

    // ========================
    // Private helpers
    // ========================

    void UiScreenSystem::ActivateCurrentScreen()
    {
        if (stackDepth > 0)
        {
            UiScreenDef* screenDef = FindScreenDef(screenStack[stackDepth - 1]);
            if (screenDef)
            {
                UiManager::Get()->LoadScreen(screenDef->rootElement, screenDef->cssRules);
                UiManager::Get()->GetInputHandler().SetCallbackMap(&screenDef->callbackMap);
                if (screenDef->controller)
                {
                    screenDef->controller->OnEnter();
                }
            }
        }
    }

    void UiScreenSystem::DeactivateCurrentScreen()
    {
        if (stackDepth > 0)
        {
            UiScreenDef* screenDef = FindScreenDef(screenStack[stackDepth - 1]);
            if (screenDef && screenDef->controller)
            {
                screenDef->controller->OnExit();
            }

            // Detach tree and callback map from UiManager without destroying them
            UiManager::Get()->GetInputHandler().SetCallbackMap(nullptr);
            UiManager::Get()->LoadScreen(static_cast<UiElement*>(nullptr));
        }
    }

    UiScreenDef* UiScreenSystem::FindScreenDef(const std::string& screenId)
    {
        UiScreenDef* result = nullptr;
        auto it = screenRegistry.find(screenId);
        if (it != screenRegistry.end())
        {
            result = &it->second;
        }
        return result;
    }
}
