#include "PauseMenuController.h"
#include "UiSurface.h"

PauseMenuController::PauseMenuController(std::function<void()> onResume, std::function<void()> onBackToMenu,
                                         std::function<void()> onQuit)
    : onResumeCallback(onResume)
    , onBackToMenuCallback(onBackToMenu)
    , onQuitCallback(onQuit)
{
}

void PauseMenuController::Init()
{
    CC::UiSurface* ui = GetSurface();

    ui->RegisterButtonAction("resume", onResumeCallback);
    ui->RegisterButtonAction("options", [ui]()
    {
        ui->Screens().TransitionForward("Options");
    });
    ui->RegisterButtonAction("xrMode", [ui]()
    {
        ui->Screens().TransitionForward("XrMode");
    });
    ui->RegisterButtonAction("backToMenu", onBackToMenuCallback);
    ui->RegisterButtonAction("quit", onQuitCallback);
}
