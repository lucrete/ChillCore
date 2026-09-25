#include "MainMenuController.h"
#include "UiSurface.h"
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
    CC::UiSurface* ui = GetSurface();

    ui->RegisterButtonAction("gotoOptions", [ui]()
    {
        ui->Screens().TransitionForward("Options");
    });

    ui->RegisterButtonAction("gotoShowcase", [ui]()
    {
        ui->Screens().TransitionForward("Showcase");
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

    ui->RegisterButtonAction("gotoAbout", [ui]()
    {
        ui->Screens().TransitionForward("About");
    });

    ui->RegisterButtonAction("quit", []()
    {
        CC::CoreMain::Get()->RequestQuit();
    });
}
