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
        CycleGradePreset,
        ShowcaseActionMax
    };

    bool isPaused;
    bool pendingTogglePause;
    // Which grade preset the cycle key last applied. Held as an index because
    // the presets are loaded from a file and their names are not known here.
    int gradePresetIndex;
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
    void ApplyNextGradePreset();
    void UpdateTimerDisplay();
};

#endif // APPSTATESHOWCASE_H
