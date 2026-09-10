#include "OptionsController.h"
#include "UiSurface.h"
#include "RenderManager.h"
#include "UiToggle.h"
#include "UiDropdown.h"

static constexpr int MSAA_SAMPLE_VALUES[] = { 0, 2, 4, 8 };

void OptionsController::Init()
{
    CC::UiSurface* ui = GetSurface();

    ui->RegisterButtonAction("back", [ui]()
    {
        ui->Screens().TransitionBack();
    });

    ui->RegisterToggleAction("fullscreen", [](bool isChecked)
    {
        bool isCurrentlyFullscreen = CC::RenderManager::Get()->IsFullscreen();
        if (isChecked != isCurrentlyFullscreen)
        {
            CC::RenderManager::Get()->ToggleFullscreen();
        }
    });

    ui->RegisterDropdownAction("msaa", [](int selectedIndex)
    {
        CC::RenderManager* renderManager = CC::RenderManager::Get();
        if (selectedIndex == 0)
        {
            renderManager->SetAntialiasingEnabled(false);
        }
        else
        {
            renderManager->SetAntialiasingEnabled(true);
            renderManager->SetMsaaSamples(MSAA_SAMPLE_VALUES[selectedIndex]);
        }
    });
}

void OptionsController::OnEnter()
{
    CC::RenderManager* renderManager = CC::RenderManager::Get();
    CC::UiSurface* ui = GetSurface();

    CC::UiToggle* fullscreenToggle = static_cast<CC::UiToggle*>(ui->GetElementById("fullscreen"));
    if (fullscreenToggle != nullptr)
    {
        fullscreenToggle->SetChecked(renderManager->IsFullscreen());
    }

    CC::UiDropdown* msaaDropdown = static_cast<CC::UiDropdown*>(ui->GetElementById("msaa"));
    if (msaaDropdown != nullptr)
    {
        int dropdownIndex = 0;
        if (renderManager->IsAntialiasingEnabled())
        {
            int samples = renderManager->GetMsaaSamples();
            switch (samples)
            {
                case 2: dropdownIndex = 1; break;
                case 4: dropdownIndex = 2; break;
                case 8: dropdownIndex = 3; break;
                default: dropdownIndex = 0; break;
            }
        }
        msaaDropdown->SetSelectedOption(dropdownIndex);
    }
}
