#include "ShowcaseHudController.h"

#include "UiSurface.h"
#include "UiElement.h"
#include "FrameTimer.h"
#include "InputManager.h"

#ifdef CC_ENABLE_XR
#include "XrManager.h"
#endif

#include <cstdio>

ShowcaseHudController::ShowcaseHudController(std::function<void()> onPause)
    : onPauseCallback(onPause)
{
}

void ShowcaseHudController::Init()
{
    CC::InputManager* input = CC::InputManager::Get();
    CC::UiSurface* ui = GetSurface();

    moveStick = ui->GetElementById("moveStick");
    lookStick = ui->GetElementById("lookStick");
    trackingLabel = ui->GetElementById("trackingXr");
    timerLabel = ui->GetElementById("timer");
    pauseButton = ui->GetElementById("pauseButton");

    ui->RegisterButtonAction("pause", onPauseCallback);

    if (trackingLabel != nullptr)
    {
        trackingLabel->SetVisible(false);
    }

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
    if (pauseButton != nullptr)
    {
        pauseButton->SetVisible(initialVisible);
    }
    areTouchControlsVisible = initialVisible;

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

    elapsedTime += CC::FrameTimer::Get()->SimulationDeltaTime();
    UpdateTimerDisplay();

    // The window is a mirror while an XR session is running.
    bool isMirroring = false;
#ifdef CC_ENABLE_XR
    isMirroring = CC::XrManager::Get() != nullptr && CC::XrManager::Get()->IsSessionRunning();
#endif
    if (isMirroring != isTrackingLabelVisible)
    {
        if (trackingLabel != nullptr)
        {
            trackingLabel->SetVisible(isMirroring);
        }
        GetSurface()->InvalidateLayout();
        isTrackingLabelVisible = isMirroring;
    }

    bool isTouchActive = input->GetActiveInputType() == CC::ActiveInputType::Touch;

    if (isTouchActive != areTouchControlsVisible)
    {
        if (moveStick != nullptr)
        {
            moveStick->SetVisible(isTouchActive);
        }
        if (lookStick != nullptr)
        {
            lookStick->SetVisible(isTouchActive);
        }
        if (pauseButton != nullptr)
        {
            pauseButton->SetVisible(isTouchActive);
        }

        // Layout skips invisible elements; toggling visible needs a re-run
        // so newly-shown elements get real rects instead of (0, 0, 0, 0).
        // The navigation list was collected at activation, so it is rebuilt
        // too or a hidden control stays reachable by gamepad.
        GetSurface()->InvalidateLayout();
        GetSurface()->GetInputHandler().BuildNavigationList();

        // Drop any cached override values when the user has switched away
        // from touch — otherwise the last drag value would persist as the
        // stick reading until something else writes the channel.
        if (!isTouchActive)
        {
            input->ClearAllJoystickOverrides();
        }

        areTouchControlsVisible = isTouchActive;
    }
}

void ShowcaseHudController::UpdateTimerDisplay()
{
    if (timerLabel != nullptr)
    {
        int totalSeconds = static_cast<int>(elapsedTime);
        int minutes = totalSeconds / 60;
        int seconds = totalSeconds % 60;

        char timeBuffer[16];
        snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", minutes, seconds);

        // Unchanged text is dropped by the setter, so this only marks the
        // surface once a second rather than every frame.
        timerLabel->SetTextContent(std::string(timeBuffer));
    }
}
