#ifndef PATTERN_H
#define PATTERN_H

#include <string>
#include <vector>

#include "TrackSource.h"

namespace CC
{
    // One row in a pattern: a source plus a velocity per step.
    // Velocities are stored as floats from day one (see plan §Velocity)
    // — 0.0 is off, any positive value is on at that gain. There is no
    // separate is_on flag.
    struct PatternTrack
    {
        TrackSource        source;
        std::vector<float> velocities;             // size = pattern.barCount * pattern.stepsPerBar
        float              gainLinear  = 1.0f;
        bool               isMuted     = false;
    };

    // A Pattern is an editable beat grid. Steps are the columns, tracks
    // are the rows. Time progresses left to right. stepsPerBar is fixed
    // at 16 in v1 (sixteenth-note grid); variable subdivision is a
    // Phase 6 polish item.
    struct Pattern
    {
        std::string               id;              // unique within the project
        std::string               displayName;
        int                       barCount     = 1;
        int                       stepsPerBar  = 16;
        std::vector<PatternTrack> tracks;

        int GetTotalStepCount() const { return barCount * stepsPerBar; }
    };
}

#endif // PATTERN_H
