#ifndef TOGGLESONGTRACKMUTEDCOMMAND_H
#define TOGGLESONGTRACKMUTEDCOMMAND_H

#include "TrackerCommand.h"

namespace CC
{
    class ToggleSongTrackMutedCommand : public TrackerCommand
    {
    public:
        explicit ToggleSongTrackMutedCommand(int songTrackIndex);

        void Execute(TrackerProject& project) override;
        void Undo(TrackerProject& project) override;
        const char* DisplayLabel() const override { return "Toggle song mute"; }

    private:
        int  songTrackIndex;
        bool previousMuted;
    };
}

#endif // TOGGLESONGTRACKMUTEDCOMMAND_H
