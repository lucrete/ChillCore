#include "XrModeController.h"

#include "UiElement.h"
#include "UiManager.h"
#include "UiScreenSystem.h"

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
    CC::UiManager* uiManager = CC::UiManager::Get();

    statusElement = uiManager->GetElementById("xrStatus");
    toggleElement = uiManager->GetElementById("xrToggle");

    uiManager->RegisterButtonAction("xrToggle", [this]() { ToggleXr(); });
    uiManager->RegisterButtonAction("xrBack", []()
    {
        CC::UiScreenSystem::Get()->TransitionBack();
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

        CC::UiManager::Get()->InvalidateLayout();
    }
}
