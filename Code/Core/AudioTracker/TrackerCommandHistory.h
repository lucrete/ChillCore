#ifndef TRACKERCOMMANDHISTORY_H
#define TRACKERCOMMANDHISTORY_H

#include <memory>
#include <vector>

namespace CC
{
    class TrackerCommand;
    struct TrackerProject;

    // Tracker-scoped undo/redo stack. Singleton so the AppState and any
    // editor controller can apply a command without threading the history
    // through their own state. The TrackerProject is passed in to each call
    // because the stack is shared across the AppState lifetime while the
    // project itself can be swapped (New, Open).
    class TrackerCommandHistory
    {
    public:
        TrackerCommandHistory();
        ~TrackerCommandHistory();

        static TrackerCommandHistory* Get();

        void Apply(std::unique_ptr<TrackerCommand> command, TrackerProject& project);

        bool CanUndo() const;
        bool CanRedo() const;

        void Undo(TrackerProject& project);
        void Redo(TrackerProject& project);

        // Clear the entire history. Called on New / Open so undo never
        // crosses a project boundary.
        void Clear();

    private:
        static TrackerCommandHistory* instance;
        static const int MAX_UNDO_DEPTH = 200;

        std::vector<std::unique_ptr<TrackerCommand>> undoStack;
        std::vector<std::unique_ptr<TrackerCommand>> redoStack;
    };
}

#endif // TRACKERCOMMANDHISTORY_H
