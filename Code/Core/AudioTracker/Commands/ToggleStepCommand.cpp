#include "ToggleStepCommand.h"

#include "TrackerProject.h"
#include "Pattern.h"
#include "PrintManager.h"

namespace CC
{
    static const float DEFAULT_ON_VELOCITY = 1.0f;

    ToggleStepCommand::ToggleStepCommand(const std::string& _patternId, int _trackIndex, int _stepIndex)
        : patternId(_patternId)
        , trackIndex(_trackIndex)
        , stepIndex(_stepIndex)
        , previousVelocity(0.0f)
    {
    }

    void ToggleStepCommand::Execute(TrackerProject& project)
    {
        Pattern* pattern = project.FindPattern(patternId);
        if (pattern == nullptr
            || trackIndex < 0
            || (size_t)trackIndex >= pattern->tracks.size())
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS, "ToggleStepCommand: pattern '%s' track %d not found", patternId.c_str(), trackIndex);
        }
        else
        {
            PatternTrack& track = pattern->tracks[(size_t)trackIndex];
            if (stepIndex < 0 || (size_t)stepIndex >= track.velocities.size())
            {
                CCPrint(PrintManager::CHANNEL_ALWAYS, "ToggleStepCommand: step %d out of range", stepIndex);
            }
            else
            {
                previousVelocity = track.velocities[(size_t)stepIndex];
                track.velocities[(size_t)stepIndex] = (previousVelocity > 0.0f) ? 0.0f : DEFAULT_ON_VELOCITY;
            }
        }
    }

    void ToggleStepCommand::Undo(TrackerProject& project)
    {
        Pattern* pattern = project.FindPattern(patternId);
        if (pattern != nullptr
            && trackIndex >= 0
            && (size_t)trackIndex < pattern->tracks.size())
        {
            PatternTrack& track = pattern->tracks[(size_t)trackIndex];
            if (stepIndex >= 0 && (size_t)stepIndex < track.velocities.size())
            {
                track.velocities[(size_t)stepIndex] = previousVelocity;
            }
        }
    }
}
