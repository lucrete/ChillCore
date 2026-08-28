#include "TrackerCommandHistory.h"

#include "TrackerCommand.h"
#include "TrackerProject.h"
#include "CCAssert.h"

namespace CC
{
    TrackerCommandHistory* TrackerCommandHistory::instance = nullptr;

    TrackerCommandHistory::TrackerCommandHistory()
    {
        CC_ASSERT(instance == nullptr, "TrackerCommandHistory already created");
        instance = this;
    }

    TrackerCommandHistory::~TrackerCommandHistory()
    {
        undoStack.clear();
        redoStack.clear();
        instance = nullptr;
    }

    TrackerCommandHistory* TrackerCommandHistory::Get()
    {
        CC_ASSERT(instance != nullptr, "TrackerCommandHistory not created yet");
        return instance;
    }

    void TrackerCommandHistory::Apply(std::unique_ptr<TrackerCommand> command, TrackerProject& project)
    {
        CC_ASSERT(command != nullptr, "TrackerCommandHistory::Apply received null command");

        command->Execute(project);
        project.version++;
        redoStack.clear();

        bool didCoalesce = false;
        if (!undoStack.empty())
        {
            TrackerCommand& previous = *undoStack.back();
            didCoalesce = command->TryCoalesceWith(previous);
        }

        if (!didCoalesce)
        {
            undoStack.push_back(std::move(command));
            if ((int)undoStack.size() > MAX_UNDO_DEPTH)
            {
                undoStack.erase(undoStack.begin());
            }
        }
    }

    bool TrackerCommandHistory::CanUndo() const
    {
        return !undoStack.empty();
    }

    bool TrackerCommandHistory::CanRedo() const
    {
        return !redoStack.empty();
    }

    void TrackerCommandHistory::Undo(TrackerProject& project)
    {
        if (!undoStack.empty())
        {
            std::unique_ptr<TrackerCommand> command = std::move(undoStack.back());
            undoStack.pop_back();
            command->Undo(project);
            project.version++;
            redoStack.push_back(std::move(command));
        }
    }

    void TrackerCommandHistory::Redo(TrackerProject& project)
    {
        if (!redoStack.empty())
        {
            std::unique_ptr<TrackerCommand> command = std::move(redoStack.back());
            redoStack.pop_back();
            command->Execute(project);
            project.version++;
            undoStack.push_back(std::move(command));
        }
    }

    void TrackerCommandHistory::Clear()
    {
        undoStack.clear();
        redoStack.clear();
    }
}
