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
    std::function<void()> onPauseCallback;
    CC::UiElement* moveStick = nullptr;
    CC::UiElement* lookStick = nullptr;
    CC::UiElement* trackingLabel = nullptr;
    bool joysticksVisible = false;
    bool isTrackingLabelVisible = false;
};

#endif // SHOWCASEHUDCONTROLLER_H
