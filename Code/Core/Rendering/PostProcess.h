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
        // separate effect. Applying one enables the grade and overwrites the
        // parameters it names, leaving every other effect alone.
        //
        // Presets are data, loaded from a file rather than compiled in, so a
        // look can be authored, named and shared without a rebuild. Each entry
        // is stored by parameter name rather than as a fixed set of fields, so
        // adding a control to the grade does not invalidate a saved look — an
        // older preset simply does not mention the new control.

        // Where looks live. One path, so the panel saves to the file the
        // engine loads and a round trip needs no argument passing.
        static const char* PRESET_FILE_PATH;

        bool LoadPresets(const char* filePath);
        bool SavePresets(const char* filePath) const;

        bool ApplyPreset(const char* presetName);
        int GetPresetCount() const;
        const char* GetPresetName(int index) const;
        const char* GetActivePresetName() const;

        // Captures the grade's current values under this name, replacing a
        // preset of the same name. Does not write the file; the caller decides
        // when to persist.
        bool CaptureCurrentAsPreset(const char* presetName);

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
        void RunBloomPasses(Gfx::TextureHandle sceneColorTexture, Gfx::SamplerHandle sampler,
                            int sourceWidth, int sourceHeight);
        void EnsureBloomTargets(int width, int height);
        void DestroyBloomTargets();

        static const int MAX_GRADE_PRESETS   = 24;
        static const int MAX_PRESET_NAME      = 32;

        // One saved value of one grade parameter. Held by name so a preset
        // survives the grade gaining or losing controls.
        struct GradePresetEntry
        {
            char    paramName[MAX_PRESET_NAME] = {};
            Vector3 value;
        };

        struct GradePreset
        {
            char            name[MAX_PRESET_NAME] = {};
            GradePresetEntry entries[MAX_POST_PROCESS_PARAMS];
            int             entryCount = 0;
        };

        int FindPresetIndex(const char* presetName) const;

        GradePreset               presets[MAX_GRADE_PRESETS];
        int                       presetCount;

        PostProcessEffect         effects[static_cast<int>(PostProcessEffectId::Max)];
        char                      activePresetName[MAX_PRESET_NAME];

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
