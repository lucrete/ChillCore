#include "AudioTestController.h"

#include "UiManager.h"
#include "UiElement.h"
#include "PrintManager.h"
#include "AudioManager.h"
#include "StateMachine.h"

AudioTestController::AudioTestController()
    : loopButton(nullptr)
    , loopMusic(nullptr)
    , isLoopPlaying(false)
{
}

AudioTestController::~AudioTestController()
{
    if (loopMusic != nullptr)
    {
        CC::AudioManager::Get()->StopMusic(loopMusic);
        loopMusic = nullptr;
        isLoopPlaying = false;
    }
}

void AudioTestController::Init()
{
    CC::UiManager* ui = CC::UiManager::Get();

    loopButton = ui->GetElementById("loopButton");

    ui->RegisterButtonAction("playOneShot", []()
    {
        CCPrint(CC::PrintManager::CHANNEL_ALWAYS, "AudioTest: playOneShot");
        CC::AudioManager::Get()->PlaySfx("Data/Audio/UiAppear.wav");
    });

    ui->RegisterButtonAction("backToMainMenu", []()
    {
        CC::StateMachine::Get()->GotoState("Boot");
    });

    ui->RegisterButtonAction("toggleLoop", [this]()
    {
        if (isLoopPlaying)
        {
            CC::AudioManager::Get()->StopMusic(loopMusic);
            loopMusic = nullptr;
            isLoopPlaying = false;
        }
        else
        {
            loopMusic = CC::AudioManager::Get()->PlayMusic("Data/Audio/loop001.ogg", true);
            isLoopPlaying = (loopMusic != nullptr);
        }

        if (loopButton != nullptr)
        {
            loopButton->SetTextContent(isLoopPlaying ? "Stop Loop" : "Start Loop");
        }
        CCPrint(CC::PrintManager::CHANNEL_ALWAYS,
            isLoopPlaying ? "AudioTest: toggleLoop -> ON" : "AudioTest: toggleLoop -> OFF");
    });
}
