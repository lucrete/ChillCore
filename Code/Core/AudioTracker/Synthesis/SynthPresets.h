#ifndef SYNTHPRESETS_H
#define SYNTHPRESETS_H

namespace CC
{
    // ========================
    // Preset identifiers
    // ========================

    // Each preset corresponds to a generator translation unit under
    // Generators/. Adding a preset is: add an entry here, write a
    // matching parameter struct, add the generator, and dispatch from
    // Synthesis::RenderPreset.
    enum class SynthPresetId
    {
        Kick,
        Snare,
        HiHat,
        Count
    };

    // ========================
    // Per-preset parameter structs
    // ========================

    // Default values reproduce the Phase 1 prototype synth output so
    // that the existing AudioTracker prototype loop sounds identical
    // after the synthesis extraction.
    struct KickParams
    {
        float durationSeconds       = 0.30f;
        float startFrequency        = 80.0f;
        float endFrequency          = 35.0f;
        float pitchGlideSeconds     = 0.025f;
        float amplitudeDecaySeconds = 0.080f;
        float amplitude             = 0.95f;
    };

    struct SnareParams
    {
        float durationSeconds       = 0.18f;
        float amplitudeDecaySeconds = 0.040f;
        float tonalFrequency        = 200.0f;
        float tonalGain             = 0.30f;
        float noiseGain             = 0.80f;
        float amplitude             = 0.65f;
    };

    struct HiHatParams
    {
        float durationSeconds       = 0.06f;
        float amplitudeDecaySeconds = 0.012f;
        float highPassFeedback      = 0.85f;
        float amplitude             = 0.45f;
    };

    // ========================
    // Tagged parameter container
    // ========================

    // A single value type carries any preset's parameters so callers
    // can pass one argument to RenderPreset. Only the field matching
    // presetId is read; the others are ignored. A union would save a
    // few bytes but the structs are small and a plain aggregate keeps
    // the YAML round-trip and constructor story trivial.
    struct SynthParams
    {
        SynthPresetId presetId    = SynthPresetId::Kick;
        KickParams    kickParams;
        SnareParams   snareParams;
        HiHatParams   hiHatParams;
    };
}

#endif // SYNTHPRESETS_H
