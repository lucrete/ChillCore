#ifndef APPSTATEXRDEMO_H
#define APPSTATEXRDEMO_H

#include "StateMachineState.h"
#include "GfxHandles.h"
#include "InputActionMap.h"
#include "SceneObject.h"
#include "UiScreenController.h"
#include "XrTypes.h"

#include <string>

namespace CC
{
    class UiElement;
    class UiWorldPanel;
}

// The panel's screen controller. Holds the readout line so the state can say
// what the hands are doing without reaching into the element tree itself.
class XrPanelController : public CC::UiScreenController
{
public:
    XrPanelController();

    virtual void Init() override;

    void SetReadout(const std::string& text);

private:
    CC::UiElement* readoutElement;
    std::string    pendingReadout;
};

// Tracked controllers driving a scene: two hands, a pointer ray from each,
// objects that can be picked up and put down, and a UI panel standing in the
// world that the ray operates.
//
// Boots directly. Nothing here assumes a menu ran first.
class AppStateXrDemo : public StateMachineState
{
public:
    AppStateXrDemo();
    virtual ~AppStateXrDemo();

    virtual void Init();
    virtual void Update();
    virtual void Shutdown();

private:
    enum XrDemoActions
    {
        Grab = CC::InputAction::GameActionStart,
        GrabSecondary,
        Select,
        SelectSecondary,
        Pause,
        PauseXrLeft,
        PauseXrRight,
        XrDemoActionMax
    };

    static const int HAND_COUNT = (int)CC::XrHand::Max;
    static const int GRABBABLE_COUNT = 4;

    // How close the grip has to be to pick something up. Generous: the grip
    // pose sits inside the fist, so a hand that looks like it is touching an
    // object is already some way inside it.
    static constexpr float GRAB_RADIUS = 0.25f;

    static constexpr float RAY_LENGTH = 3.0f;
    static constexpr float RAY_THICKNESS = 0.006f;

    // The panel in world space, in metres, and the surface behind it. The
    // surface is far denser than the panel is wide because text read from
    // half a metre away shows every pixel.
    static constexpr float PANEL_WIDTH = 1.2f;
    static constexpr float PANEL_HEIGHT = 0.9f;
    static const int PANEL_SURFACE_WIDTH = 1024;
    static const int PANEL_SURFACE_HEIGHT = 768;

    static constexpr float HAPTIC_AMPLITUDE = 0.4f;
    static constexpr float HAPTIC_DURATION_SECONDS = 0.03f;

    // Hand state
    CC::SceneObject* handObject[HAND_COUNT];
    CC::SceneObject* rayObject[HAND_COUNT];
    CC::SceneObject* heldObject[HAND_COUNT];
    bool             wasHoveringPanel[HAND_COUNT];

    CC::SceneObject* grabbableObject[GRABBABLE_COUNT];
    CC::Vector3      grabbableHomePosition[GRABBABLE_COUNT];

    // Panel. The component owns its surface, its target and its quad; the
    // state only places it and registers a screen on it.
    CC::SceneObject*   panelObject;
    CC::UiWorldPanel*  panelComponent;
    CC::Vector3        panelPosition;

    bool isXrActive;
    bool isPaused;
    bool pendingTogglePause;
    XrPanelController* panelController;

    // What the readout last said. Rewriting the same string every frame
    // dirties layout every frame for no change.
    std::string lastReadout;
    float lastReadoutTime;

    // Which grade preset the panel button last applied. An index because the
    // presets are loaded from a file and their names are not known here.
    int gradePresetIndex;

    static constexpr float READOUT_INTERVAL_SECONDS = 0.25f;

    void InitControls();
    void SceneInit();
    void SceneShutdown();

    void UpdateHands();
    void UpdateRay(CC::XrHand hand, const CC::TrackedPose& aimPose);
    void UpdateGrab(CC::XrHand hand, const CC::TrackedPose& gripPose);
    // Offers each hand's aim ray to the pointer router, which decides which
    // panel — if any — the ray reaches. Nothing here knows where a panel is.
    void SubmitPanelPointer(CC::XrHand hand, const CC::TrackedPose& aimPose);

    void RecentreGrabbables();
    void ApplyNextGradePreset();
    void UpdateReadout();
    void TogglePause();
    void ShowSceneScreen();
    void UpdatePauseInput();

    // Distance from the nearest tracked grip to the nearest sphere. Reported
    // on the panel so a grab that never fires can be told apart from a hand
    // that was never close enough.
    float NearestGrabbableDistance() const;
    bool IsReadoutDue();
};

#endif // APPSTATEXRDEMO_H
