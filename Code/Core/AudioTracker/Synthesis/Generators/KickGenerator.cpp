#include "KickGenerator.h"

#include <cmath>
#include <cstddef>

namespace CC
{
    void RenderKick(const KickParams& params, int sampleRate, int channelCount, std::vector<float>& outputPcm)
    {
        const float twoPi = 6.28318530718f;

        int frameCount = (int)((float)sampleRate * params.durationSeconds);
        outputPcm.assign((size_t)frameCount * (size_t)channelCount, 0.0f);

        float phase = 0.0f;
        for (int i = 0; i < frameCount; i++)
        {
            float secondsElapsed = (float)i / (float)sampleRate;
            float frequency      = params.endFrequency + (params.startFrequency - params.endFrequency) * expf(-secondsElapsed / params.pitchGlideSeconds);
            float envelope       = expf(-secondsElapsed / params.amplitudeDecaySeconds);
            float sample         = sinf(phase) * envelope * params.amplitude;
            phase += twoPi * frequency / (float)sampleRate;

            for (int channelIndex = 0; channelIndex < channelCount; channelIndex++)
            {
                outputPcm[(size_t)i * (size_t)channelCount + (size_t)channelIndex] = sample;
            }
        }
    }
}
