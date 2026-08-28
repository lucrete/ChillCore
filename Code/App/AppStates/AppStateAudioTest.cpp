#include "AppStateAudioTest.h"
#include "CoreMain.h"
#include "InputManager.h"
#include "PrintManager.h"
#include "CameraManager.h"
#include "UiScreenSystem.h"
#include "AudioTestController.h"

AppStateAudioTest::AppStateAudioTest()
    : cameraStatic(nullptr)
{
}

AppStateAudioTest::~AppStateAudioTest()
{
}

void AppStateAudioTest::Init()
{
    cameraStatic = new CC::CameraStatic();
    CC::CameraManager::Get()->RegisterCamera("CameraStatic", cameraStatic);
    CC::CameraManager::Get()->SetActiveCamera("CameraStatic");

    CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "AppStateAudioTest::Init()");

    CC::InputActionMap& actionMap = CC::InputManager::Get()->GetActionMap();
    actionMap.CreateContext("AppStateAudioTest");
    actionMap.RegisterAction(CC::ActionDef(Quit, "Quit", CC::InputTrigger::GamepadStart));

    CC::UiScreenSystem* screens = CC::UiScreenSystem::Get();
    screens->RegisterScreen("AudioTest", "Data/Ui/AudioTest.html", "Data/Ui/AudioTest.css",
        new AudioTestController());
    screens->SetScreen("AudioTest");
    CC::InputManager::Get()->SetInteractionMode(CC::InteractionMode::Ui);
}

void AppStateAudioTest::Update()
{
    if (CC::InputManager::Get()->EdgePositive(Quit)
        && CC::UiScreenSystem::Get()->GetStackDepth() <= 1)
    {
        CC::CoreMain::Get()->RequestQuit();
    }
}

void AppStateAudioTest::Shutdown()
{
    CC::UiScreenSystem::Get()->ClearAllScreens();

    delete cameraStatic;
    cameraStatic = nullptr;
}
