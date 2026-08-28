#ifndef APPSTATESHOWCASE_H
#define APPSTATESHOWCASE_H

#include "StateMachineState.h"
#include "InputActionMap.h"
#include "SceneObject.h"
#include "CameraStatic.h"
#include "ProceduralArtController.h"

class AppStateShowcase : public StateMachineState
{
public:
    AppStateShowcase();
    virtual ~AppStateShowcase();

    virtual void Init();
    virtual void Update();
    virtual void Shutdown();

private:
    enum Mode
    {
        ProceduralArt,
        InWorld
    };
    Mode currentMode;

    enum ShowcaseActions
    {
        Pause = CC::InputAction::GameActionStart,
        ShowcaseActionMax
    };

    bool isPaused;
    bool pendingTogglePause;
    bool wasMouseLocked;
    float elapsedTime;

    ProceduralArtController* procArtController;
    CC::CameraStatic* cameraStatic;
    CC::SceneObject* quad01Object;
    CC::SceneObject* cube01Object;

    void InitControls();
    void SceneInit();
    void SceneShutdown();
    void TogglePause();
    void UpdateTimerDisplay();
};

#endif // APPSTATESHOWCASE_H
