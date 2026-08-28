#include "SetBpmCommand.h"

#include "TrackerProject.h"

namespace CC
{
    static const int MIN_BPM = 20;
    static const int MAX_BPM = 300;

    SetBpmCommand::SetBpmCommand(int _newBpm)
        : newBpm(_newBpm)
        , previousBpm(0)
    {
        if (newBpm < MIN_BPM) { newBpm = MIN_BPM; }
        if (newBpm > MAX_BPM) { newBpm = MAX_BPM; }
    }

    void SetBpmCommand::Execute(TrackerProject& project)
    {
        previousBpm = project.bpm;
        project.bpm = newBpm;
    }

    void SetBpmCommand::Undo(TrackerProject& project)
    {
        project.bpm = previousBpm;
    }
}
