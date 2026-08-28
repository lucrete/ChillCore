#ifndef SNAREGENERATOR_H
#define SNAREGENERATOR_H

#include <vector>

#include "SynthPresets.h"

namespace CC
{
    // Snare: white noise body plus a tonalFrequency tonal element with
    // an exponential amplitude decay. Output is interleaved float at the
    // requested rate and channel count.
    void RenderSnare(const SnareParams& params, int sampleRate, int channelCount, std::vector<float>& outputPcm);
}

#endif // SNAREGENERATOR_H
