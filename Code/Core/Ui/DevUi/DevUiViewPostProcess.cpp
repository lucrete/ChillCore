#include "DevUiViewPostProcess.h"

#include "PostProcess.h"
#include "PostProcessEffect.h"
#include "RenderManager.h"
#include "imgui.h"

namespace CC
{
    void DevUiViewPostProcess::Draw()
    {
        ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_FirstUseEver);

        if (ImGui::Begin(GetName(), &isVisible, ImGuiWindowFlags_AlwaysAutoResize))
        {
            PostProcess* postProcess = RenderManager::Get()->GetPostProcess();

            if (!postProcess->IsAnyEffectEnabled())
            {
                ImGui::TextWrapped(
                    "Every effect is off. The pass still runs: it encodes the scene from "
                    "linear light to display space, which is not optional. With the tone "
                    "map off the frame is clamped instead of rolled off.");
                ImGui::Separator();
            }

            for (int i = 0; i < postProcess->GetEffectCount(); i++)
            {
                PostProcessEffect& effect = postProcess->GetEffectByIndex(i);

                ImGui::PushID(i);

                bool isEnabled = effect.IsEnabled();
                if (ImGui::Checkbox(effect.GetName(), &isEnabled))
                {
                    effect.SetEnabled(isEnabled);
                }

                if (isEnabled)
                {
                    ImGui::Indent();
                    for (int p = 0; p < effect.GetParamCount(); p++)
                    {
                        PostProcessParam& param = effect.GetParam(p);

                        if (param.type == PostProcessParamType::Color)
                        {
                            // Edited as three numbers rather than through a
                            // colour picker: these run outside 0..1 — gain and
                            // gamma sit around 1.0 and lift goes negative —
                            // and a picker clamps.
                            ImGui::SliderFloat3(param.name, &param.value.x, param.minValue, param.maxValue);
                        }
                        else
                        {
                            ImGui::SliderFloat(param.name, &param.value.x, param.minValue, param.maxValue);
                        }
                    }

                    if (ImGui::SmallButton("Reset"))
                    {
                        effect.ResetToDefaults();
                    }
                    ImGui::Unindent();
                }

                ImGui::PopID();
            }

            ImGui::Separator();
            DrawPresets();
            DrawPresetSaving();

            ImGui::Separator();

            // Prints rather than writes, unlike the preset save above. This
            // dumps a block for a scene file, and a scene file is hand-authored
            // with comments and surrounding content a serialiser would discard.
            if (ImGui::Button("Print YAML"))
            {
                postProcess->PrintYaml();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(to the console)");

            if (ImGui::Button("Disable all"))
            {
                postProcess->DisableAllEffects();
            }
        }

        ImGui::End();
    }

    const char* DevUiViewPostProcess::GetName() const
    {
        return "Post Process";
    }

    // ========================
    // Private
    // ========================

    // Saving writes the whole preset file, not just the new look: the file is
    // the engine's source of presets, so the panel and the file cannot drift.
    void DevUiViewPostProcess::DrawPresetSaving()
    {
        PostProcess* postProcess = RenderManager::Get()->GetPostProcess();

        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputText("##presetName", presetNameInput, MAX_PRESET_NAME_INPUT);
        ImGui::SameLine();

        bool hasName = presetNameInput[0] != '\0';
        if (!hasName)
        {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Save look"))
        {
            if (postProcess->CaptureCurrentAsPreset(presetNameInput))
            {
                postProcess->SavePresets(PostProcess::PRESET_FILE_PATH);
            }
        }

        if (!hasName)
        {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        ImGui::TextDisabled("(saves every preset to the file)");
    }

    void DevUiViewPostProcess::DrawPresets()
    {
        PostProcess* postProcess = RenderManager::Get()->GetPostProcess();

        ImGui::Text("Grade preset: %s", postProcess->GetActivePresetName());

        for (int i = 0; i < postProcess->GetPresetCount(); i++)
        {
            const char* presetName = postProcess->GetPresetName(i);

            if (i > 0)
            {
                ImGui::SameLine();
            }
            if (ImGui::SmallButton(presetName))
            {
                postProcess->ApplyPreset(presetName);
            }
        }
    }
}
