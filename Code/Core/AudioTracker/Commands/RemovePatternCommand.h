#ifndef REMOVEPATTERNCOMMAND_H
#define REMOVEPATTERNCOMMAND_H

#include <string>

#include "TrackerCommand.h"
#include "Pattern.h"

namespace CC
{
    // Removes a pattern by id. Refuses if the project has only one
    // pattern (a project must always carry at least one). Snapshots the
    // removed pattern + its prior list index + the previous
    // currentPatternIndex so Undo restores all three.
    class RemovePatternCommand : public TrackerCommand
    {
    public:
        explicit RemovePatternCommand(const std::string& patternId);

        void Execute(TrackerProject& project) override;
        void Undo(TrackerProject& project) override;
        const char* DisplayLabel() const override { return "Remove pattern"; }

    private:
        std::string patternId;
        Pattern     removedPattern;
        int         removedIndex;
        int         previousCurrentIndex;
        bool        didExecute;
    };
}

#endif // REMOVEPATTERNCOMMAND_H
