#include "AppStateShowcase.h"
#include "FrameTimer.h"
#include "InputManager.h"
#include "PrintManager.h"
#include "CameraManager.h"
#include "MaterialManager.h"
#include "PostProcess.h"
#include "RenderManager.h"
#include "SceneHierarchy.h"
#include "RenderableSphere.h"
#include "RotateRandom.h"
#include "UiScreenSystem.h"
#include "UiManager.h"
#include "UiElement.h"
#include "CoreMain.h"
#include "StateMachine.h"
#include "ShowcaseHudController.h"
#include "PauseMenuController.h"
#include "OptionsController.h"
#include <cmath>
#include <cstdio>

AppStateShowcase::AppStateShowcase()
    : currentMode(InWorld)
    , isPaused(false)
    , pendingTogglePause(false)
    , postProcessMode(PostProcessMode::Tonemap)
    , wasMouseLocked(false)
    , elapsedTime(0.0f)
    , procArtController(nullptr)
    , cameraStatic(nullptr)
    , quad01Object(nullptr)
    , cube01Object(nullptr)
{
}

AppStateShowcase::~AppStateShowcase()
{
}

void AppStateShowcase::Init()
{
    cameraStatic = new CC::CameraStatic();
    CC::CameraManager::Get()->RegisterCamera("CameraStatic", cameraStatic);
    CC::CameraManager::Get()->SetActiveCamera("CameraFree");

    procArtController = new ProceduralArtController();

    SceneInit();

    CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "AppStateShowcase::Init()");

    CC::InputActionMap& actionMap = CC::InputManager::Get()->GetActionMap();
    actionMap.CreateContext("AppStateShowcase");
    actionMap.RegisterAction(CC::ActionDef(Pause, "Pause", CC::InputTrigger::GamepadStart));
    actionMap.RegisterAction(CC::ActionDef(CyclePostProcess, "CyclePostProcess", CC::InputTrigger::GamepadFaceRight));

    CC::UiScreenSystem* screens = CC::UiScreenSystem::Get();
    screens->RegisterScreen("ShowcaseHud", "Data/Ui/ShowcaseHud.html", "Data/Ui/ShowcaseHud.css",
        new ShowcaseHudController(
            [this]() { pendingTogglePause = true; }));
    screens->RegisterScreen("PauseMenu", "Data/Ui/PauseMenu.html", "Data/Ui/PauseMenu.css",
        new PauseMenuController(
            [this]() { pendingTogglePause = true; },
            []() { CC::StateMachine::Get()->GotoState("Boot"); },
            []() { CC::CoreMain::Get()->RequestQuit(); }
        ));
    screens->RegisterScreen("Options", "Data/Ui/Options.html", "Data/Ui/Options.css",
        new OptionsController());
    screens->SetScreen("ShowcaseHud");

    CC::InputManager::Get()->SetInteractionMode(CC::InteractionMode::World);
    CC::InputManager::Get()->LockMouseCursor(true);

    isPaused = false;
    pendingTogglePause = false;
    currentMode = InWorld;
    elapsedTime = 0.0f;
    postProcessMode = PostProcessMode::Tonemap;
}

void AppStateShowcase::SceneInit()
{
    CC::SceneHierarchy::Get()->LoadFromFile("Data/Scenes/ExampleScene.yaml");

    quad01Object = CC::SceneHierarchy::Get()->FindObjectByName("Quad01");

    cube01Object = new CC::SceneObject("Cube01");
    cube01Object->AddComponent(new CC::RenderableSphere(CC::MaterialManager::Get()->GetMaterial("BlueTestPattern")));
    cube01Object->AddComponent(new CC::RotateRandom());
    cube01Object->GetTransform().SetPosition(CC::Vector3(2, 4, -5));
    CC::SceneHierarchy::Get()->AddRootObject(cube01Object);

    CC::SceneHierarchy::Get()->Init();
    CC::SceneHierarchy::Get()->SetEnabled(true);
}

void AppStateShowcase::SceneShutdown()
{
    CC::SceneHierarchy::Get()->Shutdown();

    quad01Object = nullptr;
    cube01Object = nullptr;
    CC::SceneHierarchy::Get()->Clear();
}

void AppStateShowcase::InitControls()
{
    CC::InputManager::Get()->GetActionMap().SetContext("AppStateShowcase");
}

void AppStateShowcase::Update()
{
    if (pendingTogglePause)
    {
        pendingTogglePause = false;
        TogglePause();
    }

    if (CC::InputManager::Get()->EdgePositive(Pause))
    {
        TogglePause();
    }

    if (CC::InputManager::Get()->EdgePositive(CyclePostProcess))
    {
        CyclePostProcessMode();
    }

    if (CC::InputManager::Get()->EdgePositive(CC::InputAction::DevReloadScene))
    {
        SceneShutdown();
        SceneInit();
    }

    if (CC::InputManager::Get()->EdgePositive(CC::InputAction::DevToggleFullscreenQuad))
    {
        switch (currentMode)
        {
        case ProceduralArt:
            procArtController->Shutdown();
            InitControls();
            currentMode = InWorld;
            CC::SceneHierarchy::Get()->SetEnabled(true);
            break;

        case InWorld:
            procArtController->Init();
            currentMode = ProceduralArt;
            CC::SceneHierarchy::Get()->SetEnabled(false);
            break;

        default:
            break;
        }
        CC::RenderManager::Get()->ToggleFullscreenQuad();
    }

    if (!isPaused)
    {
        elapsedTime += CC::FrameTimer::Get()->DeltaTime();

        switch (currentMode)
        {
            case ProceduralArt:
                procArtController->Update();
                break;

            case InWorld:
            {
                if (quad01Object != nullptr)
                {
                    float time = CC::FrameTimer::Get()->TimeSinceStartup();
                    quad01Object->GetTransform().SetPosition(CC::Vector3((float)sin(time), 2.0f, 0.0f));
                    quad01Object->GetTransform().SetRotation(CC::Vector3(0.0f, 180 * (float)sin(time * 2), 0.0f));
                    float scale = 0.5f + 0.25f * (float)sin(time * 0.3f);
                    quad01Object->GetTransform().SetScale(CC::Vector3(scale, scale, scale));
                }

                break;
            }

            default:
                break;
        }
    }

    UpdateTimerDisplay();
}

void AppStateShowcase::TogglePause()
{
    isPaused = !isPaused;
    CC::SceneHierarchy::Get()->SetPaused(isPaused);

    if (isPaused)
    {
        wasMouseLocked = CC::InputManager::Get()->IsMouseCursorLocked();
        CC::InputManager::Get()->LockMouseCursor(false);
        CC::InputManager::Get()->SetInteractionMode(CC::InteractionMode::Ui);
        CC::UiScreenSystem::Get()->SetScreen("PauseMenu");
    }
    else
    {
        CC::InputManager::Get()->SetInteractionMode(CC::InteractionMode::World);
        CC::UiScreenSystem::Get()->SetScreen("ShowcaseHud");
        if (wasMouseLocked)
        {
            CC::InputManager::Get()->LockMouseCursor(true);
        }
    }
}

void AppStateShowcase::CyclePostProcessMode()
{
    CC::PostProcess* postProcess = CC::RenderManager::Get()->GetPostProcess();
    const char* modeName = "clamped";

    switch (postProcessMode)
    {
    case PostProcessMode::Clamped:
        postProcessMode = PostProcessMode::Tonemap;
        postProcess->DisableAllEffects();
        postProcess->GetEffect(CC::PostProcessEffectId::Tonemap).SetEnabled(true);
        modeName = "tonemap";
        break;

    case PostProcessMode::Tonemap:
        postProcessMode = PostProcessMode::Full;
        postProcess->GetEffect(CC::PostProcessEffectId::Bloom).SetEnabled(true);
        postProcess->GetEffect(CC::PostProcessEffectId::Vignette).SetEnabled(true);
        postProcess->ApplyPreset("Warm");
        modeName = "bloom + vignette + warm grade";
        break;

    case PostProcessMode::Full:
        postProcessMode = PostProcessMode::Clamped;
        postProcess->DisableAllEffects();
        modeName = "clamped";
        break;

    default:
        break;
    }

    CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "AppStateShowcase: post-process %s", modeName);
}

void AppStateShowcase::UpdateTimerDisplay()
{
    CC::UiElement* timerElement = CC::UiManager::Get()->GetElementById("timer");
    if (timerElement != nullptr)
    {
        int totalSeconds = static_cast<int>(elapsedTime);
        int minutes = totalSeconds / 60;
        int seconds = totalSeconds % 60;

        char timeBuffer[16];
        snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", minutes, seconds);
        timerElement->SetTextContent(std::string(timeBuffer));
    }
}

void AppStateShowcase::Shutdown()
{
    CC::RenderManager::Get()->GetPostProcess()->DisableAllEffects();

    CC::UiScreenSystem::Get()->ClearAllScreens();

    CC::SceneHierarchy::Get()->SetPaused(false);
    SceneShutdown();

    procArtController->Shutdown();
    delete procArtController;
    procArtController = nullptr;

    delete cameraStatic;
    cameraStatic = nullptr;
}
