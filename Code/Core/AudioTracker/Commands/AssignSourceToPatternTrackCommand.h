#ifndef ASSIGNSOURCETOPATTERNTRACKCOMMAND_H
#define ASSIGNSOURCETOPATTERNTRACKCOMMAND_H

#include <string>

#include "TrackerCommand.h"
#include "TrackSource.h"

namespace CC
{
    // Sets a pattern track's TrackSource to a new value, capturing the
    // previous source for Undo so the user can roll back through a
    // chain of source changes losslessly.
    class AssignSourceToPatternTrackCommand : public TrackerCommand
    {
    public:
        AssignSourceToPatternTrackCommand(const std::string& patternId, int trackIndex, const TrackSource& newSource);

        void Execute(TrackerProject& project) override;
        void Undo(TrackerProject& project) override;
        const char* DisplayLabel() const override { return "Assign source"; }

    private:
        std::string patternId;
        int         trackIndex;
        TrackSource newSource;
        TrackSource previousSource;       // captured on Execute
    };
}

#endif // ASSIGNSOURCETOPATTERNTRACKCOMMAND_H
