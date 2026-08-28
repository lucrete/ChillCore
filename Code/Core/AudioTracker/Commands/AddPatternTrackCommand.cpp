#include "AddPatternTrackCommand.h"

#include "TrackerProject.h"
#include "Pattern.h"
#include "PrintManager.h"

namespace CC
{
    AddPatternTrackCommand::AddPatternTrackCommand(const std::string& _patternId)
        : patternId(_patternId)
        , insertedTrackIndex(-1)
    {
    }

    void AddPatternTrackCommand::Execute(TrackerProject& project)
    {
        Pattern* pattern = project.FindPattern(patternId);
        if (pattern == nullptr)
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS, "AddPatternTrackCommand: pattern '%s' not found", patternId.c_str());
            insertedTrackIndex = -1;
        }
        else
        {
            PatternTrack track;
            track.velocities.assign((size_t)pattern->GetTotalStepCount(), 0.0f);

            insertedTrackIndex = (int)pattern->tracks.size();
            pattern->tracks.push_back(track);
        }
    }

    void AddPatternTrackCommand::Undo(TrackerProject& project)
    {
        if (insertedTrackIndex >= 0)
        {
            Pattern* pattern = project.FindPattern(patternId);
            if (pattern != nullptr && (size_t)insertedTrackIndex < pattern->tracks.size())
            {
                pattern->tracks.erase(pattern->tracks.begin() + insertedTrackIndex);
            }
        }
    }
}
