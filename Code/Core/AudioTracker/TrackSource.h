#ifndef TRACKSOURCE_H
#define TRACKSOURCE_H

#include <string>

#include "Synthesis/SynthPresets.h"

namespace CC
{
    // Discriminated union over the two kinds of audio source a pattern
    // track can reference. The audio thread does not branch on kind —
    // both kinds produce PCM at edit/render time and feed the existing
    // voice pool. The kind matters only at edit time (which dropdown
    // panel shows in the UI) and at project-save time (which YAML
    // shape is emitted).
    enum class TrackSourceKind
    {
        None,
        Sample,
        Synth
    };

    struct TrackSource
    {
        TrackSourceKind kind = TrackSourceKind::None;

        // Populated when kind == Sample. Refers to a SampleLibrary
        // entry id; resolved against Tracker.yaml on load.
        std::string sampleId;

        // Populated when kind == Synth. Carries the preset id and
        // per-preset parameter struct. Pre-rendered to PCM at edit
        // time.
        SynthParams synthParams;
    };
}

#endif // TRACKSOURCE_H
