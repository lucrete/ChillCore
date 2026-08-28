#ifndef APPSTATEAUDIOTRACKER_H
#define APPSTATEAUDIOTRACKER_H
#include "StateMachineState.h"
#include "InputActionMap.h"
#include "CameraStatic.h"
#include "TrackerProject.h"

namespace CC
{
    class TrackerEngine;
}

class AppStateAudioTracker : public StateMachineState
{
public:
    AppStateAudioTracker();
    virtual ~AppStateAudioTracker();

    virtual void Init();
    virtual void Update();
    virtual void Shutdown();

private:
    enum AppStateAudioTrackerActions
    {
        Quit = CC::InputAction::GameActionStart,
        Undo,
        Redo,
        AppStateAudioTrackerActionMax
    };

    CC::CameraStatic*  cameraStatic;
    CC::TrackerEngine* trackerEngine;

    // Owned by AppState as a value type. Save / Open swap the contents,
    // not the address; controllers hold a pointer that stays valid for
    // the AppState lifetime.
    CC::TrackerProject project;
};

#endif // APPSTATEAUDIOTRACKER_H
