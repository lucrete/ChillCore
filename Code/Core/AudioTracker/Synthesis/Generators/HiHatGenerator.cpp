#include "HiHatGenerator.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace CC
{
    static float NextNoise(uint32_t& state)
    {
        state = state * 1664525u + 1013904223u;
        float result = ((float)(state >> 8) / (float)0x00FFFFFF) * 2.0f - 1.0f;
        return result;
    }

    void RenderHiHat(const HiHatParams& params, int sampleRate, int channelCount, std::vector<float>& outputPcm)
    {
        int frameCount = (int)((float)sampleRate * params.durationSeconds);
        outputPcm.assign((size_t)frameCount * (size_t)channelCount, 0.0f);

        uint32_t randomState   = 0x9E3779B9u;
        float    previousNoise = 0.0f;

        for (int i = 0; i < frameCount; i++)
        {
            float noise          = NextNoise(randomState);
            float highPassed     = noise - previousNoise;
            previousNoise        = noise * params.highPassFeedback;

            float secondsElapsed = (float)i / (float)sampleRate;
            float envelope       = expf(-secondsElapsed / params.amplitudeDecaySeconds);
            float sample         = highPassed * envelope * params.amplitude;

            for (int channelIndex = 0; channelIndex < channelCount; channelIndex++)
            {
                outputPcm[(size_t)i * (size_t)channelCount + (size_t)channelIndex] = sample;
            }
        }
    }
}
