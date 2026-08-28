#ifndef ADDSONGTRACKCOMMAND_H
#define ADDSONGTRACKCOMMAND_H

#include "TrackerCommand.h"

namespace CC
{
    // Appends a new SongTrack with patternIdPerBar sized to the
    // project's current songLengthBars and all bars empty. Undo
    // removes it.
    class AddSongTrackCommand : public TrackerCommand
    {
    public:
        AddSongTrackCommand();

        void Execute(TrackerProject& project) override;
        void Undo(TrackerProject& project) override;
        const char* DisplayLabel() const override { return "Add song track"; }

    private:
        int insertedTrackIndex;
    };
}

#endif // ADDSONGTRACKCOMMAND_H
