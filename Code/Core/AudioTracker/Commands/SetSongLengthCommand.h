#ifndef SETSONGLENGTHCOMMAND_H
#define SETSONGLENGTHCOMMAND_H

#include <string>
#include <vector>

#include "TrackerCommand.h"

namespace CC
{
    // Resizes songLengthBars and every SongTrack.patternIdPerBar to
    // match. Growing appends empty bars; shrinking truncates the tail
    // and snapshots the dropped entries so Undo restores them.
    class SetSongLengthCommand : public TrackerCommand
    {
    public:
        explicit SetSongLengthCommand(int newLengthBars);

        void Execute(TrackerProject& project) override;
        void Undo(TrackerProject& project) override;
        const char* DisplayLabel() const override { return "Set song length"; }

    private:
        int newLengthBars;
        int previousLengthBars;

        // Per-song-track snapshot of the bars dropped on shrink (empty
        // when growing). Indexed by song-track-index, holds the tail
        // entries that were beyond newLengthBars.
        std::vector<std::vector<std::string>> droppedTails;
    };
}

#endif // SETSONGLENGTHCOMMAND_H
