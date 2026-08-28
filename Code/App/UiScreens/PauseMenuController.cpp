#include "PauseMenuController.h"
#include "UiManager.h"
#include "UiScreenSystem.h"

PauseMenuController::PauseMenuController(std::function<void()> onResume, std::function<void()> onBackToMenu,
                                         std::function<void()> onQuit)
    : onResumeCallback(onResume)
    , onBackToMenuCallback(onBackToMenu)
    , onQuitCallback(onQuit)
{
}

void PauseMenuController::Init()
{
    CC::UiManager* uiManager = CC::UiManager::Get();

    uiManager->RegisterButtonAction("resume", onResumeCallback);
    uiManager->RegisterButtonAction("options", []()
    {
        CC::UiScreenSystem::Get()->TransitionForward("Options");
    });
    uiManager->RegisterButtonAction("backToMenu", onBackToMenuCallback);
    uiManager->RegisterButtonAction("quit", onQuitCallback);
}
