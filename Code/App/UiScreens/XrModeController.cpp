#include "XrModeController.h"

#include "UiElement.h"
#include "UiSurface.h"

#ifdef CC_ENABLE_XR
#include "XrManager.h"
#endif

namespace
{
    bool IsXrAvailable()
    {
        bool result = false;
#ifdef CC_ENABLE_XR
        result = CC::XrManager::Get() != nullptr && CC::XrManager::Get()->IsAvailable();
#endif
        return result;
    }

    bool IsXrActive()
    {
        bool result = false;
#ifdef CC_ENABLE_XR
        result = CC::XrManager::Get() != nullptr && CC::XrManager::Get()->IsSessionActive();
#endif
        return result;
    }
}

XrModeController::XrModeController()
    : statusElement(nullptr)
    , toggleElement(nullptr)
{
}

void XrModeController::Init()
{
    CC::UiSurface* ui = GetSurface();

    statusElement = ui->GetElementById("xrStatus");
    toggleElement = ui->GetElementById("xrToggle");

    ui->RegisterButtonAction("xrToggle", [this]() { ToggleXr(); });
    ui->RegisterButtonAction("xrBack", [ui]()
    {
        ui->Screens().TransitionBack();
    });

    RefreshLabels();
}

void XrModeController::OnEnter()
{
    RefreshLabels();
}

void XrModeController::ToggleXr()
{
#ifdef CC_ENABLE_XR
    CC::XrManager* xrManager = CC::XrManager::Get();
    if (xrManager != nullptr)
    {
        // Requests, not calls: a session may not start or stop with a render
        // pass open or a frame's views naming its targets.
        if (xrManager->IsSessionActive())
        {
            xrManager->RequestEndSession();
        }
        else
        {
            xrManager->RequestStartSession();
        }
    }
#endif

    RefreshLabels();
}

void XrModeController::RefreshLabels()
{
    if (statusElement != nullptr && toggleElement != nullptr)
    {
        if (IsXrActive())
        {
            statusElement->SetTextContent("Running in XR. The desktop is a mirror.");
            toggleElement->SetTextContent("Exit XR");
        }
        else if (IsXrAvailable())
        {
            statusElement->SetTextContent("Running on the desktop.");
            toggleElement->SetTextContent("Enter XR");
        }
        else
        {
            // Still offered: a headset connected after launch is detected on
            // the next attempt rather than being ruled out for the run.
            statusElement->SetTextContent("No headset found. Connect one and try again.");
            toggleElement->SetTextContent("Enter XR");
        }

        GetSurface()->InvalidateLayout();
    }
}
