#ifndef SYNTHESIS_H
#define SYNTHESIS_H

#include <vector>

#include "SynthPresets.h"

namespace CC
{
    // Tracker-scoped synthesis subsystem. Renders a parameterised
    // preset into an interleaved float PCM buffer matching the engine's
    // sample rate and channel count. Pure function over inputs — no
    // playback state, no audio-thread coupling. Consumers (TrackerEngine,
    // audition path, project loader) own the resulting buffer.
    //
    // Pre-rendering by design: the audio thread sees only PCM, identical
    // to disk samples. Live per-frame synth generation is out of scope.
    class Synthesis
    {
    public:
        Synthesis();
        virtual ~Synthesis();

        static Synthesis* Get();

        void RenderPreset(const SynthParams& params, int sampleRate, int channelCount, std::vector<float>& outputPcm) const;

    private:
        static Synthesis* instance;
    };
}

#endif // SYNTHESIS_H
