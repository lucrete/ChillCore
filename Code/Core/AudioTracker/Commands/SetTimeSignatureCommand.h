#ifndef SETTIMESIGNATURECOMMAND_H
#define SETTIMESIGNATURECOMMAND_H

#include "TrackerCommand.h"

namespace CC
{
    class SetTimeSignatureCommand : public TrackerCommand
    {
    public:
        SetTimeSignatureCommand(int newNumerator, int newDenominator);

        void Execute(TrackerProject& project) override;
        void Undo(TrackerProject& project) override;
        const char* DisplayLabel() const override { return "Set time signature"; }

    private:
        int newNumerator;
        int newDenominator;
        int previousNumerator;
        int previousDenominator;
    };
}

#endif // SETTIMESIGNATURECOMMAND_H
