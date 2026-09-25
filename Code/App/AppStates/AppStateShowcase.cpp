#include "AppStateShowcase.h"
#include "FrameTimer.h"
#include "InputManager.h"
#include "PrintManager.h"
#include "CameraManager.h"
#include "MaterialManager.h"
#include "Material.h"
#include "PostProcess.h"
#include "RenderManager.h"
#include "SceneHierarchy.h"
#include "RenderableSphere.h"
#include "RotateRandom.h"
#include "UiManager.h"
#include "UiSurface.h"
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
    , pendingTogglePause(false)
    , gradePresetIndex(0)
    , procArtController(nullptr)
    , cameraStatic(nullptr)
    , quad01Object(nullptr)
    , cube01Object(nullptr)
{
    isPausable = true;
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

    // Paused binds only what resumes; menu navigation comes with every
    // context. Created first so the play context is the one left active.
    CC::InputActionMap& actionMap = CC::InputManager::Get()->GetActionMap();
    actionMap.CreateContext("AppStateShowcasePaused");
    actionMap.RegisterAction(CC::ActionDef(Pause, "Pause", CC::InputTrigger::GamepadStart));

    actionMap.CreateContext("AppStateShowcase");
    actionMap.RegisterAction(CC::ActionDef(Pause, "Pause", CC::InputTrigger::GamepadStart));
    actionMap.RegisterAction(CC::ActionDef(CycleGradePreset, "CycleGradePreset", CC::InputTrigger::GamepadFaceRight));

    CC::UiScreenSystem& screens = CC::UiManager::Get()->GetWindowSurface()->Screens();
    screens.RegisterScreen("ShowcaseHud", "Data/Ui/ShowcaseHud.html", "Data/Ui/ShowcaseHud.css",
        new ShowcaseHudController([this]() { pendingTogglePause = true; }));
    screens.RegisterScreen("PauseMenu", "Data/Ui/PauseMenu.html", "Data/Ui/PauseMenu.css",
        new PauseMenuController(
            [this]() { pendingTogglePause = true; },
            []() { CC::StateMachine::Get()->GotoState("Boot"); },
            []() { CC::CoreMain::Get()->RequestQuit(); }
        ));
    screens.RegisterScreen("Options", "Data/Ui/Options.html", "Data/Ui/Options.css",
        new OptionsController());
    screens.SetScreen("ShowcaseHud");

    ApplyPlayInputTarget();
    CC::InputManager::Get()->LockMouseCursor(true);

    pendingTogglePause = false;
    currentMode = InWorld;
    gradePresetIndex = 0;
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

    // The procedural material is built in MaterialManager and referenced by
    // name from the scene, so its parameters are set here rather than loaded.
    // Aspect stays at 1 for an in-world surface: the pattern follows the UVs,
    // not the window. RenderableFullscreenQuad overrides it for its own copy.
    CC::Material* proceduralVeins = CC::MaterialManager::Get()->GetMaterial("ProceduralVeins");
    proceduralVeins->SetUniform("aspectRatio", 1.0f);
    proceduralVeins->SetUniform("patternScale", 6.0f);
    proceduralVeins->SetUniform("pulseSpeed", 0.8f);
    proceduralVeins->SetUniform("veinSharpness", 7.0f);
    proceduralVeins->SetUniform("glowColour", CC::Vector3(4.0f, 1.4f, 0.5f));

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

    // The paused context binds nothing else, so play actions are read only
    // while playing.
    if (!IsPaused() && CC::InputManager::Get()->EdgePositive(CycleGradePreset))
    {
        ApplyNextGradePreset();
    }

    if (CC::InputManager::Get()->EdgePositive(CC::InputAction::DevReloadScene))
    {
        SceneShutdown();
        SceneInit();
    }

    if (!IsPaused() && CC::InputManager::Get()->EdgePositive(CC::InputAction::DevToggleFullscreenQuad))
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

    if (!IsPaused())
    {
        switch (currentMode)
        {
            case ProceduralArt:
                procArtController->Update();
                break;

            case InWorld:
            {
                if (quad01Object != nullptr)
                {
                    float time = CC::FrameTimer::Get()->SimulationTime();
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
}

void AppStateShowcase::OnPaused()
{
    CC::InputManager* input = CC::InputManager::Get();
    CC::UiSurface* windowSurface = CC::UiManager::Get()->GetWindowSurface();

    resumeContextName = input->GetActionMap().GetContext();
    input->GetActionMap().SetContext("AppStateShowcasePaused");

    // A finger on a stick when the menu opens never lifts off that stick,
    // so its last deflection would keep steering behind the menu.
    input->ClearAllJoystickOverrides();

    input->SetInputTargetSurface(CC::InputDomain::Window, windowSurface);
    windowSurface->Screens().SetScreen("PauseMenu");
}

void AppStateShowcase::OnResumed()
{
    CC::InputManager::Get()->GetActionMap().SetContext(resumeContextName);
    ApplyPlayInputTarget();
    CC::UiManager::Get()->GetWindowSurface()->Screens().SetScreen("ShowcaseHud");
}

void AppStateShowcase::ApplyPlayInputTarget()
{
    // On a touch device every input is a touch on the HUD, and the sticks
    // there steer; on the desktop the devices steer the scene directly.
#ifdef __ANDROID__
    CC::InputManager::Get()->SetInputTargetSurface(CC::InputDomain::Window,
                                                   CC::UiManager::Get()->GetWindowSurface());
#else
    CC::InputManager::Get()->SetInputTargetScene(CC::InputDomain::Window);
#endif
}

void AppStateShowcase::ApplyNextGradePreset()
{
    CC::PostProcess* postProcess = CC::RenderManager::Get()->GetPostProcess();
    int presetCount = postProcess->GetPresetCount();

    if (presetCount <= 0)
    {
        CCPrint(CC::PrintManager::CHANNEL_WARN, "AppStateShowcase: no grade presets to cycle");
    }
    else
    {
        gradePresetIndex = (gradePresetIndex + 1) % presetCount;

        const char* presetName = postProcess->GetPresetName(gradePresetIndex);
        postProcess->ApplyPreset(presetName);

        CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "AppStateShowcase: grade preset '%s'", presetName);
    }
}

void AppStateShowcase::Shutdown()
{
    CC::RenderManager::Get()->GetPostProcess()->DisableAllEffects();

    CC::UiManager::Get()->GetWindowSurface()->Screens().ClearAllScreens();

    SceneShutdown();

    procArtController->Shutdown();
    delete procArtController;
    procArtController = nullptr;

    delete cameraStatic;
    cameraStatic = nullptr;
}
