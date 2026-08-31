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
        CyclePostProcess,
        ShowcaseActionMax
    };

    // Off renders straight to the backbuffer. Tonemap is the cheapest stack
    // that still routes through the offscreen target, so it is the A/B against
    // the direct path. Full turns on everything the stack can do at once.
    enum class PostProcessMode
    {
        Off,
        Tonemap,
        Full
    };

    bool isPaused;
    bool pendingTogglePause;
    PostProcessMode postProcessMode;
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
    void CyclePostProcessMode();
    void UpdateTimerDisplay();
};

#endif // APPSTATESHOWCASE_H
