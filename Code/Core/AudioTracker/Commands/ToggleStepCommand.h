#ifndef TOGGLESTEPCOMMAND_H
#define TOGGLESTEPCOMMAND_H

#include <string>

#include "TrackerCommand.h"

namespace CC
{
    // Toggles a step's velocity between off (0.0) and the default
    // on-velocity (1.0). On undo restores the exact velocity that was
    // present before the toggle, so a step that had been hand-set to
    // e.g. 0.7 → cleared by a click → undone returns to 0.7, not 1.0.
    //
    // Velocity-as-on-bit: there is no separate is_on flag, so a
    // click toggles between 0 and a
    // default-on value. Vertical-drag adjustment of velocity lands
    // separately as SetStepVelocityCommand.
    class ToggleStepCommand : public TrackerCommand
    {
    public:
        ToggleStepCommand(const std::string& patternId, int trackIndex, int stepIndex);

        void Execute(TrackerProject& project) override;
        void Undo(TrackerProject& project) override;
        const char* DisplayLabel() const override { return "Toggle step"; }

    private:
        std::string patternId;
        int         trackIndex;
        int         stepIndex;
        float       previousVelocity;       // captured by Execute
    };
}

#endif // TOGGLESTEPCOMMAND_H
