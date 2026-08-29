#include "AboutController.h"

#include "BuildInfo.h"
#include "UiElement.h"
#include "UiManager.h"
#include "UiScreenSystem.h"

void AboutController::Init()
{
    CC::UiManager* ui = CC::UiManager::Get();

    ui->RegisterButtonAction("back", []()
    {
        CC::UiScreenSystem::Get()->TransitionBack();
    });

    // Build details are compiled in, so they are set once rather than per
    // activation.
    SetElementText("buildId", CC::BuildInfo::GetBuildId());
    SetElementText("commitHash", CC::BuildInfo::GetCommitHash());
    SetElementText("configuration", CC::BuildInfo::GetConfiguration());
    SetElementText("buildTimestamp", CC::BuildInfo::GetTimestamp());
}

void AboutController::SetElementText(const char* elementId, const char* text)
{
    CC::UiElement* element = CC::UiManager::Get()->GetElementById(elementId);

    if (element != nullptr)
    {
        element->SetTextContent(text);
    }
}
