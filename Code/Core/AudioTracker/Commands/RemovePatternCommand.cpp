#include "RemovePatternCommand.h"

#include "TrackerProject.h"
#include "PrintManager.h"

namespace CC
{
    RemovePatternCommand::RemovePatternCommand(const std::string& _patternId)
        : patternId(_patternId)
        , removedIndex(-1)
        , previousCurrentIndex(0)
        , didExecute(false)
    {
    }

    void RemovePatternCommand::Execute(TrackerProject& project)
    {
        didExecute = false;

        if (project.patterns.size() <= 1)
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS,
                "RemovePatternCommand: refusing to remove the only remaining pattern");
        }
        else
        {
            int index = project.FindPatternIndex(patternId);
            if (index < 0)
            {
                CCPrint(PrintManager::CHANNEL_ALWAYS,
                    "RemovePatternCommand: pattern '%s' not found", patternId.c_str());
            }
            else
            {
                removedIndex         = index;
                removedPattern       = project.patterns[(size_t)index];
                previousCurrentIndex = project.currentPatternIndex;

                project.patterns.erase(project.patterns.begin() + index);

                if (project.currentPatternIndex >= (int)project.patterns.size())
                {
                    project.currentPatternIndex = (int)project.patterns.size() - 1;
                }
                if (project.currentPatternIndex < 0)
                {
                    project.currentPatternIndex = 0;
                }

                didExecute = true;
            }
        }
    }

    void RemovePatternCommand::Undo(TrackerProject& project)
    {
        if (didExecute)
        {
            int insertIndex = removedIndex;
            if (insertIndex < 0)
            {
                insertIndex = 0;
            }
            if ((size_t)insertIndex > project.patterns.size())
            {
                insertIndex = (int)project.patterns.size();
            }
            project.patterns.insert(project.patterns.begin() + insertIndex, removedPattern);
            project.currentPatternIndex = previousCurrentIndex;
        }
    }
}
