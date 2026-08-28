#ifndef APPSTATEAUDIOTEST_H
#define APPSTATEAUDIOTEST_H
#include "StateMachineState.h"
#include "InputActionMap.h"
#include "CameraStatic.h"

class AppStateAudioTest : public StateMachineState
{
public:
    AppStateAudioTest();
    virtual ~AppStateAudioTest();

    virtual void Init();
    virtual void Update();
    virtual void Shutdown();

private:
    enum AppStateAudioTestActions
    {
        Quit = CC::InputAction::GameActionStart,
        AppStateAudioTestActionMax
    };

    CC::CameraStatic* cameraStatic;
};

#endif // APPSTATEAUDIOTEST_H
