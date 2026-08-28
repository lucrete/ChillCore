#include "ToggleSongTrackMutedCommand.h"

#include "TrackerProject.h"
#include "PrintManager.h"

namespace CC
{
    ToggleSongTrackMutedCommand::ToggleSongTrackMutedCommand(int _songTrackIndex)
        : songTrackIndex(_songTrackIndex)
        , previousMuted(false)
    {
    }

    void ToggleSongTrackMutedCommand::Execute(TrackerProject& project)
    {
        if (songTrackIndex < 0 || (size_t)songTrackIndex >= project.songTracks.size())
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS,
                "ToggleSongTrackMutedCommand: song track %d not found", songTrackIndex);
        }
        else
        {
            SongTrack& track = project.songTracks[(size_t)songTrackIndex];
            previousMuted = track.isMuted;
            track.isMuted = !previousMuted;
        }
    }

    void ToggleSongTrackMutedCommand::Undo(TrackerProject& project)
    {
        if (songTrackIndex >= 0 && (size_t)songTrackIndex < project.songTracks.size())
        {
            project.songTracks[(size_t)songTrackIndex].isMuted = previousMuted;
        }
    }
}
