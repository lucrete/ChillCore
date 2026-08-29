#include "DevUiViewAbout.h"

#include <string>

#include "BuildInfo.h"
#include "imgui.h"

namespace CC
{
    void DevUiViewAbout::Draw()
    {
        ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_FirstUseEver);

        if (ImGui::Begin(GetName(), &isVisible, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("ChillCore");
            ImGui::Separator();

            ImGui::Text("Build id:      %s", BuildInfo::GetBuildId());
            ImGui::Text("Commit:        %s", BuildInfo::GetCommitHash());
            ImGui::Text("Configuration: %s", BuildInfo::GetConfiguration());
            ImGui::Text("Built:         %s", BuildInfo::GetTimestamp());

            if (!BuildInfo::IsLabelled())
            {
                ImGui::Separator();
                ImGui::TextWrapped(
                    "This build was not produced by the build pipeline, so it carries no "
                    "identifier and cannot be reproduced from a record.");
            }

            ImGui::Separator();

            if (ImGui::Button("Copy"))
            {
                std::string details =
                    std::string("Build id: ") + BuildInfo::GetBuildId() +
                    "\nCommit: " + BuildInfo::GetCommitHash() +
                    "\nConfiguration: " + BuildInfo::GetConfiguration() +
                    "\nBuilt: " + BuildInfo::GetTimestamp();

                ImGui::SetClipboardText(details.c_str());
            }
        }

        ImGui::End();
    }

    const char* DevUiViewAbout::GetName() const
    {
        return "About";
    }
}
