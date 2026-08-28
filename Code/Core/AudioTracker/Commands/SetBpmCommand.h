#ifndef SETBPMCOMMAND_H
#define SETBPMCOMMAND_H

#include "TrackerCommand.h"

namespace CC
{
    // Sets the project's BPM, captures previous for Undo. Clamps the
    // requested value to a sensible musical range so transport math
    // can't blow up on a stray click.
    class SetBpmCommand : public TrackerCommand
    {
    public:
        explicit SetBpmCommand(int newBpm);

        void Execute(TrackerProject& project) override;
        void Undo(TrackerProject& project) override;
        const char* DisplayLabel() const override { return "Set BPM"; }

    private:
        int newBpm;
        int previousBpm;
    };
}

#endif // SETBPMCOMMAND_H
