#ifndef XRMODECONTROLLER_H
#define XRMODECONTROLLER_H

#include "UiScreenController.h"

namespace CC
{
    class UiElement;
}

// Enters and leaves XR. Talks to the XR subsystem directly rather than
// through the state, so any state showing the pause menu gets the same
// control without wiring one up.
class XrModeController : public CC::UiScreenController
{
public:
    XrModeController();

    void Init() override;
    void OnEnter() override;

private:
    CC::UiElement* statusElement;
    CC::UiElement* toggleElement;

    void ToggleXr();
    void RefreshLabels();
};

#endif // XRMODECONTROLLER_H
