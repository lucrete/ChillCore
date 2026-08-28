#include "AppStateAudioTracker.h"
#include "CoreMain.h"
#include "InputManager.h"
#include "PrintManager.h"
#include "CameraManager.h"
#include "UiScreenSystem.h"
#include "AudioTrackerController.h"
#include "TrackerEngine.h"
#include "TrackerCommandHistory.h"
#include "SampleLibrary.h"
#include "PlatformWindow.h"

AppStateAudioTracker::AppStateAudioTracker()
    : cameraStatic(nullptr)
    , trackerEngine(nullptr)
{
}

AppStateAudioTracker::~AppStateAudioTracker()
{
}

void AppStateAudioTracker::Init()
{
    cameraStatic = new CC::CameraStatic();
    CC::CameraManager::Get()->RegisterCamera("CameraStatic", cameraStatic);
    CC::CameraManager::Get()->SetActiveCamera("CameraStatic");

    trackerEngine = new CC::TrackerEngine();
    trackerEngine->Init();

    // Drag a WAV/OGG onto the window to import it into the sample
    // library. Each path is offered to SampleLibrary::Import; results
    // are logged so the user has feedback while the panel UI is still
    // pending.
    CC::PlatformWindow::Get()->SetFileDropCallback([](const std::vector<std::string>& paths)
    {
        bool didImportAny = false;
        for (size_t i = 0; i < paths.size(); i++)
        {
            std::string newId;
            if (CC::SampleLibrary::Get()->Import(paths[i], newId))
            {
                CCPrint(CC::PrintManager::CHANNEL_ALWAYS,
                    "AudioTracker: imported '%s' as id '%s'", paths[i].c_str(), newId.c_str());
                didImportAny = true;
            }
            else
            {
                CCPrint(CC::PrintManager::CHANNEL_ALWAYS,
                    "AudioTracker: failed to import '%s' (see preceding log line)", paths[i].c_str());
            }
        }

        if (didImportAny)
        {
            CC::TrackerEngine::Get()->SyncWithLibrary();
        }
    });

    CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "AppStateAudioTracker::Init()");

    CC::InputActionMap& actionMap = CC::InputManager::Get()->GetActionMap();
    actionMap.CreateContext("AppStateAudioTracker");
    actionMap.RegisterAction(CC::ActionDef(Quit, "Quit", CC::InputTrigger::GamepadStart));
    actionMap.RegisterAction(CC::ActionDef(Undo, "Undo", CC::InputTrigger::EditUndo));
    actionMap.RegisterAction(CC::ActionDef(Redo, "Redo", CC::InputTrigger::EditRedo));

    // Default project: one empty pattern + one empty 8-bar song
    // track so both editors have something to render. Add Track /
    // Add Pattern / Add Song Track grow the project from there.
    project = CC::TrackerProject();
    {
        CC::Pattern defaultPattern;
        defaultPattern.id          = "P1";
        defaultPattern.displayName = "Pattern 1";
        defaultPattern.barCount    = 1;
        defaultPattern.stepsPerBar = 16;
        project.patterns.push_back(defaultPattern);
        project.currentPatternIndex = 0;

        CC::SongTrack defaultSongTrack;
        defaultSongTrack.displayName = "Track 1";
        defaultSongTrack.patternIdPerBar.assign((size_t)project.songLengthBars, std::string());
        project.songTracks.push_back(defaultSongTrack);
    }

    CC::UiScreenSystem* screens = CC::UiScreenSystem::Get();
    screens->RegisterScreen("AudioTracker", "Data/Ui/AudioTracker.html", "Data/Ui/AudioTracker.css",
        new AudioTrackerController(&project));
    screens->SetScreen("AudioTracker");
    CC::InputManager::Get()->SetInteractionMode(CC::InteractionMode::Ui);
}

void AppStateAudioTracker::Update()
{
    if (CC::InputManager::Get()->EdgePositive(Quit)
        && CC::UiScreenSystem::Get()->GetStackDepth() <= 1)
    {
        CC::CoreMain::Get()->RequestQuit();
    }

    // Ctrl+Z / Ctrl+Y. Routed at the AppState level so undo/redo work
    // regardless of which sub-panel currently has focus, per the
    // plan's editing-focus-irrelevant guidance.
    if (CC::InputManager::Get()->EdgePositive(Undo))
    {
        CC::TrackerCommandHistory::Get()->Undo(project);
    }
    if (CC::InputManager::Get()->EdgePositive(Redo))
    {
        CC::TrackerCommandHistory::Get()->Redo(project);
    }
}

void AppStateAudioTracker::Shutdown()
{
    CC::PlatformWindow::Get()->SetFileDropCallback(nullptr);

    CC::UiScreenSystem::Get()->ClearAllScreens();

    if (trackerEngine != nullptr)
    {
        trackerEngine->Shutdown();
        delete trackerEngine;
        trackerEngine = nullptr;
    }

    delete cameraStatic;
    cameraStatic = nullptr;
}
