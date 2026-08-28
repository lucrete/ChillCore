#include "ShowcaseController.h"
#include "UiManager.h"
#include "UiScreenSystem.h"
#include "PrintManager.h"

void ShowcaseController::Init()
{
    CC::UiManager* ui = CC::UiManager::Get();

    // Button callbacks
    ui->RegisterButtonAction("primary", []()
    {
        CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "Primary Action pressed!");
    });
    ui->RegisterButtonAction("secondary", []()
    {
        CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "Secondary Action pressed!");
    });
    ui->RegisterButtonAction("danger", []()
    {
        CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "Delete Item pressed!");
    });
    ui->RegisterButtonAction("prev", []()
    {
        CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "Prev pressed!");
    });
    ui->RegisterButtonAction("play", []()
    {
        CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "Play pressed!");
    });
    ui->RegisterButtonAction("next", []()
    {
        CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "Next pressed!");
    });
    ui->RegisterButtonAction("btnTextured", []()
    {
        CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "Textured Quad button pressed!");
    });
    ui->RegisterButtonAction("btnSlice9", []()
    {
        CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "Slice-9 button pressed!");
    });

    ui->RegisterButtonAction("back", []()
    {
        CC::UiScreenSystem::Get()->TransitionBack();
    });

    // Slider callbacks
    ui->RegisterSliderAction("volume", [](float value)
    {
        CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "Volume: %.1f", value);
    });
    ui->RegisterSliderAction("brightness", [](float value)
    {
        CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "Brightness: %.1f", value);
    });
    ui->RegisterSliderAction("fov", [](float value)
    {
        CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "FOV: %.1f", value);
    });

    // Toggle callbacks
    ui->RegisterToggleAction("fullscreen", [](bool checked)
    {
        CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "Fullscreen: %s", checked ? "ON" : "OFF");
    });
    ui->RegisterToggleAction("vsync", [](bool checked)
    {
        CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "VSync: %s", checked ? "ON" : "OFF");
    });
    ui->RegisterToggleAction("showfps", [](bool checked)
    {
        CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "Show FPS: %s", checked ? "ON" : "OFF");
    });
}

void ShowcaseController::OnEnter()
{
}
