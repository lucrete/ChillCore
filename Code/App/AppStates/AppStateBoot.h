#ifndef APPSTATEBOOT_H
#define APPSTATEBOOT_H
#include "StateMachineState.h"
#include "InputActionMap.h"
#include "CameraStatic.h"

class AppStateBoot : public StateMachineState
{
public:
    AppStateBoot();
    virtual ~AppStateBoot();

    virtual void Init();
    virtual void Update();
    virtual void Shutdown();

private:
    enum AppStateBootActions
    {
        Quit = CC::InputAction::GameActionStart,
        AppStateBootActionMax
    };

    void SceneInit();
    void SceneShutdown();

    CC::CameraStatic* cameraStatic;
};

#endif // APPSTATEBOOT_H
