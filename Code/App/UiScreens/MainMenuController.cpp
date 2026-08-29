#include "MainMenuController.h"
#include "UiManager.h"
#include "UiScreenSystem.h"
#include "StateMachine.h"
#include "CoreMain.h"

MainMenuController::MainMenuController()
{
}

MainMenuController::MainMenuController(std::function<void()> onEnterShowcase)
    : onEnterShowcaseCallback(onEnterShowcase)
{
}

void MainMenuController::Init()
{
    CC::UiManager* ui = CC::UiManager::Get();

    ui->RegisterButtonAction("gotoOptions", []()
    {
        CC::UiScreenSystem::Get()->TransitionForward("Options");
    });

    ui->RegisterButtonAction("gotoShowcase", []()
    {
        CC::UiScreenSystem::Get()->TransitionForward("Showcase");
    });

    ui->RegisterButtonAction("gotoAudioTest", []()
    {
        CC::StateMachine::Get()->GotoState("AudioTest");
    });

    ui->RegisterButtonAction("gotoAudioTracker", []()
    {
        CC::StateMachine::Get()->GotoState("AudioTracker");
    });

    if (onEnterShowcaseCallback)
    {
        ui->RegisterButtonAction("enterWorld", onEnterShowcaseCallback);
    }

    ui->RegisterButtonAction("gotoAbout", []()
    {
        CC::UiScreenSystem::Get()->TransitionForward("About");
    });

    ui->RegisterButtonAction("quit", []()
    {
        CC::CoreMain::Get()->RequestQuit();
    });
}
