#include "PauseMenuController.h"

#include "StateMachine.h"
#include "UiElement.h"
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

    xrModeElement = ui->GetElementById("xrMode");

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

void PauseMenuController::OnEnter()
{
    // Per activation rather than once: the same markup serves every state,
    // and which state is running is what decides whether XR is on offer.
    // A state that does not support it never routes to the XR screen, which
    // is also the state that never registered one.
    if (xrModeElement != nullptr)
    {
        StateMachineState* activeState = CC::StateMachine::Get()->GetActiveState();
        bool isXrOffered = activeState != nullptr && activeState->IsXrSupported();

        xrModeElement->SetVisible(isXrOffered);
        GetSurface()->InvalidateLayout();

        // The navigation list was collected when the screen was activated,
        // which is before this runs. Without rebuilding it a hidden entry is
        // still reachable with a gamepad.
        GetSurface()->GetInputHandler().BuildNavigationList();
    }
}
