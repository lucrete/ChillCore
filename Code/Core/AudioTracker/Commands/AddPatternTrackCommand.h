#ifndef ADDPATTERNTRACKCOMMAND_H
#define ADDPATTERNTRACKCOMMAND_H

#include <string>

#include "TrackerCommand.h"
#include "TrackSource.h"

namespace CC
{
    // Appends a new PatternTrack (with its velocities sized for the
    // pattern's current step count, all zero) to the named pattern.
    // First concrete TrackerCommand — establishes the pattern that
    // every subsequent project mutation follows: edits never touch
    // TrackerProject directly, they construct a command and hand it
    // to TrackerCommandHistory::Apply.
    class AddPatternTrackCommand : public TrackerCommand
    {
    public:
        explicit AddPatternTrackCommand(const std::string& patternId);

        void Execute(TrackerProject& project) override;
        void Undo(TrackerProject& project) override;
        const char* DisplayLabel() const override { return "Add track"; }

    private:
        std::string patternId;

        // Captured by Execute so Undo knows what to remove. Set to -1
        // until Execute runs successfully; Undo no-ops if invalid so
        // an Apply that failed pattern lookup is harmless to redo /
        // undo through.
        int insertedTrackIndex;
    };
}

#endif // ADDPATTERNTRACKCOMMAND_H
