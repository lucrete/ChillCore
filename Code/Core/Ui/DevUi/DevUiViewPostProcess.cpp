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
                        ImGui::SliderFloat(param.name, &param.value, param.minValue, param.maxValue);
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

            ImGui::Separator();

            // Prints rather than writes: there is no YAML writer, and a scene
            // file is hand-authored with comments a serialiser would discard.
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
