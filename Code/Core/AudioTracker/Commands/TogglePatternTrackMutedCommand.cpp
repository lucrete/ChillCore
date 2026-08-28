#include "TogglePatternTrackMutedCommand.h"

#include "TrackerProject.h"
#include "Pattern.h"
#include "PrintManager.h"

namespace CC
{
    TogglePatternTrackMutedCommand::TogglePatternTrackMutedCommand(const std::string& _patternId, int _trackIndex)
        : patternId(_patternId)
        , trackIndex(_trackIndex)
        , previousMuted(false)
    {
    }

    void TogglePatternTrackMutedCommand::Execute(TrackerProject& project)
    {
        Pattern* pattern = project.FindPattern(patternId);
        if (pattern == nullptr
            || trackIndex < 0
            || (size_t)trackIndex >= pattern->tracks.size())
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS,
                "TogglePatternTrackMutedCommand: pattern '%s' track %d not found", patternId.c_str(), trackIndex);
        }
        else
        {
            PatternTrack& track = pattern->tracks[(size_t)trackIndex];
            previousMuted  = track.isMuted;
            track.isMuted  = !previousMuted;
        }
    }

    void TogglePatternTrackMutedCommand::Undo(TrackerProject& project)
    {
        Pattern* pattern = project.FindPattern(patternId);
        if (pattern != nullptr
            && trackIndex >= 0
            && (size_t)trackIndex < pattern->tracks.size())
        {
            pattern->tracks[(size_t)trackIndex].isMuted = previousMuted;
        }
    }
}
