#include "AppMain.h"
#include "StateMachineState.h"
#include "AppStateBoot.h"
#include "AppStateShowcase.h"
#include "AppStateAudioTest.h"
#include "AppStateAudioTracker.h"
#include "AudioManager.h"

AppMain::AppMain()
{

}

AppMain::~AppMain()
{

}

void AppMain::Init()
{
    LoadGlobalUiSfx();

    stateMachine = new CC::StateMachine();
    stateMachine->RegisterState("Boot", new AppStateBoot());
    stateMachine->RegisterState("Showcase", new AppStateShowcase());
    stateMachine->RegisterState("AudioTest", new AppStateAudioTest());
    stateMachine->RegisterState("AudioTracker", new AppStateAudioTracker());
    stateMachine->GotoState("AudioTracker");
}

// Global UI SFX must load before any AppState runs so any state can be the
// initial GotoState target.
void AppMain::LoadGlobalUiSfx()
{
    CC::AudioManager* audio = CC::AudioManager::Get();
    audio->LoadSfx(CC::SfxId::UiMove,    "Data/Audio/UI/WHOOSH Fast Air (mono).wav");
    audio->LoadSfx(CC::SfxId::UiAdvance, "Data/Audio/UI/UI Animate Noise Glide Appear (stereo).wav");
    audio->LoadSfx(CC::SfxId::UiBack,    "Data/Audio/UI/UI Animate Noise Glide Disappear (stereo).wav");
}

void AppMain::Shutdown()
{
    delete(stateMachine);
}

void AppMain::Update()
{
    stateMachine->Update();
}