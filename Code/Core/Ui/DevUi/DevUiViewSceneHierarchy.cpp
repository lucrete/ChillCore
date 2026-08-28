#include "DevUiViewSceneHierarchy.h"
#include "imgui.h"
#include "SceneHierarchy.h"
#include "SceneObject.h"
#include "Component.h"
#include <cstdint>

namespace CC
{
    DevUiViewSceneHierarchy::DevUiViewSceneHierarchy()
    {
    }

    const char* DevUiViewSceneHierarchy::GetName() const
    {
        return "Scene Hierarchy";
    }

    // ========================
    // Draw
    // ========================

    void DevUiViewSceneHierarchy::Draw()
    {
        int currentVersion = SceneHierarchy::Get()->GetVersion();
        if (currentVersion != lastSceneVersion)
        {
            selectedObject = nullptr;
            lastSceneVersion = currentVersion;
        }

        ImGui::SetNextWindowPos(ImVec2(370, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin("Scene Hierarchy", nullptr, ImGuiWindowFlags_NoFocusOnAppearing))
        {
            ImGui::End();
            return;
        }

        float availableWidth = ImGui::GetContentRegionAvail().x;
        float treeWidth = availableWidth * 0.45f;

        ImGui::BeginChild("SceneTree", ImVec2(treeWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
        DrawSceneTree();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("Properties", ImVec2(0, 0), ImGuiChildFlags_Borders);
        DrawPropertiesPanel();
        ImGui::EndChild();

        ImGui::End();
    }

    // ========================
    // Scene Tree
    // ========================

    void DevUiViewSceneHierarchy::DrawSceneTree()
    {
        const std::vector<SceneObject*>& rootObjects = SceneHierarchy::Get()->GetRootObjects();

        if (rootObjects.empty())
        {
            ImGui::TextDisabled("No scene loaded");
            return;
        }

        for (SceneObject* object : rootObjects)
        {
            DrawObjectNode(object);
        }
    }

    void DevUiViewSceneHierarchy::DrawObjectNode(SceneObject* object)
    {
        const std::vector<SceneObject*>& children = object->GetChildren();
        bool isLeaf = children.empty();
        bool isSelected = (object == selectedObject);
        bool isDisabled = !object->IsEnabled();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (isLeaf)
        {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }
        if (isSelected)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        if (isDisabled)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        }

        bool isOpen = ImGui::TreeNodeEx((void*)(intptr_t)object, flags, "%s", object->GetName().c_str());

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            selectedObject = object;
        }

        if (isDisabled)
        {
            ImGui::PopStyleColor();
        }

        if (isOpen && !isLeaf)
        {
            for (SceneObject* child : children)
            {
                DrawObjectNode(child);
            }
            ImGui::TreePop();
        }
    }

    // ========================
    // Properties Panel
    // ========================

    void DevUiViewSceneHierarchy::DrawPropertiesPanel()
    {
        if (selectedObject == nullptr)
        {
            ImGui::TextDisabled("Select an object");
            return;
        }

        ImGui::Text("%s", selectedObject->GetName().c_str());
        ImGui::SameLine();
        if (selectedObject->IsEnabled())
        {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "(Enabled)");
        }
        else
        {
            ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.4f, 1.0f), "(Disabled)");
        }
        ImGui::Separator();

        DrawTransformProperties(selectedObject);
        DrawComponentList(selectedObject);
    }

    void DevUiViewSceneHierarchy::DrawTransformProperties(SceneObject* object)
    {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const Transform& transform = object->GetTransform();
            const Vector3& position = transform.GetPosition();
            Vector3 rotation = transform.GetRotation();
            const Vector3& scale = transform.GetScale();

            ImGui::Text("Position: %.2f, %.2f, %.2f", position.x, position.y, position.z);
            ImGui::Text("Rotation: %.1f, %.1f, %.1f", rotation.x, rotation.y, rotation.z);
            ImGui::Text("Scale:    %.2f, %.2f, %.2f", scale.x, scale.y, scale.z);

            Vector3 worldPosition = object->GetWorldPosition();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "World Pos: %.2f, %.2f, %.2f",
                worldPosition.x, worldPosition.y, worldPosition.z);
        }
    }

    void DevUiViewSceneHierarchy::DrawComponentList(SceneObject* object)
    {
        if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const std::vector<Component*>& components = object->GetComponents();

            if (components.empty())
            {
                ImGui::TextDisabled("No components");
                return;
            }

            for (Component* component : components)
            {
                bool isDisabled = !component->IsEnabled();
                if (isDisabled)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                }

                ImGui::BulletText("%s", component->GetTypeName());

                if (isDisabled)
                {
                    ImGui::PopStyleColor();
                }
            }
        }
    }
}
