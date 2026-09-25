#ifndef SHOWCASEHUDCONTROLLER_H
#define SHOWCASEHUDCONTROLLER_H

#include <functional>

#include "UiScreenController.h"

namespace CC
{
    class UiElement;
}

class ShowcaseHudController : public CC::UiScreenController
{
public:
    explicit ShowcaseHudController(std::function<void()> onPause);

    void Init() override;
    void OnUpdate() override;

private:
    CC::UiElement* moveStick = nullptr;
    CC::UiElement* lookStick = nullptr;
    CC::UiElement* trackingLabel = nullptr;
    CC::UiElement* timerLabel = nullptr;
    CC::UiElement* pauseButton = nullptr;
    std::function<void()> onPauseCallback;
    bool areTouchControlsVisible = false;
    bool isTrackingLabelVisible = false;

    // Simulation time since this HUD was registered, which is the state's
    // own start. It stops with the world.
    float elapsedTime = 0.0f;

    void UpdateTimerDisplay();
};

#endif // SHOWCASEHUDCONTROLLER_H
