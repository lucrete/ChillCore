// Suppress macro redefinition warnings from Windows headers conflicting with GLFW
#pragma warning(push)
#pragma warning(disable: 4005)
#define RYML_SINGLE_HDR_DEFINE_NOW
#include "SceneLoader.h"
#pragma warning(pop)
#include "SceneHierarchy.h"
#include "SceneObject.h"
#include "Component.h"
#include "ComponentFactory.h"
#include "MaterialManager.h"
#include "PostProcess.h"
#include "PostProcessEffect.h"
#include "RenderManager.h"
#include "PrintManager.h"
#include "GltfLoader.h"
#include "PlatformFileSystem.h"

namespace CC
{
    // ========================
    // Helper Functions
    // ========================

    static std::string NodeToString(ryml::ConstNodeRef node)
    {
        if (!node.has_val())
        {
            return "";
        }
        c4::csubstr val = node.val();
        return std::string(val.data(), val.size());
    }

    static std::string KeyToString(ryml::ConstNodeRef node)
    {
        if (!node.has_key())
        {
            return "";
        }
        c4::csubstr key = node.key();
        return std::string(key.data(), key.size());
    }

    static float NodeToFloat(ryml::ConstNodeRef node)
    {
        if (!node.has_val())
        {
            return 0.0f;
        }
        float value = 0.0f;
        node >> value;
        return value;
    }

    // ========================
    // Scene Loading
    // ========================

    bool SceneLoader::LoadScene(const std::string& filePath, SceneHierarchy& hierarchy)
    {
        bool succeeded = false;

        std::string content;
        if (!PlatformFileSystem::Get()->ReadFileText(filePath.c_str(), content))
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS, "SceneLoader: Failed to open scene file: %s", filePath.c_str());
        }
        else if (content.empty())
        {
            CCPrint(PrintManager::CHANNEL_ALWAYS, "SceneLoader: Scene file is empty: %s", filePath.c_str());
        }
        else
        {
            ryml::Tree tree;
            bool parseOk = true;
            try
            {
                tree = ryml::parse_in_arena(ryml::to_csubstr(content));
            }
            catch (const std::exception& e)
            {
                CCPrint(PrintManager::CHANNEL_ALWAYS, "SceneLoader: Failed to parse YAML: %s", e.what());
                parseOk = false;
            }

            if (parseOk)
            {
                ryml::ConstNodeRef root = tree.rootref();

                // Process scene name
                if (root.has_child("name"))
                {
                    std::string sceneName = NodeToString(root["name"]);
                    CCPrint(PrintManager::CHANNEL_ALWAYS, "SceneLoader: Loading scene '%s'", sceneName.c_str());
                }

                // Process materials first
                if (root.has_child("materials") && root["materials"].is_seq())
                {
                    if (!ProcessMaterials(root["materials"]))
                    {
                        CCPrint(PrintManager::CHANNEL_WARN, "SceneLoader: Some materials failed to load");
                    }
                }

                // Post-process stack before objects: it is frame-wide state
                // rather than anything an object refers to, so nothing in the
                // object pass depends on the order.
                if (root.has_child("postProcess") && root["postProcess"].is_map())
                {
                    ProcessPostProcess(root["postProcess"]);
                }

                // Process objects
                if (root.has_child("objects") && root["objects"].is_seq())
                {
                    for (ryml::ConstNodeRef objNode : root["objects"].children())
                    {
                        SceneObject* sceneObj = ProcessObject(objNode);
                        if (sceneObj != nullptr)
                        {
                            hierarchy.AddRootObject(sceneObj);
                        }
                    }
                }

                succeeded = true;
            }
        }

        return succeeded;
    }

    void SceneLoader::ProcessPostProcess(ryml::ConstNodeRef postProcessNode)
    {
        PostProcess* postProcess = RenderManager::Get()->GetPostProcess();

        // The block is authoritative for the scene: it loads onto stack
        // defaults, so a scene cannot inherit an effect left enabled by
        // whatever ran before it. Defaults rather than all-off, because the
        // tone curve is part of the output transform and a scene naming only
        // a vignette should not lose it and start clipping.
        postProcess->ResetEffectsToDefaults();

        if (postProcessNode.has_child("preset"))
        {
            std::string presetName = NodeToString(postProcessNode["preset"]);
            if (!postProcess->ApplyPreset(presetName.c_str()))
            {
                CCPrint(PrintManager::CHANNEL_WARN,
                    "SceneLoader: Unknown post-process preset '%s'", presetName.c_str());
            }
        }

        for (ryml::ConstNodeRef effectNode : postProcessNode.children())
        {
            std::string effectName = KeyToString(effectNode);

            if (effectName != "preset")
            {
                PostProcessEffect* effect = postProcess->FindEffect(effectName.c_str());

                if (effect == nullptr)
                {
                    CCPrint(PrintManager::CHANNEL_WARN,
                        "SceneLoader: Unknown post-process effect '%s'", effectName.c_str());
                }
                else
                {
                    for (ryml::ConstNodeRef paramNode : effectNode.children())
                    {
                        std::string paramName = KeyToString(paramNode);

                        if (paramName == "enabled")
                        {
                            effect->SetEnabled(NodeToString(paramNode) == "true");
                        }
                        else if (!effect->SetParamValue(paramName.c_str(), NodeToFloat(paramNode)))
                        {
                            CCPrint(PrintManager::CHANNEL_WARN,
                                "SceneLoader: Unknown parameter '%s' on post-process effect '%s'",
                                paramName.c_str(), effectName.c_str());
                        }
                    }
                }
            }
        }
    }

    bool SceneLoader::ProcessMaterials(ryml::ConstNodeRef materials)
    {
        bool allSucceeded = true;
        MaterialManager* matManager = MaterialManager::Get();

        for (ryml::ConstNodeRef matNode : materials.children())
        {
            if (!matNode.has_child("name") || !matNode.has_child("shader"))
            {
                CCPrint(PrintManager::CHANNEL_WARN, "SceneLoader: Material missing name or shader");
                allSucceeded = false;
                continue;
            }

            std::string name = NodeToString(matNode["name"]);
            std::string shader = NodeToString(matNode["shader"]);
            std::string texture = "";
            Vector3 baseColor(1.0f, 1.0f, 1.0f);
            Vector2 tiling(1.0f, 1.0f);

            if (matNode.has_child("texture"))
            {
                texture = NodeToString(matNode["texture"]);
            }

            if (matNode.has_child("baseColor"))
            {
                ryml::ConstNodeRef colorNode = matNode["baseColor"];
                if (colorNode.num_children() >= 3)
                {
                    baseColor.x = NodeToFloat(colorNode[0]);
                    baseColor.y = NodeToFloat(colorNode[1]);
                    baseColor.z = NodeToFloat(colorNode[2]);
                }
            }

            if (matNode.has_child("tiling"))
            {
                ryml::ConstNodeRef tilingNode = matNode["tiling"];
                if (tilingNode.num_children() >= 2)
                {
                    tiling.x = NodeToFloat(tilingNode[0]);
                    tiling.y = NodeToFloat(tilingNode[1]);
                }
            }

            float opacity = 1.0f;
            if (matNode.has_child("opacity"))
            {
                opacity = NodeToFloat(matNode["opacity"]);
            }

            // Light the surface emits, in scene-linear units. Written either
            // as [r, g, b] or as a single number for a white emission, where
            // a value above 1.0 puts the surface above display white.
            bool hasEmissive = false;
            Vector3 emissive(0.0f, 0.0f, 0.0f);
            if (matNode.has_child("emissive"))
            {
                ryml::ConstNodeRef emissiveNode = matNode["emissive"];
                hasEmissive = true;

                if (emissiveNode.num_children() >= 3)
                {
                    emissive.x = NodeToFloat(emissiveNode[0]);
                    emissive.y = NodeToFloat(emissiveNode[1]);
                    emissive.z = NodeToFloat(emissiveNode[2]);
                }
                else
                {
                    float intensity = NodeToFloat(emissiveNode);
                    emissive = Vector3(intensity, intensity, intensity);
                }
            }

            if (matManager->HasMaterial(name))
            {
                matManager->RemoveMaterial(name);
                CCPrint(PrintManager::CHANNEL_ALWAYS, "SceneLoader: Recreating material '%s'", name.c_str());
            }
            else
            {
                CCPrint(PrintManager::CHANNEL_ALWAYS, "SceneLoader: Created material '%s'", name.c_str());
            }
            matManager->CreateMaterial(name, shader, texture, baseColor, tiling, opacity);

            if (hasEmissive)
            {
                Material* material = matManager->GetMaterial(name);
                if (material != nullptr)
                {
                    material->SetEmissive(emissive);
                }
            }
        }

        return allSucceeded;
    }

    SceneObject* SceneLoader::ProcessObject(ryml::ConstNodeRef objNode)
    {
        // Get name
        std::string name = "";
        if (objNode.has_child("name"))
        {
            name = NodeToString(objNode["name"]);
        }

        SceneObject* sceneObject = new SceneObject(name);

        // Process transform
        if (objNode.has_child("transform"))
        {
            ryml::ConstNodeRef transNode = objNode["transform"];

            if (transNode.has_child("position"))
            {
                ryml::ConstNodeRef posNode = transNode["position"];
                if (posNode.num_children() >= 3)
                {
                    Vector3 pos(
                        NodeToFloat(posNode[0]),
                        NodeToFloat(posNode[1]),
                        NodeToFloat(posNode[2])
                    );
                    sceneObject->GetTransform().SetPosition(pos);
                }
            }

            if (transNode.has_child("rotation"))
            {
                ryml::ConstNodeRef rotNode = transNode["rotation"];
                if (rotNode.num_children() >= 3)
                {
                    Vector3 rot(
                        NodeToFloat(rotNode[0]),
                        NodeToFloat(rotNode[1]),
                        NodeToFloat(rotNode[2])
                    );
                    sceneObject->GetTransform().SetRotation(rot);
                }
            }

            if (transNode.has_child("scale"))
            {
                ryml::ConstNodeRef scaleNode = transNode["scale"];
                if (scaleNode.num_children() >= 3)
                {
                    Vector3 scale(
                        NodeToFloat(scaleNode[0]),
                        NodeToFloat(scaleNode[1]),
                        NodeToFloat(scaleNode[2])
                    );
                    sceneObject->GetTransform().SetScale(scale);
                }
            }
        }

        // Process components
        if (objNode.has_child("components") && objNode["components"].is_seq())
        {
            for (ryml::ConstNodeRef compNode : objNode["components"].children())
            {
                if (!compNode.has_child("type"))
                {
                    continue;
                }

                std::string type = NodeToString(compNode["type"]);
                Component* component = ComponentFactory::Get()->CreateComponent(type, compNode);
                if (component != nullptr)
                {
                    sceneObject->AddComponent(component);
                }
            }
        }

        // Process glTF model (loads as child hierarchy)
        if (objNode.has_child("gltf"))
        {
            std::string gltfPath = NodeToString(objNode["gltf"]);
            SceneObject* gltfHierarchy = GltfLoader::LoadGltf(gltfPath);
            if (gltfHierarchy != nullptr)
            {
                // Copy children before iterating to avoid iterator invalidation
                // (RemoveChild modifies the internal vector during iteration)
                std::vector<SceneObject*> gltfChildren(gltfHierarchy->GetChildren());
                for (SceneObject* gltfChild : gltfChildren)
                {
                    gltfHierarchy->RemoveChild(gltfChild);
                    sceneObject->AddChild(gltfChild);
                }
                delete gltfHierarchy;

                CCPrint(PrintManager::CHANNEL_ALWAYS, "SceneLoader: Loaded glTF '%s' as children of '%s'",
                    gltfPath.c_str(), name.c_str());
            }
            else
            {
                CCPrint(PrintManager::CHANNEL_WARN, "SceneLoader: Failed to load glTF '%s'", gltfPath.c_str());
            }
        }

        // Process children recursively
        if (objNode.has_child("children") && objNode["children"].is_seq())
        {
            for (ryml::ConstNodeRef childNode : objNode["children"].children())
            {
                SceneObject* child = ProcessObject(childNode);
                if (child != nullptr)
                {
                    sceneObject->AddChild(child);
                }
            }
        }

        return sceneObject;
    }
}
