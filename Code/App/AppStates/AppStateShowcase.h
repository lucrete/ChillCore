#ifndef APPSTATESHOWCASE_H
#define APPSTATESHOWCASE_H

#include <string>

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

protected:
    virtual void OnPaused() override;
    virtual void OnResumed() override;

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

    bool pendingTogglePause;
    // Which grade preset the cycle key last applied. Held as an index because
    // the presets are loaded from a file and their names are not known here.
    int gradePresetIndex;

    // The context in use when pause began. Procedural-art mode has its own,
    // so resuming cannot assume the in-world one.
    std::string resumeContextName;

    ProceduralArtController* procArtController;
    CC::CameraStatic* cameraStatic;
    CC::SceneObject* quad01Object;
    CC::SceneObject* cube01Object;

    void InitControls();
    void SceneInit();
    void SceneShutdown();
    void ApplyPlayInputTarget();
    void ApplyNextGradePreset();
};

#endif // APPSTATESHOWCASE_H
