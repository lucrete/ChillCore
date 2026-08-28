#include "AssignSourceToPatternTrackCommand.h"

#include "TrackerProject.h"
#include "Pattern.h"
#include "PrintManager.h"

namespace CC
{
    AssignSourceToPatternTrackCommand::AssignSourceToPatternTrackCommand(
        const std::string& _patternId, int _trackIndex, const TrackSource& _newSource)
        : patternId(_patternId)
        , trackIndex(_trackIndex)
        , newSource(_newSource)
    {
    }

    void AssignSourceToPatternTrackCommand::Execute(TrackerProject& project)
    {
        Pattern* pattern = project.FindPattern(patternId);
        if (pattern == nullptr
            || trackIndex < 0
            || (size_t)trackIndex >= pattern->tracks.size())
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS,
                "AssignSourceToPatternTrackCommand: pattern '%s' track %d not found", patternId.c_str(), trackIndex);
        }
        else
        {
            PatternTrack& track = pattern->tracks[(size_t)trackIndex];
            previousSource = track.source;
            track.source   = newSource;
        }
    }

    void AssignSourceToPatternTrackCommand::Undo(TrackerProject& project)
    {
        Pattern* pattern = project.FindPattern(patternId);
        if (pattern != nullptr
            && trackIndex >= 0
            && (size_t)trackIndex < pattern->tracks.size())
        {
            pattern->tracks[(size_t)trackIndex].source = previousSource;
        }
    }
}
