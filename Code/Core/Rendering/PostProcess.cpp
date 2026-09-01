#include "PostProcess.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <rapidyaml-0.10.0.hpp>
#include "PlatformFileSystem.h"
#include "YamlUtils.h"

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

    void CopyBounded(char* destination, const char* source, int capacity)
    {
        int index = 0;
        while (source != nullptr && source[index] != '\0' && index < capacity - 1)
        {
            destination[index] = source[index];
            index++;
        }
        destination[index] = '\0';
    }


}

namespace CC
{
    PostProcess::PostProcess()
        : presetCount(0)
        , uberMaterial(nullptr)
        , bloomPrefilterMaterial(nullptr)
        , bloomBlurMaterial(nullptr)
        , fullscreenQuad(nullptr)
        , bloomWidth(0)
        , bloomHeight(0)
    {
        // The grade starts at its neutral defaults, which is what the Neutral
        // preset holds, so naming it here keeps the reported look and the
        // actual parameter values in step from the first frame.
        CopyBounded(activePresetName, "Neutral", MAX_PRESET_NAME);

        ConfigureEffects();
    }

    PostProcess::~PostProcess()
    {
    }

    const char* PostProcess::PRESET_FILE_PATH = "Data/PostProcess/GradePresets.yaml";

    void PostProcess::Init()
    {
        LoadPresets(PRESET_FILE_PATH);

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

        CopyBounded(activePresetName, "Neutral", MAX_PRESET_NAME);
    }

    bool PostProcess::LoadPresets(const char* filePath)
    {
        std::string content;
        bool result = PlatformFileSystem::Get()->ReadFileText(filePath, content);

        if (!result)
        {
            CCPrint(PrintManager::CHANNEL_WARN, "PostProcess: could not read presets from %s", filePath);
        }
        else
        {
            ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(content));
            ryml::ConstNodeRef root = tree.rootref();
            presetCount = 0;

            if (root.has_child("presets"))
            {
                for (ryml::ConstNodeRef presetNode : root["presets"].children())
                {
                    if (presetCount >= MAX_GRADE_PRESETS)
                    {
                        CCPrint(PrintManager::CHANNEL_WARN,
                                "PostProcess: more than %d presets, the rest are ignored", MAX_GRADE_PRESETS);
                    }
                    else if (presetNode.has_child("name"))
                    {
                        GradePreset& preset = presets[presetCount];
                        preset.entryCount = 0;
                        CopyBounded(preset.name, NodeToString(presetNode["name"]).c_str(), MAX_PRESET_NAME);

                        for (ryml::ConstNodeRef valueNode : presetNode.children())
                        {
                            std::string key = KeyToString(valueNode);

                            if (key != "name" && preset.entryCount < MAX_POST_PROCESS_PARAMS)
                            {
                                GradePresetEntry& entry = preset.entries[preset.entryCount];
                                CopyBounded(entry.paramName, key.c_str(), MAX_PRESET_NAME);

                                if (valueNode.num_children() >= 3)
                                {
                                    entry.value = Vector3(NodeToFloat(valueNode[0]),
                                                          NodeToFloat(valueNode[1]),
                                                          NodeToFloat(valueNode[2]));
                                }
                                else
                                {
                                    float scalar = NodeToFloat(valueNode);
                                    entry.value = Vector3(scalar, scalar, scalar);
                                }
                                preset.entryCount++;
                            }
                        }
                        presetCount++;
                    }
                }
            }

            CCPrint(PrintManager::CHANNEL_ALWAYS, "PostProcess: loaded %d grade presets from %s",
                    presetCount, filePath);
        }

        return result;
    }

    bool PostProcess::SavePresets(const char* filePath) const
    {
        std::string yaml;
        yaml += "# Grade presets. A preset names colour-grade parameters and the\n";
        yaml += "# values to set them to; anything it does not name is left alone.\n";
        yaml += "presets:\n";

        char line[256];
        for (int i = 0; i < presetCount; i++)
        {
            const GradePreset& preset = presets[i];
            snprintf(line, sizeof(line), "  - name: %s\n", preset.name);
            yaml += line;

            for (int e = 0; e < preset.entryCount; e++)
            {
                const GradePresetEntry& entry = preset.entries[e];
                const PostProcessEffect& grade = GetEffect(PostProcessEffectId::ColorGrade);
                int paramIndex = -1;
                for (int p = 0; p < grade.GetParamCount(); p++)
                {
                    if (EqualsIgnoreCase(grade.GetParam(p).name, entry.paramName))
                    {
                        paramIndex = p;
                    }
                }

                bool isColor = paramIndex >= 0
                    && grade.GetParam(paramIndex).type == PostProcessParamType::Color;

                if (isColor)
                {
                    snprintf(line, sizeof(line), "    %s: [%.4f, %.4f, %.4f]\n",
                             entry.paramName, entry.value.x, entry.value.y, entry.value.z);
                }
                else
                {
                    snprintf(line, sizeof(line), "    %s: %.4f\n", entry.paramName, entry.value.x);
                }
                yaml += line;
            }
        }

        bool result = PlatformFileSystem::Get()->WriteFileTextAtomic(filePath, yaml);
        CCPrint(PrintManager::CHANNEL_ALWAYS, result
                ? "PostProcess: saved %d grade presets to %s"
                : "PostProcess: failed to write %s", presetCount, filePath);
        return result;
    }

    int PostProcess::FindPresetIndex(const char* presetName) const
    {
        int result = -1;
        for (int i = 0; i < presetCount; i++)
        {
            if (EqualsIgnoreCase(presets[i].name, presetName))
            {
                result = i;
            }
        }
        return result;
    }

    bool PostProcess::ApplyPreset(const char* presetName)
    {
        int index = FindPresetIndex(presetName);
        bool result = index >= 0;

        if (result)
        {
            const GradePreset& preset = presets[index];
            PostProcessEffect& grade = GetEffect(PostProcessEffectId::ColorGrade);

            // Start from defaults so a preset that does not mention a control
            // gets the neutral value rather than whatever the last look left.
            grade.ResetToDefaults();

            for (int e = 0; e < preset.entryCount; e++)
            {
                if (!grade.SetParamColor(preset.entries[e].paramName, preset.entries[e].value))
                {
                    CCPrint(PrintManager::CHANNEL_WARN,
                            "PostProcess: preset '%s' names unknown grade parameter '%s'",
                            preset.name, preset.entries[e].paramName);
                }
            }

            grade.SetEnabled(true);
            CopyBounded(activePresetName, preset.name, MAX_PRESET_NAME);
        }

        return result;
    }

    bool PostProcess::CaptureCurrentAsPreset(const char* presetName)
    {
        int index = FindPresetIndex(presetName);
        bool result = true;

        if (index < 0 && presetCount < MAX_GRADE_PRESETS)
        {
            index = presetCount;
            presetCount++;
        }
        else if (index < 0)
        {
            CCPrint(PrintManager::CHANNEL_WARN, "PostProcess: no room for another preset");
            result = false;
        }

        if (result)
        {
            const PostProcessEffect& grade = GetEffect(PostProcessEffectId::ColorGrade);
            GradePreset& preset = presets[index];
            CopyBounded(preset.name, presetName, MAX_PRESET_NAME);
            preset.entryCount = 0;

            for (int p = 0; p < grade.GetParamCount(); p++)
            {
                const PostProcessParam& param = grade.GetParam(p);
                GradePresetEntry& entry = preset.entries[preset.entryCount];
                CopyBounded(entry.paramName, param.name, MAX_PRESET_NAME);
                entry.value = param.value;
                preset.entryCount++;
            }

            CopyBounded(activePresetName, presetName, MAX_PRESET_NAME);
        }

        return result;
    }

    int PostProcess::GetPresetCount() const
    {
        return presetCount;
    }

    const char* PostProcess::GetPresetName(int index) const
    {
        CC_ASSERT(index >= 0 && index < presetCount, "Grade preset index out of range");
        return presets[index].name;
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

    // Every effect is on by default. The stack is the engine's output
    // transform, not a set of optional extras: the scene renders in
    // scene-linear light and only the fused pass encodes it for display, so a
    // frame with the stack switched off is the exception to reach for
    // deliberately rather than the state to start from. Each effect's default
    // parameters are neutral enough to be the baseline look.
    void PostProcess::ConfigureEffects()
    {
        PostProcessEffect& bloom = GetEffect(PostProcessEffectId::Bloom);
        bloom.Configure(EFFECT_NAME_BLOOM, true);
        bloom.AddParam("threshold", 1.0f, 0.0f, 5.0f);
        bloom.AddParam("knee",      0.5f, 0.0f, 1.0f);
        bloom.AddParam("intensity", 0.6f, 0.0f, 3.0f);

        PostProcessEffect& vignette = GetEffect(PostProcessEffectId::Vignette);
        vignette.Configure(EFFECT_NAME_VIGNETTE, true);
        vignette.AddParam("intensity",  0.5f, 0.0f, 1.0f);
        vignette.AddParam("smoothness", 0.5f, 0.01f, 1.0f);
        vignette.AddParam("roundness",  1.0f, 0.0f, 1.0f);

        PostProcessEffect& grade = GetEffect(PostProcessEffectId::ColorGrade);
        grade.Configure(EFFECT_NAME_COLORGRADE, true);
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

        // A flat colour composited over the image. This is how the classic
        // photographic filters are built, and curves alone cannot reach them.
        // Mode is an index: 0 multiply, 1 screen, 2 overlay, 3 soft light.
        grade.AddColorParam("blendColor", Vector3(0.5f, 0.5f, 0.5f), 0.0f, 1.0f);
        grade.AddParam("blendMode",     0.0f, 0.0f, 3.0f);
        grade.AddParam("blendStrength", 0.0f, 0.0f, 1.0f);

        // Switching the tone curve off falls back to a hard clamp, which
        // clips an emissive surface or a bright highlight flat instead of
        // rolling it off.
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
            grade.GetParamValue("tint"), grade.GetParamValue("blendMode"), 0.0f, 0.0f));

        Vector3 blendColor = grade.GetParamColor("blendColor");
        uberMaterial->SetUniform("gradeBlend", Vector4(
            blendColor.x, blendColor.y, blendColor.z, grade.GetParamValue("blendStrength")));

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
