#ifndef POSTPROCESS_H
#define POSTPROCESS_H

#include "GfxHandles.h"
#include "PostProcessEffect.h"

namespace CC
{
    class Material;
    class RenderableFullscreenQuad;

    // ========================
    // PostProcess
    // ========================
    //
    // The engine's post-process stack, owned by RenderManager and run at the
    // point in the frame where the scene target is resolved to the backbuffer.
    //
    // Everything except bloom is a uniform-gated block inside one fused pass.
    // A full-screen pass is bandwidth bound rather than arithmetic bound, so
    // chaining an effect per pass would cost a full read and write of the
    // frame each time, which matters on a tiled mobile GPU far more than the
    // few extra instructions the fused shader executes with its blocks
    // switched off. Bloom is the exception: it needs its own downsampled
    // ping-pong targets, so it runs its passes first and hands the result to
    // the fused pass as a second texture.
    //
    // Order within the fused pass is bloom composite, exposure, vignette, log
    // encode, grade, tone map, output. Grading before the tone curve and in
    // log space is what makes a preset portable: the same numbers land the
    // same way regardless of the exposure they are viewed at.

    enum class PostProcessEffectId
    {
        Bloom,
        Vignette,
        ColorGrade,
        Tonemap,
        Max
    };

    class PostProcess
    {
    public:
        PostProcess();
        ~PostProcess();

        // Separate from the constructor because it builds materials, which
        // needs MaterialManager and ShaderManager already standing.
        void Init();
        void Shutdown();

        // ========================
        // Effects
        // ========================

        PostProcessEffect& GetEffect(PostProcessEffectId id);
        const PostProcessEffect& GetEffect(PostProcessEffectId id) const;
        int GetEffectCount() const;
        PostProcessEffect& GetEffectByIndex(int index);

        // Case-insensitive; scene files and console commands address effects
        // by name rather than by enum.
        PostProcessEffect* FindEffect(const char* effectName);

        bool IsAnyEffectEnabled() const;
        void DisableAllEffects();

        // Every effect back to the enable state and parameter values the stack
        // configures itself with. This is what a scene loads onto, rather than
        // an all-off stack: the tone curve is part of the output transform, so
        // a scene that names only a vignette should not lose it and start
        // clipping its highlights.
        void ResetEffectsToDefaults();

        // ========================
        // Grade presets
        // ========================
        //
        // A preset is a bundle of colour-grade parameter values, not a
        // separate effect. Applying one enables the grade and overwrites its
        // parameters, leaving every other effect alone.

        bool ApplyPreset(const char* presetName);
        int GetPresetCount() const;
        const char* GetPresetName(int index) const;
        const char* GetActivePresetName() const;

        // ========================
        // Authoring
        // ========================

        // Writes the current stack to the log as a scene-file postProcess
        // block, ready to paste. Nothing is written to disk.
        void PrintYaml() const;

        // ========================
        // Frame
        // ========================

        // Runs bloom's passes if it is enabled, then the fused pass into the
        // backbuffer. sceneColorTexture is the offscreen scene target.
        void Execute(Gfx::TextureHandle sceneColorTexture,
                     Gfx::SamplerHandle sampler,
                     int width,
                     int height);

    private:
        void ConfigureEffects();
        void UploadUberParameters();
        void RunBloomPasses(Gfx::TextureHandle sceneColorTexture, Gfx::SamplerHandle sampler);
        void EnsureBloomTargets(int width, int height);
        void DestroyBloomTargets();

        static const int PRESET_COUNT = 5;

        PostProcessEffect         effects[static_cast<int>(PostProcessEffectId::Max)];
        const char*               activePresetName;

        Material*                 uberMaterial;
        Material*                 bloomPrefilterMaterial;
        Material*                 bloomBlurMaterial;
        RenderableFullscreenQuad* fullscreenQuad;

        // Bloom works at half resolution: the blur is wide and low frequency,
        // so the lost detail is not visible, and it quarters the bandwidth of
        // every bloom pass.
        Gfx::RenderTargetHandle   bloomTarget[2];
        Gfx::TextureHandle        bloomTexture[2];
        int                       bloomWidth;
        int                       bloomHeight;
    };
}

#endif // POSTPROCESS_H
