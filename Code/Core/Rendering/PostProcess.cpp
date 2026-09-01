#include "PostProcess.h"

#include <cstring>

#include "CCAssert.h"
#include "CCVector4.h"
#include "GfxDescriptions.h"
#include "GfxRenderApi.h"
#include "Material.h"
#include "MaterialManager.h"
#include "PrintManager.h"
#include "RenderableFullscreenQuad.h"

namespace
{
    // Parameter names are the scene-file keys and the panel's slider labels,
    // so they are declared once here and referenced everywhere else.
    const char* EFFECT_NAME_BLOOM      = "bloom";
    const char* EFFECT_NAME_VIGNETTE   = "vignette";
    const char* EFFECT_NAME_COLORGRADE = "colorGrade";
    const char* EFFECT_NAME_TONEMAP    = "tonemap";

    char ToLowerAscii(char value)
    {
        return (value >= 'A' && value <= 'Z') ? static_cast<char>(value - 'A' + 'a') : value;
    }

    bool EqualsIgnoreCase(const char* left, const char* right)
    {
        int index = 0;
        while (left[index] != '\0' && ToLowerAscii(left[index]) == ToLowerAscii(right[index]))
        {
            index++;
        }

        return ToLowerAscii(left[index]) == ToLowerAscii(right[index]);
    }

    struct GradePreset
    {
        const char* name;
        float       contrast;
        float       saturation;
        float       temperature;
        float       tint;
        CC::Vector3 lift;
        CC::Vector3 gamma;
        CC::Vector3 gain;
    };

    // Descriptive rather than branded: these are ordinary grade bundles, and
    // naming them after someone else's filters would tie the scene format to
    // a third party's product.
    const GradePreset GRADE_PRESETS[] =
    {
        //  name       contrast satur.  temp.   tint         lift (r,g,b)                  gamma (r,g,b)                 gain (r,g,b)
        {  "Neutral",   1.00f,   1.00f,  0.00f,  0.00f, { 0.00f,  0.00f,  0.00f}, {1.00f, 1.00f, 1.00f}, {1.00f, 1.00f, 1.00f} },
        // Warm lifts the shadows towards amber and gains the highlights
        // towards straw, which is the split a warm grade actually makes.
        {  "Warm",      1.05f,   1.10f,  0.25f,  0.05f, { 0.03f,  0.01f, -0.01f}, {1.00f, 1.00f, 1.02f}, {1.06f, 1.02f, 0.96f} },
        // Cool is the mirror: shadows towards blue, highlights cooled.
        {  "Cool",      1.05f,   0.95f, -0.25f, -0.05f, {-0.01f,  0.00f,  0.03f}, {1.02f, 1.00f, 1.00f}, {0.96f, 1.00f, 1.06f} },
        // Faded is the film look: shadows lifted off black, highlights pulled
        // down, so the image never reaches either end of the range.
        {  "Faded",     0.85f,   0.80f,  0.05f,  0.02f, { 0.07f,  0.06f,  0.05f}, {1.10f, 1.10f, 1.08f}, {0.94f, 0.95f, 0.97f} },
        // Noir removes colour, then leans the remaining grey slightly cold.
        {  "Noir",      1.30f,   0.00f,  0.00f,  0.00f, {-0.02f, -0.02f, -0.01f}, {0.95f, 0.95f, 0.97f}, {1.00f, 1.00f, 1.02f} },
    };
}

namespace CC
{
    PostProcess::PostProcess()
        : activePresetName("Neutral")
        , uberMaterial(nullptr)
        , bloomPrefilterMaterial(nullptr)
        , bloomBlurMaterial(nullptr)
        , fullscreenQuad(nullptr)
        , bloomWidth(0)
        , bloomHeight(0)
    {
        ConfigureEffects();
    }

    PostProcess::~PostProcess()
    {
    }

    void PostProcess::Init()
    {
        MaterialManager* materialManager = MaterialManager::Get();

        materialManager->CreateMaterial("PostProcessUber", "PostProcessUber");
        materialManager->CreateMaterial("PostProcessBloomPrefilter", "PostProcessBloomPrefilter");
        materialManager->CreateMaterial("PostProcessBloomBlur", "PostProcessBloomBlur");

        uberMaterial           = materialManager->GetMaterial("PostProcessUber");
        bloomPrefilterMaterial = materialManager->GetMaterial("PostProcessBloomPrefilter");
        bloomBlurMaterial      = materialManager->GetMaterial("PostProcessBloomBlur");

        // Screen-space passes, like the fade overlay: there is no depth to
        // test against in a fullscreen resolve.
        uberMaterial->SetDepthTestEnabled(false);
        bloomPrefilterMaterial->SetDepthTestEnabled(false);
        bloomBlurMaterial->SetDepthTestEnabled(false);

        fullscreenQuad = new RenderableFullscreenQuad(uberMaterial);
    }

    void PostProcess::Shutdown()
    {
        DestroyBloomTargets();

        delete fullscreenQuad;
        fullscreenQuad = nullptr;
    }

    // ========================
    // Effects
    // ========================

    PostProcessEffect& PostProcess::GetEffect(PostProcessEffectId id)
    {
        return effects[static_cast<int>(id)];
    }

    const PostProcessEffect& PostProcess::GetEffect(PostProcessEffectId id) const
    {
        return effects[static_cast<int>(id)];
    }

    int PostProcess::GetEffectCount() const
    {
        return static_cast<int>(PostProcessEffectId::Max);
    }

    PostProcessEffect& PostProcess::GetEffectByIndex(int index)
    {
        CC_ASSERT(index >= 0 && index < GetEffectCount(), "Post-process effect index out of range");
        return effects[index];
    }

    PostProcessEffect* PostProcess::FindEffect(const char* effectName)
    {
        PostProcessEffect* result = nullptr;

        for (int i = 0; i < GetEffectCount() && result == nullptr; i++)
        {
            if (EqualsIgnoreCase(effects[i].GetName(), effectName))
            {
                result = &effects[i];
            }
        }

        return result;
    }

    bool PostProcess::IsAnyEffectEnabled() const
    {
        bool result = false;

        for (int i = 0; i < GetEffectCount() && !result; i++)
        {
            result = effects[i].IsEnabled();
        }

        return result;
    }

    void PostProcess::DisableAllEffects()
    {
        for (int i = 0; i < GetEffectCount(); i++)
        {
            effects[i].SetEnabled(false);
        }
    }

    // ========================
    // Grade presets
    // ========================

    void PostProcess::ResetEffectsToDefaults()
    {
        for (int i = 0; i < static_cast<int>(PostProcessEffectId::Max); i++)
        {
            effects[i].ResetEnabledToDefault();
            effects[i].ResetToDefaults();
        }

        activePresetName = "Neutral";
    }

    bool PostProcess::ApplyPreset(const char* presetName)
    {
        bool result = false;

        for (int i = 0; i < PRESET_COUNT && !result; i++)
        {
            const GradePreset& preset = GRADE_PRESETS[i];

            if (strcmp(preset.name, presetName) == 0)
            {
                PostProcessEffect& grade = GetEffect(PostProcessEffectId::ColorGrade);
                grade.SetParamValue("contrast",    preset.contrast);
                grade.SetParamValue("saturation",  preset.saturation);
                grade.SetParamValue("temperature", preset.temperature);
                grade.SetParamValue("tint",        preset.tint);
                grade.SetParamColor("lift",        preset.lift);
                grade.SetParamColor("gamma",       preset.gamma);
                grade.SetParamColor("gain",        preset.gain);
                grade.SetEnabled(true);

                activePresetName = preset.name;
                result = true;
            }
        }

        return result;
    }

    int PostProcess::GetPresetCount() const
    {
        return PRESET_COUNT;
    }

    const char* PostProcess::GetPresetName(int index) const
    {
        CC_ASSERT(index >= 0 && index < PRESET_COUNT, "Grade preset index out of range");
        return GRADE_PRESETS[index].name;
    }

    const char* PostProcess::GetActivePresetName() const
    {
        return activePresetName;
    }

    // ========================
    // Authoring
    // ========================

    void PostProcess::PrintYaml() const
    {
        CCPrint(PrintManager::CHANNEL_ALWAYS, "postProcess:");

        for (int i = 0; i < GetEffectCount(); i++)
        {
            const PostProcessEffect& effect = effects[i];
            CCPrint(PrintManager::CHANNEL_ALWAYS, "  %s:", effect.GetName());
            CCPrint(PrintManager::CHANNEL_ALWAYS, "    enabled: %s", effect.IsEnabled() ? "true" : "false");

            for (int p = 0; p < effect.GetParamCount(); p++)
            {
                const PostProcessParam& param = effect.GetParam(p);

                if (param.type == PostProcessParamType::Color)
                {
                    CCPrint(PrintManager::CHANNEL_ALWAYS, "    %s: [%.3f, %.3f, %.3f]",
                            param.name, param.value.x, param.value.y, param.value.z);
                }
                else
                {
                    CCPrint(PrintManager::CHANNEL_ALWAYS, "    %s: %.3f", param.name, param.value.x);
                }
            }
        }
    }

    // ========================
    // Frame
    // ========================

    void PostProcess::Execute(Gfx::TextureHandle sceneColorTexture,
                              Gfx::SamplerHandle sampler,
                              int width,
                              int height)
    {
        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();

        bool isBloomEnabled = GetEffect(PostProcessEffectId::Bloom).IsEnabled();
        if (isBloomEnabled)
        {
            EnsureBloomTargets(width, height);
            RunBloomPasses(sceneColorTexture, sampler, width, height);
        }

        UploadUberParameters();

        gfxApi->BeginRenderPass(gfxApi->GetBackbuffer(), "PostProcess");
        gfxApi->InvalidateCachedState();

        fullscreenQuad->SetMaterial(uberMaterial);
        fullscreenQuad->PreRender();

        // Unit 0 is the scene, unit 1 the bloom result. PreRender bound the
        // material's own base texture at unit 0, so both are overridden here.
        gfxApi->BindTexture(0, sceneColorTexture, sampler);
        gfxApi->BindTexture(1, isBloomEnabled ? bloomTexture[0] : sceneColorTexture, sampler);

        fullscreenQuad->Render(nullptr);

        gfxApi->EndRenderPass();
    }

    // ========================
    // Private
    // ========================

    void PostProcess::ConfigureEffects()
    {
        PostProcessEffect& bloom = GetEffect(PostProcessEffectId::Bloom);
        bloom.Configure(EFFECT_NAME_BLOOM, false);
        bloom.AddParam("threshold", 1.0f, 0.0f, 5.0f);
        bloom.AddParam("knee",      0.5f, 0.0f, 1.0f);
        bloom.AddParam("intensity", 0.6f, 0.0f, 3.0f);

        PostProcessEffect& vignette = GetEffect(PostProcessEffectId::Vignette);
        vignette.Configure(EFFECT_NAME_VIGNETTE, false);
        vignette.AddParam("intensity",  0.5f, 0.0f, 1.0f);
        vignette.AddParam("smoothness", 0.5f, 0.01f, 1.0f);
        vignette.AddParam("roundness",  1.0f, 0.0f, 1.0f);

        PostProcessEffect& grade = GetEffect(PostProcessEffectId::ColorGrade);
        grade.Configure(EFFECT_NAME_COLORGRADE, false);
        grade.AddParam("contrast",    1.0f, 0.0f, 2.0f);
        grade.AddParam("saturation",  1.0f, 0.0f, 2.0f);
        grade.AddParam("temperature", 0.0f, -1.0f, 1.0f);
        grade.AddParam("tint",        0.0f, -1.0f, 1.0f);
        // Per channel: lift moves the shadows, gamma the mid-tones, gain the
        // highlights, and a grade tints each of the three differently. A
        // single number per control can only brighten or darken.
        grade.AddColorParam("lift",   Vector3(0.0f, 0.0f, 0.0f), -0.5f, 0.5f);
        grade.AddColorParam("gamma",  Vector3(1.0f, 1.0f, 1.0f),  0.1f, 3.0f);
        grade.AddColorParam("gain",   Vector3(1.0f, 1.0f, 1.0f),  0.0f, 2.0f);

        // On by default. The scene is rendered in scene-linear light, so
        // values above white are real and need a curve to roll them off.
        // Switching it off falls back to a hard clamp, which clips an emissive
        // surface or a bright highlight flat instead.
        //
        // Exposure applies either way: it scales the scene before the curve,
        // and is as meaningful when the frame is clamped as when it is
        // tone mapped.
        PostProcessEffect& tonemap = GetEffect(PostProcessEffectId::Tonemap);
        tonemap.Configure(EFFECT_NAME_TONEMAP, true);
        tonemap.AddParam("exposure", 1.0f, 0.0f, 8.0f);
    }

    void PostProcess::UploadUberParameters()
    {
        const PostProcessEffect& bloom    = GetEffect(PostProcessEffectId::Bloom);
        const PostProcessEffect& vignette = GetEffect(PostProcessEffectId::Vignette);
        const PostProcessEffect& grade    = GetEffect(PostProcessEffectId::ColorGrade);
        const PostProcessEffect& tonemap  = GetEffect(PostProcessEffectId::Tonemap);

        // Packed into vec4s rather than sent as loose floats: the shader's
        // parameter block is std140, where a lone float still occupies a
        // 16-byte slot, and the block has a fixed capacity.
        uberMaterial->SetUniform("bloomAndExposure", Vector4(
            bloom.IsEnabled() ? 1.0f : 0.0f,
            bloom.GetParamValue("intensity"),
            tonemap.GetParamValue("exposure"),
            tonemap.IsEnabled() ? 1.0f : 0.0f));

        uberMaterial->SetUniform("vignetteParams", Vector4(
            vignette.IsEnabled() ? vignette.GetParamValue("intensity") : 0.0f,
            vignette.GetParamValue("smoothness"),
            vignette.GetParamValue("roundness"),
            0.0f));

        uberMaterial->SetUniform("gradeParamsA", Vector4(
            grade.IsEnabled() ? 1.0f : 0.0f,
            grade.GetParamValue("contrast"),
            grade.GetParamValue("saturation"),
            grade.GetParamValue("temperature")));

        uberMaterial->SetUniform("gradeParamsB", Vector4(
            grade.GetParamValue("tint"), 0.0f, 0.0f, 0.0f));

        // One vec4 each: std140 pads a vec3 to 16 bytes anyway, so nothing is
        // saved by packing them together and the w stays free for later use.
        Vector3 lift  = grade.GetParamColor("lift");
        Vector3 gamma = grade.GetParamColor("gamma");
        Vector3 gain  = grade.GetParamColor("gain");

        uberMaterial->SetUniform("gradeLift",  Vector4(lift.x,  lift.y,  lift.z,  0.0f));
        uberMaterial->SetUniform("gradeGamma", Vector4(gamma.x, gamma.y, gamma.z, 0.0f));
        uberMaterial->SetUniform("gradeGain",  Vector4(gain.x,  gain.y,  gain.z,  0.0f));
    }

    void PostProcess::RunBloomPasses(Gfx::TextureHandle sceneColorTexture, Gfx::SamplerHandle sampler,
                                    int sourceWidth, int sourceHeight)
    {
        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();
        const PostProcessEffect& bloom = GetEffect(PostProcessEffectId::Bloom);

        float threshold = bloom.GetParamValue("threshold");
        float knee      = bloom.GetParamValue("knee");

        // The bright pass weights the four source texels behind each of its
        // own, so it needs the source texel size rather than its own.
        float sourceTexelU = 1.0f / static_cast<float>(sourceWidth > 0 ? sourceWidth : 1);
        float sourceTexelV = 1.0f / static_cast<float>(sourceHeight > 0 ? sourceHeight : 1);

        bloomPrefilterMaterial->SetUniform("prefilterParams",
            Vector4(threshold, knee, sourceTexelU, sourceTexelV));

        // Pass 1: bright pass, full res in, half res out.
        gfxApi->BeginRenderPass(bloomTarget[0], "PostProcess/BloomPrefilter");
        gfxApi->InvalidateCachedState();
        fullscreenQuad->SetMaterial(bloomPrefilterMaterial);
        fullscreenQuad->PreRender();
        gfxApi->BindTexture(0, sceneColorTexture, sampler);
        fullscreenQuad->Render(nullptr);
        gfxApi->EndRenderPass();

        float texelU = 1.0f / static_cast<float>(bloomWidth);
        float texelV = 1.0f / static_cast<float>(bloomHeight);

        // Pass 2 and 3: separable blur, horizontal then vertical. Ping-pong
        // because a pass cannot sample the target it is writing.
        bloomBlurMaterial->SetUniform("blurParams", Vector4(texelU, texelV, 1.0f, 0.0f));
        gfxApi->BeginRenderPass(bloomTarget[1], "PostProcess/BloomBlurH");
        gfxApi->InvalidateCachedState();
        fullscreenQuad->SetMaterial(bloomBlurMaterial);
        fullscreenQuad->PreRender();
        gfxApi->BindTexture(0, bloomTexture[0], sampler);
        fullscreenQuad->Render(nullptr);
        gfxApi->EndRenderPass();

        bloomBlurMaterial->SetUniform("blurParams", Vector4(texelU, texelV, 0.0f, 1.0f));
        gfxApi->BeginRenderPass(bloomTarget[0], "PostProcess/BloomBlurV");
        gfxApi->InvalidateCachedState();
        fullscreenQuad->SetMaterial(bloomBlurMaterial);
        fullscreenQuad->PreRender();
        gfxApi->BindTexture(0, bloomTexture[1], sampler);
        fullscreenQuad->Render(nullptr);
        gfxApi->EndRenderPass();
    }

    void PostProcess::EnsureBloomTargets(int width, int height)
    {
        int halfWidth  = width > 1 ? width / 2 : 1;
        int halfHeight = height > 1 ? height / 2 : 1;

        bool isCurrent = bloomTarget[0].IsValid()
            && halfWidth == bloomWidth
            && halfHeight == bloomHeight;

        if (!isCurrent)
        {
            DestroyBloomTargets();

            Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();
            const Gfx::GfxCapabilities& caps = gfxApi->GetCapabilities();

            // Bloom carries values above white until the tone map compresses
            // them, so it wants the same precision the scene target has.
            Gfx::TextureFormat format = caps.supportsHalfFloatRenderTargets
                ? Gfx::TextureFormat::Rgba16Float
                : Gfx::TextureFormat::Rgba8Unorm;

            for (int i = 0; i < 2; i++)
            {
                Gfx::TextureDescription colorDesc;
                colorDesc.width          = halfWidth;
                colorDesc.height         = halfHeight;
                colorDesc.format         = format;
                colorDesc.isRenderTarget = true;
                colorDesc.debugName      = "PostProcess::BloomColor";
                bloomTexture[i] = gfxApi->CreateTexture(colorDesc);

                Gfx::RenderTargetDescription targetDesc;
                targetDesc.width                          = halfWidth;
                targetDesc.height                         = halfHeight;
                targetDesc.colorAttachmentCount           = 1;
                targetDesc.colorAttachments[0].texture    = bloomTexture[i];
                targetDesc.colorAttachments[0].loadOp     = Gfx::LoadOp::Clear;
                targetDesc.colorAttachments[0].storeOp    = Gfx::StoreOp::Store;
                targetDesc.hasDepthStencil                = false;
                targetDesc.debugName                      = "PostProcess::BloomTarget";
                bloomTarget[i] = gfxApi->CreateRenderTarget(targetDesc);
            }

            bloomWidth  = halfWidth;
            bloomHeight = halfHeight;
        }
    }

    void PostProcess::DestroyBloomTargets()
    {
        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();

        for (int i = 0; i < 2; i++)
        {
            if (bloomTarget[i].IsValid())
            {
                gfxApi->DestroyRenderTarget(bloomTarget[i]);
                bloomTarget[i] = Gfx::RenderTargetHandle();
            }
            if (bloomTexture[i].IsValid())
            {
                gfxApi->DestroyTexture(bloomTexture[i]);
                bloomTexture[i] = Gfx::TextureHandle();
            }
        }

        bloomWidth  = 0;
        bloomHeight = 0;
    }
}
