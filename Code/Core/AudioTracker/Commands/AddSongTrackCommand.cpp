#include "AddSongTrackCommand.h"

#include <cstdio>

#include "TrackerProject.h"

namespace CC
{
    AddSongTrackCommand::AddSongTrackCommand()
        : insertedTrackIndex(-1)
    {
    }

    void AddSongTrackCommand::Execute(TrackerProject& project)
    {
        SongTrack track;
        track.patternIdPerBar.assign((size_t)project.songLengthBars, std::string());

        char nameBuffer[32];
        std::snprintf(nameBuffer, sizeof(nameBuffer), "Track %zu", project.songTracks.size() + 1);
        track.displayName = nameBuffer;

        insertedTrackIndex = (int)project.songTracks.size();
        project.songTracks.push_back(track);
    }

    void AddSongTrackCommand::Undo(TrackerProject& project)
    {
        if (insertedTrackIndex >= 0 && (size_t)insertedTrackIndex < project.songTracks.size())
        {
            project.songTracks.erase(project.songTracks.begin() + insertedTrackIndex);
        }
    }
}
