#ifndef KICKGENERATOR_H
#define KICKGENERATOR_H

#include <vector>

#include "SynthPresets.h"

namespace CC
{
    // Pitched-sine kick: fast pitch sweep (startFrequency → endFrequency)
    // over pitchGlideSeconds, plus an exponential amplitude envelope of
    // amplitudeDecaySeconds. Output is interleaved float at the requested
    // rate and channel count.
    void RenderKick(const KickParams& params, int sampleRate, int channelCount, std::vector<float>& outputPcm);
}

#endif // KICKGENERATOR_H
