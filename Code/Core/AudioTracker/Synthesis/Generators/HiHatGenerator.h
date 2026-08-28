#ifndef HIHATGENERATOR_H
#define HIHATGENERATOR_H

#include <vector>

#include "SynthPresets.h"

namespace CC
{
    // Closed hi-hat: white noise through a single-pole high-pass plus a
    // very fast amplitude decay. Output is interleaved float at the
    // requested rate and channel count.
    void RenderHiHat(const HiHatParams& params, int sampleRate, int channelCount, std::vector<float>& outputPcm);
}

#endif // HIHATGENERATOR_H
