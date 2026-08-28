#include "AddPatternCommand.h"

#include <cstdio>

#include "TrackerProject.h"
#include "Pattern.h"

namespace CC
{
    AddPatternCommand::AddPatternCommand()
    {
    }

    void AddPatternCommand::Execute(TrackerProject& project)
    {
        // Pick the lowest unused P<n> id. Linear scan over patterns to
        // find the maximum existing P-prefix integer + 1; bounded by
        // the pattern count so cheap in practice.
        int nextIndex = 1;
        for (size_t i = 0; i < project.patterns.size(); i++)
        {
            const std::string& id = project.patterns[i].id;
            if (id.size() >= 2 && id[0] == 'P')
            {
                int parsedIndex = 0;
                bool isNumeric = true;
                for (size_t c = 1; c < id.size() && isNumeric; c++)
                {
                    char ch = id[c];
                    if (ch < '0' || ch > '9')
                    {
                        isNumeric = false;
                    }
                    else
                    {
                        parsedIndex = parsedIndex * 10 + (ch - '0');
                    }
                }
                if (isNumeric && parsedIndex >= nextIndex)
                {
                    nextIndex = parsedIndex + 1;
                }
            }
        }

        char idBuffer[16];
        char nameBuffer[32];
        std::snprintf(idBuffer,   sizeof(idBuffer),   "P%d",       nextIndex);
        std::snprintf(nameBuffer, sizeof(nameBuffer), "Pattern %d", nextIndex);

        Pattern newPattern;
        newPattern.id          = idBuffer;
        newPattern.displayName = nameBuffer;
        newPattern.barCount    = 1;
        newPattern.stepsPerBar = 16;

        insertedId = newPattern.id;
        project.patterns.push_back(newPattern);
    }

    void AddPatternCommand::Undo(TrackerProject& project)
    {
        if (!insertedId.empty())
        {
            int index = project.FindPatternIndex(insertedId);
            if (index >= 0)
            {
                project.patterns.erase(project.patterns.begin() + index);
                if (project.currentPatternIndex >= (int)project.patterns.size())
                {
                    project.currentPatternIndex = (int)project.patterns.size() - 1;
                }
                if (project.currentPatternIndex < 0 && !project.patterns.empty())
                {
                    project.currentPatternIndex = 0;
                }
            }
        }
    }
}
