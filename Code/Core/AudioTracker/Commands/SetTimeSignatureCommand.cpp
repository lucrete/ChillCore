#include "SetTimeSignatureCommand.h"

#include "TrackerProject.h"

namespace CC
{
    SetTimeSignatureCommand::SetTimeSignatureCommand(int _newNumerator, int _newDenominator)
        : newNumerator(_newNumerator)
        , newDenominator(_newDenominator)
        , previousNumerator(4)
        , previousDenominator(4)
    {
        if (newNumerator   < 1) { newNumerator   = 1; }
        if (newNumerator   > 16) { newNumerator  = 16; }
        if (newDenominator < 1) { newDenominator = 1; }
        if (newDenominator > 32) { newDenominator = 32; }
    }

    void SetTimeSignatureCommand::Execute(TrackerProject& project)
    {
        previousNumerator           = project.timeSignatureNumerator;
        previousDenominator         = project.timeSignatureDenominator;
        project.timeSignatureNumerator   = newNumerator;
        project.timeSignatureDenominator = newDenominator;
    }

    void SetTimeSignatureCommand::Undo(TrackerProject& project)
    {
        project.timeSignatureNumerator   = previousNumerator;
        project.timeSignatureDenominator = previousDenominator;
    }
}
