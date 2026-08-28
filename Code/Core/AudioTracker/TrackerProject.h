#ifndef TRACKERPROJECT_H
#define TRACKERPROJECT_H

#include <cstddef>
#include <string>
#include <vector>

#include "Pattern.h"

namespace CC
{
    // One row in the Song view. Each entry of patternIdPerBar names a
    // pattern (by id) that fires at that bar; an empty string is an
    // empty bar (silence). Multi-bar patterns are represented by
    // repeating the pattern id across consecutive bars; the renderer
    // groups consecutive equal ids into a single visually-wide cell.
    struct SongTrack
    {
        std::string              displayName;
        float                    gainLinear = 1.0f;
        bool                     isMuted    = false;
        std::vector<std::string> patternIdPerBar;
    };

    // In-memory model for the AudioTracker's working state. Owned by
    // AppStateAudioTracker as a value type, not a singleton — Save / Open
    // (Phase 5) swap the contents of this object rather than the
    // pointer, so the rest of the tracker can hold stable references.
    //
    // Phase 3 scope: patterns only. Song-level state (tracks across
    // bars, BPM, time signature, length) lands in Phase 4.
    struct TrackerProject
    {
        // Transport-level state. BPM and time signature affect both
        // pattern audition timing and song playback timing.
        int                    bpm                      = 90;
        int                    timeSignatureNumerator   = 4;
        int                    timeSignatureDenominator = 4;
        int                    songLengthBars           = 8;

        std::vector<Pattern>   patterns;
        std::vector<SongTrack> songTracks;
        int                    currentPatternIndex = 0;

        // Bumped by TrackerCommandHistory on every Apply / Undo / Redo.
        // UI controllers poll this to know when to re-render content
        // that depends on project state. Mutation routes through
        // commands by design, so a single version bump per command
        // keeps the polling cost trivial.
        int version = 0;

        // Convenience: index lookup by pattern id. Returns -1 when not
        // found.
        int FindPatternIndex(const std::string& id) const
        {
            int result = -1;
            for (size_t i = 0; i < patterns.size(); i++)
            {
                if (patterns[i].id == id)
                {
                    result = (int)i;
                    break;
                }
            }
            return result;
        }

        Pattern* FindPattern(const std::string& id)
        {
            int index = FindPatternIndex(id);
            return (index >= 0) ? &patterns[(size_t)index] : nullptr;
        }
    };
}

#endif // TRACKERPROJECT_H
