#include "Synthesis.h"

#include "Generators/KickGenerator.h"
#include "Generators/SnareGenerator.h"
#include "Generators/HiHatGenerator.h"
#include "CCAssert.h"

namespace CC
{
    Synthesis* Synthesis::instance = nullptr;

    Synthesis::Synthesis()
    {
        CC_ASSERT(instance == nullptr, "Synthesis already created");
        instance = this;
    }

    Synthesis::~Synthesis()
    {
        instance = nullptr;
    }

    Synthesis* Synthesis::Get()
    {
        CC_ASSERT(instance != nullptr, "Synthesis not created yet");
        return instance;
    }

    void Synthesis::RenderPreset(const SynthParams& params, int sampleRate, int channelCount, std::vector<float>& outputPcm) const
    {
        switch (params.presetId)
        {
            case SynthPresetId::Kick:
                RenderKick(params.kickParams, sampleRate, channelCount, outputPcm);
                break;
            case SynthPresetId::Snare:
                RenderSnare(params.snareParams, sampleRate, channelCount, outputPcm);
                break;
            case SynthPresetId::HiHat:
                RenderHiHat(params.hiHatParams, sampleRate, channelCount, outputPcm);
                break;
            case SynthPresetId::Count:
                CC_ASSERT(false, "Synthesis::RenderPreset called with sentinel preset id");
                break;
        }
    }
}
