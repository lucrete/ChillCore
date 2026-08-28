#ifndef PLACEPATTERNINSONGCOMMAND_H
#define PLACEPATTERNINSONGCOMMAND_H

#include <string>

#include "TrackerCommand.h"

namespace CC
{
    // Sets songTracks[songTrackIndex].patternIdPerBar[barIndex] to a
    // pattern id (or to empty string for "no pattern"). Captures the
    // previous value for Undo.
    class PlacePatternInSongCommand : public TrackerCommand
    {
    public:
        PlacePatternInSongCommand(int songTrackIndex, int barIndex, const std::string& patternId);

        void Execute(TrackerProject& project) override;
        void Undo(TrackerProject& project) override;
        const char* DisplayLabel() const override { return "Place pattern"; }

    private:
        int         songTrackIndex;
        int         barIndex;
        std::string patternId;
        std::string previousPatternId;
        bool        didExecute;
    };
}

#endif // PLACEPATTERNINSONGCOMMAND_H
