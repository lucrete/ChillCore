#include "PlacePatternInSongCommand.h"

#include "TrackerProject.h"
#include "PrintManager.h"

namespace CC
{
    PlacePatternInSongCommand::PlacePatternInSongCommand(int _songTrackIndex, int _barIndex, const std::string& _patternId)
        : songTrackIndex(_songTrackIndex)
        , barIndex(_barIndex)
        , patternId(_patternId)
        , didExecute(false)
    {
    }

    void PlacePatternInSongCommand::Execute(TrackerProject& project)
    {
        didExecute = false;

        if (songTrackIndex < 0
            || (size_t)songTrackIndex >= project.songTracks.size())
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS,
                "PlacePatternInSongCommand: song track %d out of range", songTrackIndex);
        }
        else
        {
            SongTrack& track = project.songTracks[(size_t)songTrackIndex];
            if (barIndex < 0 || (size_t)barIndex >= track.patternIdPerBar.size())
            {
                CCPrint(PrintManager::CHANNEL_ALWAYS,
                    "PlacePatternInSongCommand: bar %d out of range", barIndex);
            }
            else
            {
                previousPatternId = track.patternIdPerBar[(size_t)barIndex];
                track.patternIdPerBar[(size_t)barIndex] = patternId;
                didExecute = true;
            }
        }
    }

    void PlacePatternInSongCommand::Undo(TrackerProject& project)
    {
        if (didExecute
            && songTrackIndex >= 0
            && (size_t)songTrackIndex < project.songTracks.size())
        {
            SongTrack& track = project.songTracks[(size_t)songTrackIndex];
            if (barIndex >= 0 && (size_t)barIndex < track.patternIdPerBar.size())
            {
                track.patternIdPerBar[(size_t)barIndex] = previousPatternId;
            }
        }
    }
}
