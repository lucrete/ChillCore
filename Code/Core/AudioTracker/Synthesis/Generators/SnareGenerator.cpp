#include "SnareGenerator.h"

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

    void RenderSnare(const SnareParams& params, int sampleRate, int channelCount, std::vector<float>& outputPcm)
    {
        const float twoPi = 6.28318530718f;

        int frameCount = (int)((float)sampleRate * params.durationSeconds);
        outputPcm.assign((size_t)frameCount * (size_t)channelCount, 0.0f);

        uint32_t randomState = 0x12345678u;
        float    phase       = 0.0f;

        for (int i = 0; i < frameCount; i++)
        {
            float noise          = NextNoise(randomState);
            float secondsElapsed = (float)i / (float)sampleRate;
            float envelope       = expf(-secondsElapsed / params.amplitudeDecaySeconds);
            float tonal          = sinf(phase) * params.tonalGain;
            phase += twoPi * params.tonalFrequency / (float)sampleRate;

            float sample = (tonal + noise * params.noiseGain) * envelope * params.amplitude;

            for (int channelIndex = 0; channelIndex < channelCount; channelIndex++)
            {
                outputPcm[(size_t)i * (size_t)channelCount + (size_t)channelIndex] = sample;
            }
        }
    }
}
