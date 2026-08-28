#include "SetSongLengthCommand.h"

#include "TrackerProject.h"
#include "PrintManager.h"

namespace CC
{
    static const int MIN_SONG_LENGTH = 1;
    static const int MAX_SONG_LENGTH = 256;

    SetSongLengthCommand::SetSongLengthCommand(int _newLengthBars)
        : newLengthBars(_newLengthBars)
        , previousLengthBars(0)
    {
        if (newLengthBars < MIN_SONG_LENGTH) { newLengthBars = MIN_SONG_LENGTH; }
        if (newLengthBars > MAX_SONG_LENGTH) { newLengthBars = MAX_SONG_LENGTH; }
    }

    void SetSongLengthCommand::Execute(TrackerProject& project)
    {
        previousLengthBars      = project.songLengthBars;
        project.songLengthBars  = newLengthBars;

        droppedTails.clear();
        droppedTails.resize(project.songTracks.size());

        for (size_t i = 0; i < project.songTracks.size(); i++)
        {
            std::vector<std::string>& bars = project.songTracks[i].patternIdPerBar;

            if ((int)bars.size() < newLengthBars)
            {
                // Grow with empty bars.
                bars.insert(bars.end(), (size_t)newLengthBars - bars.size(), std::string());
            }
            else if ((int)bars.size() > newLengthBars)
            {
                // Shrink and snapshot the dropped tail for Undo.
                droppedTails[i].assign(bars.begin() + newLengthBars, bars.end());
                bars.erase(bars.begin() + newLengthBars, bars.end());
            }
        }
    }

    void SetSongLengthCommand::Undo(TrackerProject& project)
    {
        project.songLengthBars = previousLengthBars;

        for (size_t i = 0; i < project.songTracks.size(); i++)
        {
            std::vector<std::string>& bars = project.songTracks[i].patternIdPerBar;

            if ((int)bars.size() < previousLengthBars)
            {
                if (i < droppedTails.size() && !droppedTails[i].empty())
                {
                    bars.insert(bars.end(), droppedTails[i].begin(), droppedTails[i].end());
                }
                // If we don't have a tail snapshot (project was grown
                // by Execute), just pad with empties.
                while ((int)bars.size() < previousLengthBars)
                {
                    bars.push_back(std::string());
                }
            }
            else if ((int)bars.size() > previousLengthBars)
            {
                bars.erase(bars.begin() + previousLengthBars, bars.end());
            }
        }
    }
}
