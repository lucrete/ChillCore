#ifndef TOGGLEPATTERNTRACKMUTEDCOMMAND_H
#define TOGGLEPATTERNTRACKMUTEDCOMMAND_H

#include <string>

#include "TrackerCommand.h"

namespace CC
{
    class TogglePatternTrackMutedCommand : public TrackerCommand
    {
    public:
        TogglePatternTrackMutedCommand(const std::string& patternId, int trackIndex);

        void Execute(TrackerProject& project) override;
        void Undo(TrackerProject& project) override;
        const char* DisplayLabel() const override { return "Toggle mute"; }

    private:
        std::string patternId;
        int         trackIndex;
        bool        previousMuted;
    };
}

#endif // TOGGLEPATTERNTRACKMUTEDCOMMAND_H
