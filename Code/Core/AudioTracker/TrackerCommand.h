#ifndef TRACKERCOMMAND_H
#define TRACKERCOMMAND_H

namespace CC
{
    struct TrackerProject;

    // Tracker-scoped undo/redo interface. Every mutation to TrackerProject
    // flows through a TrackerCommand so that TrackerCommandHistory can
    // replay it in either direction. Phase 3 introduces the first concrete
    // commands; Phase 0 ships only the interface.
    class TrackerCommand
    {
    public:
        virtual ~TrackerCommand() = default;

        virtual void Execute(TrackerProject& project) = 0;
        virtual void Undo(TrackerProject& project) = 0;
        virtual const char* DisplayLabel() const = 0;

        // Drag-paint and slider-drag interactions emit a stream of small
        // commands; coalescing collapses adjacent same-shape edits into one
        // undo entry. Default: never coalesce.
        virtual bool TryCoalesceWith(const TrackerCommand&) { return false; }
    };
}

#endif // TRACKERCOMMAND_H
