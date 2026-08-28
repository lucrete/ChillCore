#ifndef AUDIOTESTCONTROLLER_H
#define AUDIOTESTCONTROLLER_H

#include "UiScreenController.h"

namespace CC
{
    class UiElement;
    class Sound;
}

class AudioTestController : public CC::UiScreenController
{
public:
    AudioTestController();
    ~AudioTestController() override;

    void Init() override;

private:
    CC::UiElement* loopButton;
    CC::Sound* loopMusic;
    bool isLoopPlaying;
};

#endif // AUDIOTESTCONTROLLER_H
