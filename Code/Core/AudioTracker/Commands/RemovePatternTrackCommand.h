#ifndef REMOVEPATTERNTRACKCOMMAND_H
#define REMOVEPATTERNTRACKCOMMAND_H

#include <string>

#include "TrackerCommand.h"
#include "Pattern.h"

namespace CC
{
    // Removes a PatternTrack from a pattern, snapshotting the entire
    // track (source + velocities + gain + mute) so Undo restores it
    // exactly. The snapshot also pins the original index so re-insert
    // lands at the same row position.
    class RemovePatternTrackCommand : public TrackerCommand
    {
    public:
        RemovePatternTrackCommand(const std::string& patternId, int trackIndex);

        void Execute(TrackerProject& project) override;
        void Undo(TrackerProject& project) override;
        const char* DisplayLabel() const override { return "Remove track"; }

    private:
        std::string  patternId;
        int          trackIndex;
        PatternTrack removedTrack;       // captured by Execute for Undo
        bool         didExecute;
    };
}

#endif // REMOVEPATTERNTRACKCOMMAND_H
