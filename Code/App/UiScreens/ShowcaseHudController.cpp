#include "ShowcaseHudController.h"

#include "UiManager.h"
#include "UiElement.h"
#include "InputManager.h"

ShowcaseHudController::ShowcaseHudController(std::function<void()> onPause)
    : onPauseCallback(onPause)
{
}

void ShowcaseHudController::Init()
{
    CC::InputManager* input = CC::InputManager::Get();
    CC::UiManager* ui = CC::UiManager::Get();

    moveStick = ui->GetElementById("moveStick");
    lookStick = ui->GetElementById("lookStick");

    ui->RegisterButtonAction("pause", onPauseCallback);

    // Initial visibility is platform-defined; ActiveInputType is unreliable
    // here because Init runs partway through MainMenu navigation on Android,
    // by which point taps have already fired mouse-position activity.
    // OnUpdate takes over for dynamic input-type transitions.
#ifdef __ANDROID__
    bool initialVisible = true;
#else
    bool initialVisible = false;
#endif
    if (moveStick != nullptr)
    {
        moveStick->SetVisible(initialVisible);
    }
    if (lookStick != nullptr)
    {
        lookStick->SetVisible(initialVisible);
    }
    joysticksVisible = initialVisible;

    ui->RegisterJoystickAction("move",
        [input](float x, float y)
        {
            input->SetJoystickOverride(CC::InputManager::STICK_LEFT, x, y);
        });

    ui->RegisterJoystickAction("look",
        [input](float x, float y)
        {
            // CameraFree multiplies right-stick X by a negative velocity for
            // non-mouse input types; the on-screen joystick's +x (push right)
            // needs the opposite sign so push-right looks right.
            input->SetJoystickOverride(CC::InputManager::STICK_RIGHT, x, -y);
        });
}

void ShowcaseHudController::OnUpdate()
{
    CC::InputManager* input = CC::InputManager::Get();
    bool isTouchActive = input->GetActiveInputType() == CC::ActiveInputType::Touch;

    if (isTouchActive != joysticksVisible)
    {
        if (moveStick != nullptr)
        {
            moveStick->SetVisible(isTouchActive);
        }
        if (lookStick != nullptr)
        {
            lookStick->SetVisible(isTouchActive);
        }

        // Layout skips invisible elements; toggling visible needs a re-run
        // so newly-shown elements get real rects instead of (0, 0, 0, 0).
        CC::UiManager::Get()->InvalidateLayout();

        // Drop any cached override values when the user has switched away
        // from touch — otherwise the last drag value would persist as the
        // stick reading until something else writes the channel.
        if (!isTouchActive)
        {
            input->ClearAllJoystickOverrides();
        }

        joysticksVisible = isTouchActive;
    }
}
