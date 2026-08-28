#ifndef GLTFLOADER_H
#define GLTFLOADER_H

#include <string>
#include <vector>
#include "RenderableMesh.h"
#include "Material.h"
#include "CCVector3.h"

struct cgltf_node;
struct cgltf_data;

namespace CC
{
    class SceneObject;

    class GltfLoader
    {
    public:
        // Returns SceneObject hierarchy containing all meshes/primitives.
        // Each primitive becomes a child SceneObject with RenderableMesh component.
        static SceneObject* LoadGltf(const std::string& filePath);

        // Load single mesh with explicit material (for backwards compatibility).
        static RenderableMesh* LoadGltfMesh(const std::string& filePath, Material* material);

        // Load single mesh with auto-created PBR material from glTF data.
        static RenderableMesh* LoadGltfMesh(const std::string& filePath);

    private:
        static SceneObject* ProcessNode(cgltf_node* node, cgltf_data* data,
            const std::string& baseName, const std::string& directory, int& meshIndex);

        struct PbrMaterialData
        {
            std::string name;
            Vector3 baseColorFactor = Vector3(1.0f, 1.0f, 1.0f);
            float metallicFactor = 1.0f;
            float roughnessFactor = 1.0f;
            Vector3 emissiveFactor = Vector3(0.0f, 0.0f, 0.0f);
            float opacity = 1.0f;
            AlphaBlendMode alphaMode = AlphaBlendMode::Opaque;
            float alphaCutoff = 0.5f;
            std::string baseColorTexture;
            std::string metallicRoughnessTexture;
            std::string normalTexture;
            std::string occlusionTexture;
            std::string emissiveTexture;
        };

    };
}

#endif // GLTFLOADER_H
