#include "RemovePatternTrackCommand.h"

#include "TrackerProject.h"
#include "PrintManager.h"

namespace CC
{
    RemovePatternTrackCommand::RemovePatternTrackCommand(const std::string& _patternId, int _trackIndex)
        : patternId(_patternId)
        , trackIndex(_trackIndex)
        , didExecute(false)
    {
    }

    void RemovePatternTrackCommand::Execute(TrackerProject& project)
    {
        Pattern* pattern = project.FindPattern(patternId);
        if (pattern == nullptr
            || trackIndex < 0
            || (size_t)trackIndex >= pattern->tracks.size())
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS,
                "RemovePatternTrackCommand: pattern '%s' track %d not found", patternId.c_str(), trackIndex);
            didExecute = false;
        }
        else
        {
            removedTrack = pattern->tracks[(size_t)trackIndex];
            pattern->tracks.erase(pattern->tracks.begin() + trackIndex);
            didExecute = true;
        }
    }

    void RemovePatternTrackCommand::Undo(TrackerProject& project)
    {
        if (didExecute)
        {
            Pattern* pattern = project.FindPattern(patternId);
            if (pattern != nullptr
                && trackIndex >= 0
                && (size_t)trackIndex <= pattern->tracks.size())
            {
                pattern->tracks.insert(pattern->tracks.begin() + trackIndex, removedTrack);
            }
        }
    }
}
