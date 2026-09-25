#include "StateMachineState.h"

#include "CCAssert.h"
#include "FrameTimer.h"
#include "SceneHierarchy.h"

// ========================
// Pause
// ========================
void StateMachineState::SetPaused(bool doPause)
{
    CC_ASSERT(isPausable, "This state has no pause");

    if (doPause != isPaused)
    {
        isPaused = doPause;

        // Paused, not disabled. Renderables submit to the render list from
        // their own component update, so disabling the hierarchy would stop
        // the scene being drawn rather than stop it moving.
        CC::SceneHierarchy::Get()->SetPaused(isPaused);
        CC::FrameTimer::Get()->SetSimulationPaused(isPaused);

        if (isPaused)
        {
            OnPaused();
        }
        else
        {
            OnResumed();
        }
    }
}

void StateMachineState::TogglePause()
{
    SetPaused(!isPaused);
}
