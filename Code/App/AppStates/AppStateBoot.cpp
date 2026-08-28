#include "AppStateBoot.h"
#include "CoreMain.h"
#include "InputManager.h"
#include "PrintManager.h"
#include "CameraManager.h"
#include "SceneHierarchy.h"
#include "UiScreenSystem.h"
#include "StateMachine.h"
#include "MainMenuController.h"
#include "OptionsController.h"
#include "ShowcaseController.h"

AppStateBoot::AppStateBoot()
    : cameraStatic(nullptr)
{
}

AppStateBoot::~AppStateBoot()
{
}

void AppStateBoot::Init()
{
    cameraStatic = new CC::CameraStatic();
    CC::CameraManager::Get()->RegisterCamera("CameraStatic", cameraStatic);
    CC::CameraManager::Get()->SetActiveCamera("CameraStatic");

    SceneInit();

    CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "AppStateBoot::Init()");

    CC::InputActionMap& actionMap = CC::InputManager::Get()->GetActionMap();
    actionMap.CreateContext("AppStateBoot");
    actionMap.RegisterAction(CC::ActionDef(Quit, "Quit", CC::InputTrigger::GamepadStart));

    CC::UiScreenSystem* screens = CC::UiScreenSystem::Get();
    screens->RegisterScreen("MainMenu", "Data/Ui/MainMenu.html", "Data/Ui/MainMenu.css",
        new MainMenuController([]() { CC::StateMachine::Get()->GotoState("Showcase"); }));
    screens->RegisterScreen("Options", "Data/Ui/Options.html", "Data/Ui/Options.css", new OptionsController());
    screens->RegisterScreen("Showcase", "Data/Ui/Showcase.html", "Data/Ui/Showcase.css", new ShowcaseController());
    screens->SetScreen("MainMenu");
    CC::InputManager::Get()->SetInteractionMode(CC::InteractionMode::Ui);
}

void AppStateBoot::SceneInit()
{
    CC::SceneHierarchy::Get()->LoadFromFile("Data/Scenes/BootScene.yaml");
    CC::SceneHierarchy::Get()->Init();
    CC::SceneHierarchy::Get()->SetEnabled(true);
}

void AppStateBoot::SceneShutdown()
{
    CC::SceneHierarchy::Get()->Shutdown();
    CC::SceneHierarchy::Get()->Clear();
}

void AppStateBoot::Update()
{
    if (CC::InputManager::Get()->EdgePositive(Quit)
        && CC::UiScreenSystem::Get()->GetStackDepth() <= 1)
    {
        CC::CoreMain::Get()->RequestQuit();
    }
}

void AppStateBoot::Shutdown()
{
    CC::UiScreenSystem::Get()->ClearAllScreens();
    SceneShutdown();

    delete cameraStatic;
    cameraStatic = nullptr;
}
